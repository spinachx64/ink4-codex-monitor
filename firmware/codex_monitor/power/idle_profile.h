#pragma once
#include <cstdint>

namespace codex_power {
// Only a visible Codex page in AppIdle may opt in. AppIdle already excludes
// audio, OTA, provisioning and network consumers; USB/standby inhibition also veto it.
inline bool allow_auto_sleep(bool codex, bool app_idle, bool inhibited, int64_t inactive_us) {
    return codex && app_idle && !inhibited && inactive_us >= 5000000;
}
}
