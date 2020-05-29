#include <napi.h>

#include <ykcore.h>

#include "common.h"
#include "get-yubikeys.h"
#include "challenge-response.h"

Napi::Object init(Napi::Env env, Napi::Object exports) {
    if (!yk_init()) {
        Napi::Error::New(env, "YubiKey init failed").ThrowAsJavaScriptException();
        return exports;
    }

    if (!initAddonLock()) {
        Napi::Error::New(env, "Addon lock init failed").ThrowAsJavaScriptException();
        return exports;
    }

    exports.Set("version", Napi::String::New(env, ADDON_VERSION));

    exports.Set("getYubiKeys", Napi::Function::New(env, getYubiKeys));
    exports.Set("challengeResponse", Napi::Function::New(env, challengeResponse));

    return exports;
}

NODE_API_MODULE(yubikey_otp, init)
