#pragma once

#include <napi.h>

bool initChallengeResponse();
void challengeResponse(const Napi::CallbackInfo& info);
void cancelChallengeResponse(const Napi::CallbackInfo& info);
