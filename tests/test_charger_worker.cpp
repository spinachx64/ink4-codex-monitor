// Exercise the actual charger worker with fake registers, including input loss
// and user commands during a recharge pulse. No ESP-IDF hardware is needed.
#include "power/background_policy.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>

using esp_err_t = int;
constexpr int ESP_OK=0, portMAX_DELAY=0;
constexpr int REG_STATUS1=0x1e, REG_CHG_CTRL3=0x18, REG_CHG_CTRL1=0x16, REG_CHG_CTRL0=0x14;
constexpr unsigned CX25601N_VREG_MIN_MV=3500, CX25601N_VREG_MAX_MV=4500;
constexpr int CX25601N_CHG_STAT_NOT=0, kUvBatfetShutdownMv=3100;
constexpr unsigned pdMS_TO_TICKS(unsigned ms) { return ms; }
template<class... Args> void fake_log(Args...) {}
#define ESP_LOGI(...) fake_log(__VA_ARGS__)
#define ESP_LOGW(...) fake_log(__VA_ARGS__)
constexpr const char* TAG="charger-test";
int s_lock=0;
unsigned s_vreg_vol_mv=4400;
bool s_uv_shutdown_done=false;
uint8_t regs[256]{};
int voltage=4100, current=-38, writes=0, loop_count=0, shutdowns=0;
int failed_reg=-1;
bool current_ok=true;
int64_t now_us=1000000;
enum class PulseEvent { None, Unplug, DisableCharge, EnableOtg };
PulseEvent pulse_event=PulseEvent::None;
extern uint32_t s_charge_command_revision;
struct Stop {};
void vTaskDelay(unsigned ms);
void xSemaphoreTake(int,int) {}
void xSemaphoreGive(int) {}
int64_t esp_timer_get_time() { return now_us; }
const char* esp_err_to_name(int) { return "fake"; }
int battery_get_bat_voltage() { return voltage; }
esp_err_t read_byte(uint8_t reg,uint8_t* out) {
    if(reg==failed_reg) return -1;
    *out=regs[reg];return ESP_OK;
}
esp_err_t update_bits(uint8_t reg,uint8_t mask,uint8_t shift,uint8_t field) {
    ++writes;
    regs[reg]=(regs[reg]&~(mask<<shift)) | ((field&mask)<<shift);
    return ESP_OK;
}
void set_en_term(bool enabled) { update_bits(REG_CHG_CTRL0,1,2,enabled); }
void apply_vreg_fixed(unsigned) {}
esp_err_t cx25601n_get_vbus_stat(uint8_t* out) {
    uint8_t status;
    if(read_byte(REG_STATUS1,&status)!=ESP_OK) return -1;
    *out=status&7;return ESP_OK;
}
esp_err_t cx25601n_get_chrg_stat(uint8_t* out) {
    uint8_t status;
    if(read_byte(REG_STATUS1,&status)!=ESP_OK) return -1;
    *out=(status>>3)&3;return ESP_OK;
}
esp_err_t cx25601n_enter_batfet_shutdown() { ++shutdowns;return ESP_OK; }
struct Bq27220Gauge {
    static Bq27220Gauge& GetInstance() { static Bq27220Gauge g;return g; }
    bool ReadCurrentMa(int16_t& out) { out=current;return current_ok; }
};
#include "power/charger_vreg_task.inc"
void vTaskDelay(unsigned ms) {
    now_us+=static_cast<int64_t>(ms)*1000;
    if(ms==200) {
        if(pulse_event==PulseEvent::Unplug) regs[REG_STATUS1]=0;
        if(pulse_event==PulseEvent::DisableCharge) {
            ++s_charge_command_revision;regs[REG_CHG_CTRL1]&=~(1<<5);
        }
        if(pulse_event==PulseEvent::EnableOtg) {
            ++s_charge_command_revision;regs[REG_CHG_CTRL3]|=1<<6;
        }
    } else if(++loop_count==4) throw Stop{};
}
void reset(uint8_t vbus=0) {
    for(auto& r:regs) r=0;
    regs[REG_STATUS1]=vbus;regs[REG_CHG_CTRL1]=1<<5;
    voltage=4100;current=-38;writes=loop_count=shutdowns=0;
    current_ok=true;failed_reg=-1;now_us=1000000;
    s_charge_command_revision=0;s_uv_shutdown_done=false;pulse_event=PulseEvent::None;
}
void run() { try { charger_vreg_task(nullptr); } catch(const Stop&) {} }
int main() {
    reset();run();assert(writes==0 && shutdowns==0); // battery: no writes/recharge
    reset(7);run();assert(writes==0); // OTG VBUS
    reset(1);regs[REG_CHG_CTRL3]=1<<6;run();assert(writes==0);
    reset(1);regs[REG_CHG_CTRL1]=0;run();assert(writes==0); // user disabled charge
    reset(1);regs[REG_CHG_CTRL1]|=1<<4;run();assert(writes==0); // HIZ
    for(int reg:{REG_STATUS1,REG_CHG_CTRL3,REG_CHG_CTRL1}) {
        reset(1);failed_reg=reg;run();assert(writes==0); // fail closed
    }
    reset(1);current=0;run();assert(writes==7); // 4 EN_TERM + one pulse (3 writes), cooldown
    assert(regs[REG_CHG_CTRL1]&(1<<5));
    reset(1);current=-38;run();assert(writes==4); // signed discharge is not zero charge
    reset(1);current_ok=false;run();assert(writes==4); // no pulse on missing gauge
    reset();voltage=3050;run();assert(shutdowns==1); // UV stays active offline
    reset();voltage=3050;failed_reg=REG_STATUS1;run();assert(shutdowns==0);
    reset(1);voltage=3050;current=100;run();assert(shutdowns==0);
    for(auto event:{PulseEvent::Unplug,PulseEvent::DisableCharge,PulseEvent::EnableOtg}) {
        reset(1);pulse_event=event;assert(kick_recharge(4100,4400));
        if(event==PulseEvent::DisableCharge) assert(!(regs[REG_CHG_CTRL1]&(1<<5)));
        else if(event==PulseEvent::EnableOtg) assert(!(regs[REG_CHG_CTRL1]&(1<<5)));
        else assert(regs[REG_CHG_CTRL1]&(1<<5)); // next plug-in can still charge
    }
    using codex_power::should_recharge;
    assert(!should_recharge(true,true,0,true,4100,4400,60999999,1000000));
    assert(should_recharge(true,true,0,true,4100,4400,61000000,1000000));
    using codex_power::input_cache_ms;
    assert(input_cache_ms(true,false,false)==1000);
    assert(input_cache_ms(true,true,false)==20); // interrupt restores sampling
    assert(input_cache_ms(true,false,true)==20); // held key/release remain responsive
    assert(input_cache_ms(false,false,false)==20); // normal apps unchanged
    assert(codex_power::monitor_period_ms(true)==60000);
    assert(codex_power::monitor_period_ms(false)==3000);
}
