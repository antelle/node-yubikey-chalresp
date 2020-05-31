#include <napi.h>
#include <uv.h>

#include <yubikey.h>
#include <ykdef.h>
#include <ykcore.h>
#include <ykstatus.h>

#include <set>
#include <vector>

#include "common.h"
#include "challenge-response.h"

uv_mutex_t chalRespKeyMutex;
YK_KEY* chalRespKey = nullptr;

class ChallengeResponseThreadData {
    public:
        int vid;
        int pid;
        unsigned int serial;
        int slot;
        std::vector<unsigned char> challenge;
        Napi::ThreadSafeFunction threadSafeCallback;

    public:
        void errorCallback(YubiKeyError ykError) {
            threadSafeCallback.BlockingCall(this, [ykError]
                (Napi::Env env, Napi::Function jsCallback, ChallengeResponseThreadData* threadData) {
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

        void touchCallback() {
            threadSafeCallback.BlockingCall([]
                (Napi::Env env, Napi::Function jsCallback) {
                    auto err = Napi::Error::New(env, "Touch requested").Value();
                    err.Set("touchRequested", true);
                    jsCallback.Call({ err });
                });
        }

        void successCallback(std::vector<unsigned char> response) {
            threadSafeCallback.BlockingCall(this, [response]
                (Napi::Env env, Napi::Function jsCallback, ChallengeResponseThreadData* threadData) {
                    auto buffer = Napi::Buffer<unsigned char>::Copy(env, response.data(), response.size());
                    jsCallback.Call({ env.Undefined(), buffer });
                    delete threadData;
                });
            threadSafeCallback.Release();
            releaseAddonLock();
        }
};

YK_KEY* openYubiKey(int vid, int pid, unsigned int serial) {
    const int pids[] = { pid };

    for (int index = 0; index < MAX_YUBIKEYS; index++) {
        for (int attempt = 0; attempt < YUBIKEY_OPEN_ATTEMPTS; attempt++) {
            // why? see here: https://github.com/Yubico/yubikey-personalization/issues/161
            auto yk = yk_open_key_vid_pid(vid, pids, 1, index);
            if (!yk) {
                return nullptr;
            }
            unsigned int ykSerial;
            if (!yk_get_serial(yk, 1, 0, &ykSerial)) {
                yk_close_key(yk);
                return nullptr;
            }
            if (ykSerial == serial) {
                return yk;
            }
            yk_close_key(yk);
        }
    }

    return nullptr;
}

bool initChallengeResponse() {
    return !uv_mutex_init(&chalRespKeyMutex);
}

void setChalRespKey(YK_KEY* yk) {
    uv_mutex_lock(&chalRespKeyMutex);
    chalRespKey = yk;
    uv_mutex_unlock(&chalRespKeyMutex);
}

void challengeResponse(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 4) {
        Napi::RangeError::New(env, "Not enough arguments").ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsObject()) {
        Napi::TypeError::New(env, "YubiKey is not an object").ThrowAsJavaScriptException();
        return;
    }

    auto yubiKeyArg = info[0].ToObject();

    auto serialProp = yubiKeyArg.Get("serial");
    auto vidProp = yubiKeyArg.Get("vid");
    auto pidProp = yubiKeyArg.Get("pid");

    if (!serialProp.IsNumber()) {
        Napi::TypeError::New(env, "yubiKey.serial not a number").ThrowAsJavaScriptException();
        return;
    }
    if (!vidProp.IsNumber()) {
        Napi::TypeError::New(env, "yubiKey.vid not a number").ThrowAsJavaScriptException();
        return;
    }
    if (!pidProp.IsNumber()) {
        Napi::TypeError::New(env, "yubiKey.pid not a number").ThrowAsJavaScriptException();
        return;
    }

    auto serial = serialProp.ToNumber().Uint32Value();
    auto vid = vidProp.ToNumber().Int32Value();
    auto pid = pidProp.ToNumber().Int32Value();

    if (!info[1].IsBuffer()) {
        Napi::TypeError::New(env, "Challenge is not a buffer").ThrowAsJavaScriptException();
        return;
    }

    auto buffer = info[1].As<Napi::Buffer<unsigned char>>();
    auto challenge = std::vector<unsigned char>(buffer.Data(), buffer.Data() + buffer.Length());

    if (!info[2].IsNumber()) {
        Napi::TypeError::New(env, "Slot is not a number").ThrowAsJavaScriptException();
        return;
    }

    auto slot = info[2].ToNumber().Int32Value();
    if (slot != 1 && slot != 2) {
        Napi::RangeError::New(env, "Slot must be 1 or 2").ThrowAsJavaScriptException();
        return;
    }

    if (!info[3].IsFunction()) {
        Napi::TypeError::New(env, "Callback is not a function").ThrowAsJavaScriptException();
        return;
    }

    auto callback = info[3].As<Napi::Function>();

    auto threadData = new ChallengeResponseThreadData();
    threadData->threadSafeCallback = Napi::ThreadSafeFunction::New(env, callback, "callback", 0, 1);
    threadData->vid = vid;
    threadData->pid = pid;
    threadData->serial = serial;
    threadData->slot = slot;
    threadData->challenge = challenge;

    uv_thread_t tid;
    uv_thread_create(&tid, [](void* data) {
        acquireAddonLock();

        auto threadData = (ChallengeResponseThreadData*)data;

        auto yk = openYubiKey(threadData->vid, threadData->pid, threadData->serial);
        if (!yk) {
            threadData->errorCallback(getYubiKeyError("yk_open_key_vid_pid"));
            return;
        }

        std::vector<unsigned char> response(SHA1_MAX_BLOCK_SIZE);

        auto cmd = threadData->slot == 1 ? SLOT_CHAL_HMAC1 : SLOT_CHAL_HMAC2;

        if (!yk_challenge_response(yk, cmd, false,
            threadData->challenge.size(), threadData->challenge.data(),
            response.size(), response.data())
        ) {
            if (yk_errno == YK_EWOULDBLOCK) {
                threadData->touchCallback();
                setChalRespKey(yk);

                if (!yk_challenge_response(yk, cmd, true,
                    threadData->challenge.size(), threadData->challenge.data(),
                    response.size(), response.data())
                ) {
                    setChalRespKey(nullptr);
                    yk_close_key(yk);
                    threadData->errorCallback(getYubiKeyError("yk_challenge_response"));
                    return;
                }
                setChalRespKey(nullptr);
            } else {
                yk_close_key(yk);
                threadData->errorCallback(getYubiKeyError("yk_challenge_response"));
                return;
            }
        }

        yk_close_key(yk);

        response = std::vector<unsigned char>(response.begin(), response.begin() + SHA1_DIGEST_SIZE);
        threadData->successCallback(response);
    }, threadData);
}

void cancelChallengeResponse(const Napi::CallbackInfo& info) {
    if (!chalRespKey) {
        return;
    }

    uv_thread_t tid;
    uv_thread_create(&tid, [](void* data) {
        uv_mutex_lock(&chalRespKeyMutex);
        if (chalRespKey) {
            yk_force_key_update(chalRespKey);
        }
        uv_mutex_unlock(&chalRespKeyMutex);
    }, nullptr);
}
