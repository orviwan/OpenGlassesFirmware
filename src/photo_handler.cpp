#include "photo_handler.h"
#include "logger.h"
#include "ble_handler.h"
#include "camera_handler.h"
#include "state_handler.h"
#include <algorithm>

void take_and_send_photo() {
    log_message("Taking photo...");
    uint8_t* pic_buf;
    size_t pic_len;

    if (take_photo(&pic_buf, &pic_len)) {
        log_message("Photo taken, sending over BLE...");
        size_t chunk_size = 200; 
        for (size_t i = 0; i < pic_len; i += chunk_size) {
            size_t size_to_send = std::min(chunk_size, pic_len - i);
            notify_photo_data(pic_buf + i, size_to_send);
        }
        release_photo_buffer();
        log_message("Photo sent.");
    } else {
        log_message("Failed to take photo.");
    }
    set_current_state(STATE_IDLE);
}

void start_interval_photo(uint32_t interval_ms) {
    log_message("Interval photo mode started.");
}

void stop_interval_photo() {
    log_message("Interval photo mode stopped.");
    set_current_state(STATE_IDLE);
}
