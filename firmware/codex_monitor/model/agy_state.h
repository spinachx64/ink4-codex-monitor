#pragma once
#include "quota_state.h"

namespace quota {

struct AgyStore {
    Window gemini[2];    // [0] = 5H, [1] = Weekly
    Window external[2];  // [0] = 5H, [1] = Weekly (Claude/GPT)
    int64_t sequence = 0;
    int64_t lastApiReset[2] = {0, 0};

    bool apply(const char* line, size_t length, int64_t receivedUs, bool resetConfirmed[2]);
};

}  // namespace quota
