#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <vector>
#include <algorithm>

constexpr int pdTRUE = 1, ESP_OK = 0;
constexpr unsigned portMAX_DELAY = ~0U;
enum { UART_DATA, UART_FIFO_OVF, UART_BUFFER_FULL, UART_PARITY_ERR };
struct uart_event_t { int type; size_t size; };
struct Stop {};
std::deque<uart_event_t> events;
std::deque<uint8_t> received;
std::vector<uint8_t> delivered;
int waits = 0, reads = 0, flushes = 0, resets = 0;
int xQueueReceive(int, uart_event_t* event, unsigned timeout) {
    assert(timeout == portMAX_DELAY); // No periodic idle timeout.
    ++waits;
    if (events.empty()) throw Stop{};
    *event = events.front(); events.pop_front(); return pdTRUE;
}
int uart_read_bytes(int, void* buf, size_t n, unsigned timeout) {
    assert(timeout == 0);
    ++reads;
    n = std::min(n, received.size());
    for (size_t i = 0; i < n; ++i) {
        static_cast<uint8_t*>(buf)[i] = received.front(); received.pop_front();
    }
    return static_cast<int>(n);
}
void uart_flush_input(int) { ++flushes; received.clear(); }
void xQueueReset(int) { ++resets; events.clear(); }
class SimpleUart {
public:
    int m_uartQueue = 1, m_uartNum = 2;
    std::function<void(const std::vector<uint8_t>&)> m_callback;
    static constexpr size_t RX_BUFFER_SIZE = 1024;
#include "power/uart_receive_task.inc"
};

struct Timer { bool paused = false; int ready = 0; };
struct Input { Timer timer; bool has_timer = true; int notifications = 0; };
struct Snapshot { unsigned live_count = 0, pending_down = 0, pending_up = 0; bool lvgl_down = false; };
Snapshot s_touch_snap;
int s_touch_snap_mux = 0;
bool s_codex_input_idle = false, s_touch_irq_ready = true;
int lock_depth = 0, wake_requests = 0;
void portENTER_CRITICAL(int*) {}
void portEXIT_CRITICAL(int*) {}
Timer* lv_indev_get_read_timer(Input* input) { return input->has_timer ? &input->timer : nullptr; }
void lv_timer_pause(Timer* timer) { timer->paused = true; }
bool lv_timer_get_paused(Timer* timer) { return timer->paused; }
void lv_timer_resume(Timer* timer) { timer->paused = false; }
void lv_timer_ready(Timer* timer) { ++timer->ready; }
int esp_lv_adapter_lock(int) { ++lock_depth; return ESP_OK; }
void esp_lv_adapter_unlock() { --lock_depth; }
void esp_lv_adapter_touch_notify_interrupt(Input* input) { assert(lock_depth > 0); ++input->notifications; }
void esp_lv_adapter_request_wake() { assert(lock_depth == 0); ++wake_requests; }
class LVAdapterDisplay {
public:
    Input* touch_indev_ = nullptr;
    void SetCodexInputIdle(bool idle);
    void KickTouchInput();
};
#include "power/touch_input_idle.inc"

void run_uart(SimpleUart& uart) {
    try { SimpleUart::uartReceiveTask(&uart); assert(false); } catch (const Stop&) {}
}
void reset_uart() {
    events.clear(); received.clear(); delivered.clear(); waits = reads = flushes = resets = 0;
}
int main() {
    SimpleUart uart;
    uart.m_callback = [](const auto& bytes) { delivered.insert(delivered.end(), bytes.begin(), bytes.end()); };
    reset_uart(); run_uart(uart);
    assert(waits == 1 && reads == 0); // Empty queue blocks, never reads UART.
    reset_uart();
    for (unsigned i = 0; i < 2500; ++i) received.push_back(i % 251);
    events = {{UART_DATA, 1300}, {UART_PARITY_ERR, 0}, {UART_DATA, 1200}};
    run_uart(uart);
    assert(delivered.size() == 2500 && received.empty() && reads == 4);
    for (unsigned i = 0; i < delivered.size(); ++i) assert(delivered[i] == i % 251);
    for (int fault : {UART_FIFO_OVF, UART_BUFFER_FULL}) {
        reset_uart(); received = {1, 2}; events = {{fault, 0}, {UART_DATA, 2}};
        run_uart(uart);
        assert(flushes == 1 && resets == 1 && received.empty() && delivered.empty());
    }
    reset_uart(); events = {{UART_DATA, 20}}; received = {1, 2}; run_uart(uart);
    assert(delivered.size() == 2 && reads == 2); // Short reads cannot spin forever.
    reset_uart(); uart.m_callback = nullptr; events = {{UART_DATA, 2}}; received = {1, 2}; run_uart(uart);
    assert(received.empty());

    Input input;
    LVAdapterDisplay display; display.touch_indev_ = &input;
    display.SetCodexInputIdle(true); assert(input.timer.paused);
    s_touch_snap.pending_down = 1;
    display.KickTouchInput();
    assert(!input.timer.paused && input.timer.ready == 1 && input.notifications == 1 && wake_requests == 1);
    display.SetCodexInputIdle(true); assert(!input.timer.paused); // Pending short tap remains deliverable.
    s_touch_snap = {}; s_touch_snap.live_count = 1;
    display.SetCodexInputIdle(true); assert(!input.timer.paused); // Hold/gesture polling retained.
    s_touch_snap = {}; s_touch_snap.lvgl_down = true;
    display.SetCodexInputIdle(true); assert(!input.timer.paused);
    s_touch_snap = {}; s_touch_snap.pending_up = 1;
    display.SetCodexInputIdle(true); assert(!input.timer.paused); // Release must be consumed before parking.
    s_touch_snap = {};
    display.SetCodexInputIdle(true); assert(input.timer.paused);
    display.SetCodexInputIdle(false); assert(!input.timer.paused); // Page exit restores normal input.
    s_touch_irq_ready = false;
    display.SetCodexInputIdle(true); assert(!input.timer.paused); // Missing IRQ keeps fallback polling.
    s_touch_irq_ready = true;
    display.SetCodexInputIdle(true); assert(input.timer.paused);
    // Edge arrives between parking and the feed's kick: it still resumes.
    s_touch_snap.pending_down = 1; s_touch_snap.pending_up = 1;
    display.KickTouchInput(); assert(!input.timer.paused);
    display.touch_indev_ = nullptr; display.SetCodexInputIdle(true); display.KickTouchInput();
    assert(lock_depth == 0);
}
