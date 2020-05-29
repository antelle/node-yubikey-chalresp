#include <uv.h>

#include <string>

#include <yubikey.h>
#include <ykdef.h>
#include <ykcore.h>
#include <ykstatus.h>
#include <ykpers-version.h>

#include "common.h"

uv_mutex_t mutex;

bool initAddonLock() {
    return !uv_mutex_init(&mutex);
}

void acquireAddonLock() {
    uv_mutex_lock(&mutex);
}

void releaseAddonLock() {
    uv_mutex_unlock(&mutex);
}

std::string getYubiKeyError(std::string location) {
    constexpr size_t SIZE = 1024;
    char message[SIZE];

    if (yk_errno) {
        if (yk_errno == YK_EUSBERR) {
            snprintf(message, SIZE, "USB error in %s: %s", location.c_str(), yk_usb_strerror());
        } else {
            snprintf(message, SIZE, "Yubikey core error in %s: %s", location.c_str(), yk_strerror(yk_errno));
        }
    } else {
        snprintf(message, SIZE, "Unexpected YubiKey API error in %s", location.c_str());
    }

    return message;
}
