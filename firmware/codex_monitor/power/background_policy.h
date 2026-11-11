#pragma once
#include <cstdint>

namespace codex_power {
// Status 0 is no input, 7 is OTG, 6 is reserved in the vendor map.
inline bool charger_input_usable(bool reads_ok, uint8_t vbus, bool otg,
                                 bool charge_enabled, bool hiz) {
    return reads_ok && vbus >= 1 && vbus <= 5 && !otg && charge_enabled && !hiz;
}
inline bool should_recharge(bool input_usable, bool current_ok, int current_ma,
                            bool idle, int voltage_mv, unsigned target_mv,
                            int64_t now_us, int64_t last_attempt_us) {
    return input_usable && current_ok && current_ma >= -5 && current_ma < 5 &&
        idle && voltage_mv >= 2500 && static_cast<unsigned>(voltage_mv) < target_mv &&
        (last_attempt_us == 0 || now_us - last_attempt_us >= 60000000);
}
inline unsigned monitor_period_ms(bool idle) { return idle ? 60000 : 3000; }
inline unsigned input_cache_ms(bool idle, bool irq_asserted, bool volume_pressed) {
    return idle && !irq_asserted && !volume_pressed ? 1000 : 20;
}
}
