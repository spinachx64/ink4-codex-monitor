"""Bound the SSD1677 non-copy DMA bounce buffer without changing frame contents."""
from pathlib import Path

def patch_display(source: Path, output: Path):
    rel=Path('main/boards/metalio-e-ink-4/display/esp_lcd_panel_ssd1677.c')
    text=(source/rel).read_text()
    start=text.index('            if (!epaper_panel->_framebuffer) {',text.index('const bool direct_ok'))
    end=text.index('            tx_buf = epaper_panel->_framebuffer;',start)+len('            tx_buf = epaper_panel->_framebuffer;')
    text=text[:start]+'''            // A full 48KB internal bounce conflicts with BLE and I2S DMA.
            // Polling SPI transactions finish before this 4KB buffer is reused.
            enum { BOUNCE_BYTES = 4096 };
            if (!epaper_panel->_framebuffer) {
                epaper_panel->_framebuffer = heap_caps_malloc(BOUNCE_BYTES,
                    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
                ESP_RETURN_ON_FALSE(epaper_panel->_framebuffer, ESP_ERR_NO_MEM,
                                    TAG, "dma bounce alloc failed");
            }
            const uint8_t command = (epaper_panel->bitmap_color == SSD1677_EPAPER_BITMAP_PREVIOUS)
                ? SSD1677_CMD_WRITE_RED_VRAM : SSD1677_CMD_WRITE_BW_VRAM;
            for (size_t offset = 0; offset < (size_t)buffer_size;) {
                size_t length = (size_t)buffer_size - offset;
                if (length > BOUNCE_BYTES) length = BOUNCE_BYTES;
                memcpy(epaper_panel->_framebuffer, (const uint8_t*)color_data + offset, length);
                // -1 sends data only: the panel's VRAM address counter continues.
                ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(epaper_panel->io,
                    offset == 0 ? command : -1, epaper_panel->_framebuffer, length),
                    TAG, "chunked VRAM write failed");
                offset += length;
            }
            return ESP_OK;'''+text[end:]
    (output/rel).write_text(text)
