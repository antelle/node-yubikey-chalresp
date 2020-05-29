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

    exports.Set("YK_EUSBERR", Napi::Number::New(env, YK_EUSBERR));
    exports.Set("YK_EWRONGSIZ", Napi::Number::New(env, YK_EWRONGSIZ));
    exports.Set("YK_EWRITEERR", Napi::Number::New(env, YK_EWRITEERR));
    exports.Set("YK_ETIMEOUT", Napi::Number::New(env, YK_ETIMEOUT));
    exports.Set("YK_ENOKEY", Napi::Number::New(env, YK_ENOKEY));
    exports.Set("YK_EFIRMWARE", Napi::Number::New(env, YK_EFIRMWARE));
    exports.Set("YK_ENOMEM", Napi::Number::New(env, YK_ENOMEM));
    exports.Set("YK_ENOSTATUS", Napi::Number::New(env, YK_ENOSTATUS));
    exports.Set("YK_ENOTYETIMPL", Napi::Number::New(env, YK_ENOTYETIMPL));
    exports.Set("YK_ECHECKSUM", Napi::Number::New(env, YK_ECHECKSUM));
    exports.Set("YK_EWOULDBLOCK", Napi::Number::New(env, YK_EWOULDBLOCK));
    exports.Set("YK_EINVALIDCMD", Napi::Number::New(env, YK_EINVALIDCMD));
    exports.Set("YK_EMORETHANONE", Napi::Number::New(env, YK_EMORETHANONE));
    exports.Set("YK_ENODATA", Napi::Number::New(env, YK_ENODATA));

    return exports;
}

NODE_API_MODULE(yubikey_otp, init)
