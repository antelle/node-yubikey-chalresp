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

struct YubiKeySlot {
    int number;
    bool valid;
    bool touch;
};

struct YubiKeyInfo {
    int vid;
    int pid;
    unsigned int serial;
    std::string version;
    std::vector<YubiKeySlot> slots;
    std::vector<unsigned char> capabilities;
};

class GetYubiKeysThreadData {
    public:
        Napi::ThreadSafeFunction threadSafeCallback;
        bool getCapabilities;
        bool testSlots;

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

                        auto slotsArray = Napi::Array::New(env);

                        ykObj.Set("serial", Napi::Number::New(env, yk.serial));
                        ykObj.Set("vid", Napi::Number::New(env, yk.vid));
                        ykObj.Set("pid", Napi::Number::New(env, yk.pid));
                        ykObj.Set("version", Napi::String::New(env, yk.version));
                        ykObj.Set("slots", slotsArray);

                        for (auto &slot: yk.slots) {
                            auto slotObj = Napi::Object::New(env);

                            slotObj.Set("number", Napi::Number::New(env, slot.number));
                            slotObj.Set("valid", Napi::Boolean::New(env, slot.valid));
                            slotObj.Set("touch", Napi::Boolean::New(env, slot.touch));

                            slotsArray.Set(slotsArray.Length(), slotObj);
                        }

                        if (!yk.capabilities.empty()) {
                            auto capBuffer = Napi::Buffer<unsigned char>::Copy(env,
                                yk.capabilities.data(), yk.capabilities.size());
                            ykObj.Set("capabilities", capBuffer);
                        }

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

    if (info.Length() < 2) {
        Napi::RangeError::New(env, "Not enough arguments").ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsObject()) {
        Napi::TypeError::New(env, "Options is not an object").ThrowAsJavaScriptException();
        return;
    }
    auto options = info[0].ToObject();

    if (!info[1].IsFunction()) {
        Napi::TypeError::New(env, "Callback is not a function").ThrowAsJavaScriptException();
        return;
    }

    auto callback = info[1].As<Napi::Function>();

    auto threadData = new GetYubiKeysThreadData();
    threadData->threadSafeCallback = Napi::ThreadSafeFunction::New(env, callback, "callback", 0, 1);
    threadData->getCapabilities = options.Get("getCapabilities").ToBoolean();
    threadData->testSlots = options.Get("testSlots").ToBoolean();

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
            auto vMajor = ykds_version_major(st);
            auto vMinor = ykds_version_minor(st);
            auto vBuild = ykds_version_build(st);

            snprintf(version, 32, "%d.%d.%d", vMajor, vMinor, vBuild);
            yubiKeyInfo.version = version;
            
            auto touchLevel = ykds_touch_level(st);

            for (int slotNum = 1; slotNum <= 2; slotNum++) {
                YubiKeySlot slot;

                slot.number = slotNum;

                auto slotConfigValid = slotNum == 1 ? CONFIG1_VALID : CONFIG2_VALID;
                slot.valid = (touchLevel & slotConfigValid) == slotConfigValid;

                auto slotConfigTouch = slotNum == 1 ? CONFIG1_TOUCH : CONFIG2_TOUCH;
                slot.touch = (touchLevel & slotConfigTouch) == slotConfigTouch;

                if (slot.valid) {
                    if (vMajor < 5) {
                        // those YubiKeys always require touch
                        slot.touch = true;
                    } else {
                        if (threadData->testSlots) {
                            auto cmd = slotNum == 1 ? SLOT_CHAL_HMAC1 : SLOT_CHAL_HMAC2;
                            unsigned char challenge[1];
                            unsigned char response[SHA1_MAX_BLOCK_SIZE];
                            if (yk_challenge_response(yk, cmd, false,
                                sizeof(challenge), challenge,
                                sizeof(response), response)
                            ) {
                                slot.touch = false;
                            } else {
                                if (yk_errno == YK_EWOULDBLOCK) {
                                    slot.touch = true;
                                } else {
                                    slot.valid = false;
                                }
                            }
                        }
                    }
                }

                yubiKeyInfo.slots.push_back(slot);
            }

            ykds_free(st);

            if (threadData->getCapabilities) {
                if (vMajor > 4 || (vMajor == 4 && vMinor >= 1)) {
                    unsigned char capData[0xff];
                    unsigned int capLen = 0xff;
                    if (yk_get_capabilities(yk, 1, 0, capData, &capLen)) {
                        yubiKeyInfo.capabilities = std::vector<unsigned char>(capData, capData + capLen);
                    }
                }
            }

            yk_close_key(yk);

            result.push_back(yubiKeyInfo);
        }

        threadData->errorCallback(getYubiKeyError("YubiKey enumeration error"));
    }, threadData);
}
