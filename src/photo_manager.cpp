#include "photo_manager.h"
#include "config.h" // For photo constants
#include "camera_handler.h" // For take_photo(), release_photo_buffer(), and fb
#include "ble_handler.h"    // For g_photo_data_characteristic and g_is_ble_connected
#include <Arduino.h> // For Serial, millis(), memcpy()

// Define global photo state variables here
bool g_is_capturing_photos = false;
int g_capture_interval_ms = 0;
unsigned long g_last_capture_time_ms = 0;
size_t g_sent_photo_bytes = 0;
uint16_t g_sent_photo_frames = 0; // Changed to uint16_t to match 2-byte frame counter
bool g_is_photo_uploading = false;

uint8_t *s_photo_chunk_buffer = nullptr;

void initialize_photo_manager() {
        Serial.printf("[MEM] Free PSRAM before photo chunk buffer alloc: %u bytes\n", ESP.getFreePsram());
    s_photo_chunk_buffer = (uint8_t *)ps_calloc(PHOTO_CHUNK_BUFFER_SIZE, sizeof(uint8_t));
    if (!s_photo_chunk_buffer) {
        Serial.println("[MEM] ERROR: Failed to allocate photo chunk buffer! Halting.");
        while (1);
    } else {
        Serial.println("[MEM] Photo chunk buffer allocated.");
    }

    // Default photo capture interval
    g_is_capturing_photos = true; // Default to capturing
    g_capture_interval_ms = 30000; // 30 seconds
    g_last_capture_time_ms = millis() - g_capture_interval_ms; // Ensure first capture can happen soon
    Serial.printf("[PHOTO] Default capture interval set to %d ms.\n", g_capture_interval_ms);
}

void handle_photo_control(int8_t control_value) {
    if (control_value == -1) {
        Serial.println("[PHOTO] Control: Single photo requested.");
        g_is_capturing_photos = true;
        g_capture_interval_ms = 0; // Indicates single shot
    } else if (control_value == 0) {
        Serial.println("[PHOTO] Control: Stop capture requested.");
        g_is_capturing_photos = false;
        g_capture_interval_ms = 0;
    } else if (control_value >= 1 && control_value <= 127) { // Interval in seconds
        Serial.printf("[PHOTO] Control: Interval capture requested. Interval: %d s.\n", control_value);
        g_capture_interval_ms = (unsigned long)control_value * 1000;
        g_is_capturing_photos = true;
        g_last_capture_time_ms = millis() - g_capture_interval_ms; // Allow immediate capture if due
    }
}

void process_photo_capture_and_upload(unsigned long current_time_ms) {
    // Check if it's time to capture a photo
    if (g_is_capturing_photos && !g_is_photo_uploading && g_is_ble_connected) {
        if ((g_capture_interval_ms == 0 && g_is_capturing_photos) || // Single shot request
            (g_capture_interval_ms > 0 && (current_time_ms - g_last_capture_time_ms >= (unsigned long)g_capture_interval_ms))) {
            
            if (g_capture_interval_ms == 0) { // If it was a single shot
                g_is_capturing_photos = false; // Don't take another unless requested
            }
            Serial.println("[PHOTO] Triggering photo capture...");
            if (take_photo()) { // take_photo is from camera_handler.h
                g_is_photo_uploading = true;
                g_sent_photo_bytes = 0;
                g_sent_photo_frames = 0;
                g_last_capture_time_ms = current_time_ms;
            } else {
                 g_is_photo_uploading = false; // Ensure this is false if take_photo failed
            }
        }
    }

    // If a photo is being uploaded
    if (g_is_photo_uploading && fb && s_photo_chunk_buffer && g_photo_data_characteristic) {
        size_t remaining = fb->len - g_sent_photo_bytes;
        if (remaining > 0) {
            s_photo_chunk_buffer[0] = (uint8_t)(g_sent_photo_frames & 0xFF);
            s_photo_chunk_buffer[1] = (uint8_t)((g_sent_photo_frames >> 8) & 0xFF);
            size_t bytes_to_copy = (remaining > MAX_PHOTO_CHUNK_PAYLOAD_SIZE) ? MAX_PHOTO_CHUNK_PAYLOAD_SIZE : remaining;
            memcpy(&s_photo_chunk_buffer[PHOTO_CHUNK_HEADER_LEN], &fb->buf[g_sent_photo_bytes], bytes_to_copy);

            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, bytes_to_copy + PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();

            g_sent_photo_bytes += bytes_to_copy;
            g_sent_photo_frames++;
            // Serial.printf("[PHOTO] Uploading chunk %u (%zu bytes). %zu bytes remaining.\n", g_sent_photo_frames, bytes_to_copy, remaining - bytes_to_copy);
        } else {
            s_photo_chunk_buffer[0] = 0xFF; // End of photo marker
            s_photo_chunk_buffer[1] = 0xFF;
            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();
            Serial.printf("[PHOTO] Upload complete. Total chunks: %u, Total bytes: %zu\n", g_sent_photo_frames, g_sent_photo_bytes);
            g_is_photo_uploading = false;
            release_photo_buffer(); // release_photo_buffer is from camera_handler.h
        }
    }
}