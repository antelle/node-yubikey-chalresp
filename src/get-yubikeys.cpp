#include <napi.h>
#include <uv.h>

#include <yubikey.h>
#include <ykdef.h>
#include <ykcore.h>
#include <ykstatus.h>

#include <set>
#include <vector>

#include "common.h"
#include "get-yubikeys.h"

struct YubiKeyInfo {
    int vid;
    int pid;
    unsigned int serial;
    std::string version;
    bool slot1;
    bool slot2;
};

class GetYubiKeysThreadData {
    public:
        Napi::ThreadSafeFunction threadSafeCallback;

    public:
        void errorCallback(YubiKeyError ykError) {
            threadSafeCallback.BlockingCall(this, [ykError]
                (Napi::Env env, Napi::Function jsCallback, GetYubiKeysThreadData* threadData) {
                    auto err = Napi::Error::New(env, ykError.message).Value();
                    if (ykError.code) {
                        err.Set("code", Napi::Number::New(env, ykError.code));
                    }
                    jsCallback.Call({ err });
                    delete threadData;
                });
            threadSafeCallback.Release();
            releaseAddonLock();
        }

        void successCallback(std::vector<YubiKeyInfo> result) {
            threadSafeCallback.BlockingCall(this, [result]
                (Napi::Env env, Napi::Function jsCallback, GetYubiKeysThreadData* threadData) {
                    auto yubiKeys = Napi::Array::New(env);

                    for (auto &yk: result) {
                        auto ykObj = Napi::Object::New(env);

                        ykObj.Set("serial", Napi::Number::New(env, yk.serial));
                        ykObj.Set("vid", Napi::Number::New(env, yk.vid));
                        ykObj.Set("pid", Napi::Number::New(env, yk.pid));
                        ykObj.Set("version", Napi::String::New(env, yk.version));
                        ykObj.Set("slot1", Napi::Boolean::New(env, yk.slot1));
                        ykObj.Set("slot2", Napi::Boolean::New(env, yk.slot2));

                        yubiKeys.Set(yubiKeys.Length(), ykObj);
                    }

                    jsCallback.Call({ env.Undefined(), yubiKeys });
                    delete threadData;
                });
            threadSafeCallback.Release();
            releaseAddonLock();
        }
};

void getYubiKeys(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 1) {
        Napi::RangeError::New(env, "Not enough arguments").ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsFunction()) {
        Napi::TypeError::New(env, "Callback is not a function").ThrowAsJavaScriptException();
        return;
    }

    auto callback = info[0].As<Napi::Function>();

    auto threadData = new GetYubiKeysThreadData();
    threadData->threadSafeCallback = Napi::ThreadSafeFunction::New(env, callback, "callback", 0, 1);

    uv_thread_t tid;
    uv_thread_create(&tid, [](void* data) {
        acquireAddonLock();

        auto threadData = (GetYubiKeysThreadData*)data;

        std::vector<YubiKeyInfo> result;
        std::set<unsigned int> serialMap;

        for (int i = 0; i < MAX_YUBIKEYS; i++) {
            YK_KEY *yk = nullptr;

            YubiKeyInfo yubiKeyInfo;

            // There's no way to get a list of connected YubiKeys, and the API can return a random one,
            // so we're doing it with a stupid retry, apparently that's how it's also solved in ykman.
            // The official tool also returns a random serial: `ykinfo -sn1`.
            // See https://github.com/Yubico/yubikey-personalization/issues/161
            for (int attempt = 0; attempt < YUBIKEY_OPEN_ATTEMPTS; attempt++) {
                if (!(yk = yk_open_key(i))) {
                    if (yk_errno == YK_ENOKEY) {
                        threadData->successCallback(result);
                        return;
                    }
                    threadData->errorCallback(getYubiKeyError("yk_open_key"));
                    return;
                }

                if (!yk_get_serial(yk, 1, 0, &yubiKeyInfo.serial)) {
                    yk_close_key(yk);
                    threadData->errorCallback(getYubiKeyError("yk_get_serial"));
                    return;
                }

                if (serialMap.find(yubiKeyInfo.serial) == serialMap.end()) {
                    break;
                }
            }

            if (serialMap.find(yubiKeyInfo.serial) != serialMap.end()) {
                yk_close_key(yk);
                threadData->errorCallback(getYubiKeyError("check_unique_serial"));
                return;
            }

            serialMap.insert(yubiKeyInfo.serial);

            if (!yk_get_key_vid_pid(yk, &yubiKeyInfo.vid, &yubiKeyInfo.pid)) {
                yk_close_key(yk);
                threadData->errorCallback(getYubiKeyError("yk_get_key_vid_pid"));
                return;
            }

            YK_STATUS *st = ykds_alloc();
            if (!yk_get_status(yk, st)) {
                ykds_free(st);
                yk_close_key(yk);
                threadData->errorCallback(getYubiKeyError("yk_get_status"));
                return;
            }

            char version[32];
            snprintf(version, 32, "%d.%d.%d",
                ykds_version_major(st), ykds_version_minor(st), ykds_version_build(st));
            yubiKeyInfo.version = version;
            
            auto touchLevel = ykds_touch_level(st);

            yubiKeyInfo.slot1 = (touchLevel & CONFIG1_VALID) == CONFIG1_VALID;
            yubiKeyInfo.slot2 = (touchLevel & CONFIG2_VALID) == CONFIG2_VALID;

            ykds_free(st);

            // // This times out, as well as `ykinfo -c`
            // unsigned char capData[0xff];
            // unsigned int capLen = 0xff;
            // if (!yk_get_capabilities(yk, 1, 0, capData, &capLen)) {
            //     throwYubiKeyError(env, "yk_get_capabilities");
            //     return result;
            // }

            yk_close_key(yk);

            result.push_back(yubiKeyInfo);
        }

        threadData->errorCallback(getYubiKeyError("YubiKey enumeration error"));
    }, threadData);
}
