/*
 * SPDX-FileCopyrightText: 2026 wangjiacheng
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>

namespace ble_nus {

// Reserve controller/host memory on the internal-RAM boot task, before EPD DMA.
// Does not advertise or connect; the application remains manually launched.
bool prepare(const char* device_name);
// Activate the prepared service; never initialize the controller on the LVGL task.
bool start(const char* device_name);
bool failed();

// Stop advertising and disconnect; retain the initialized host for reuse.
void pause();

// If a complete '\n'-terminated line has arrived since the last call, copy it
// (without the newline) into `out` and return true. Otherwise return false.
bool poll_line(char* out, int out_size, int64_t* received_us = nullptr);

// Notify the connected central (the Mac bridge) to push a fresh reading now.
// Before connection/subscription, coalesce requests and send once TX is ready.
// Closing the page cancels the pending request.
void request_refresh();
bool connected();

}  // namespace ble_nus
