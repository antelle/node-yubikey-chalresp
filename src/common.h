#pragma once

#include <string>

#define YUBIKEY_OPEN_ATTEMPTS 10
#define MAX_YUBIKEYS 0xff

bool initAddonLock();
void acquireAddonLock();
void releaseAddonLock();

std::string getYubiKeyError(std::string location);
