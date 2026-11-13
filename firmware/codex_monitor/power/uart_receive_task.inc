// Included inside SimpleUart. The UART driver owns/deletes its event queue.
    static void uartReceiveTask(void* arg) {
        auto* uart = static_cast<SimpleUart*>(arg);
        uint8_t buffer[RX_BUFFER_SIZE];
        uart_event_t event{};
        while (true) {
            if (xQueueReceive(uart->m_uartQueue, &event, portMAX_DELAY) != pdTRUE) continue;
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                uart_flush_input(uart->m_uartNum);
                xQueueReset(uart->m_uartQueue);
                continue;
            }
            if (event.type != UART_DATA) continue;
            size_t remaining = event.size;
            while (remaining) {
                const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
                const int length = uart_read_bytes(uart->m_uartNum, buffer, chunk, 0);
                if (length <= 0) break;
                remaining -= static_cast<size_t>(length);
                if (uart->m_callback) {
                    uart->m_callback(std::vector<uint8_t>(buffer, buffer + length));
                }
            }
        }
    }
