#include "photo_manager.h"
#include "config.h" // For photo constants
#include "camera_handler.h" // For take_photo(), release_photo_buffer(), and fb
#include "ble_handler.h"    // For g_photo_data_characteristic and g_is_ble_connected
#include <Arduino.h> // For Serial, millis(), memcpy()

// Define global photo state variables here
// bool g_is_capturing_photos = false; // Replaced by g_capture_mode
// int g_capture_interval_ms = 0; // Replaced by g_capture_interval_ms
PhotoCaptureMode g_capture_mode = MODE_STOP;
int g_capture_interval_ms = 0;
unsigned long g_last_capture_time_ms = 0;
size_t g_sent_photo_bytes = 0;
uint16_t g_sent_photo_frames = 0; // Changed to uint16_t to match 2-byte frame counter
bool g_is_photo_uploading = false;
bool g_is_processing_capture_request = false; // Flag to indicate if a capture request is being processed
bool g_single_shot_pending = false; // Flag for single photo request pending

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

    // Start in stop mode by default, client will set mode
    g_capture_mode = MODE_STOP;
    g_capture_interval_ms = 0;
    g_last_capture_time_ms = 0;
    Serial.println("[PHOTO] Photo manager initialized. Starting in STOP mode.");
}

void handle_photo_control(int8_t control_value) {
    Serial.printf("[PHOTO] handle_photo_control received: %d\n", control_value);
    if (control_value == -1) {
        Serial.println("[PHOTO] Control: Single photo requested.");
        g_single_shot_pending = true;
    } else if (control_value == 0) {
        Serial.println("[PHOTO] Control: Stop capture requested.");
        g_capture_mode = MODE_STOP;
        g_capture_interval_ms = 0;
        g_single_shot_pending = false;
    } else if (control_value >= 5 && control_value <= 127) { // Only allow intervals >= 5s
        Serial.printf("[PHOTO] Control: Interval capture requested. Interval: %d s.\n", control_value);
        g_capture_interval_ms = (unsigned long)control_value * 1000;
        g_capture_mode = MODE_INTERVAL;
        g_last_capture_time_ms = millis(); // Start timer from now, so first image is after interval
        g_single_shot_pending = false;
    } else {
        Serial.printf("[PHOTO] Ignoring invalid or too-short interval: %d\n", control_value);
    }
}

void process_photo_capture_and_upload(unsigned long current_time_ms) {
    // Serial.println("[PHOTO] process_photo_capture_and_upload"); // Too frequent, uncomment if needed

    // Handle photo capture triggering
    if (!g_is_photo_uploading && g_is_ble_connected && !g_is_processing_capture_request) {
        bool trigger_capture = false;

        // Prioritize single shot request
        if (g_single_shot_pending) {
            trigger_capture = true;
            g_single_shot_pending = false; // Consume the single shot request immediately
            Serial.println("[PHOTO] Single photo request."); // Simplified log
        }
        // If no single shot pending, check for interval capture if in interval mode
        else if (g_capture_mode == MODE_INTERVAL) {
            if (current_time_ms - g_last_capture_time_ms >= (unsigned long)g_capture_interval_ms) {
                trigger_capture = true;
                // g_last_capture_time_ms = current_time_ms; // Update timer *after* capture attempt succeeds
                Serial.println("[PHOTO] Interval capture."); // Simplified log
            }
        }

        if (trigger_capture) {
            g_is_processing_capture_request = true;
            // Always release previous frame buffer before taking a new photo
            release_photo_buffer();

            if (take_photo()) { // take_photo is from camera_handler.h
                // Serial.println("[PHOTO] take_photo succeeded."); // Commented to reduce log noise
                if (fb && fb->len > 0) {
                    g_is_photo_uploading = true;
                    g_sent_photo_bytes = 0;
                    g_sent_photo_frames = 0;
                    // Update interval timer only on successful capture
                    if (g_capture_mode == MODE_INTERVAL) {
                         g_last_capture_time_ms = current_time_ms;
                    }
                     Serial.printf("[PHOTO] Uploading image: %zu bytes\n", fb->len); // Simplified log
                } else {
                    Serial.println("[PHOTO] take_photo succeeded but returned empty or invalid frame buffer.");
                    g_is_photo_uploading = false; // Ensure this is false if frame buffer is invalid
                    release_photo_buffer(); // Attempt to release the invalid buffer
                }
            } else {
                Serial.println("[PHOTO] take_photo failed.");
                 g_is_photo_uploading = false; // Ensure this is false if take_photo failed
            }
            g_is_processing_capture_request = false; // Reset flag after capture attempt completes
        }
    }

    // If a photo is being uploaded
    if (g_is_photo_uploading && fb && fb->len > 0 && s_photo_chunk_buffer && g_photo_data_characteristic) {
        size_t remaining = fb->len - g_sent_photo_bytes;
        if (remaining > 0) {
            s_photo_chunk_buffer[0] = (uint8_t)(g_sent_photo_frames & 0xFF);
            s_photo_chunk_buffer[1] = (uint8_t)((g_sent_photo_frames >> 8) & 0xFF);
            size_t bytes_to_copy = (remaining > MAX_PHOTO_CHUNK_PAYLOAD_SIZE) ? MAX_PHOTO_CHUNK_PAYLOAD_SIZE : remaining;
            memcpy(&s_photo_chunk_buffer[PHOTO_CHUNK_HEADER_LEN], &fb->buf[g_sent_photo_bytes], bytes_to_copy);

            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, bytes_to_copy + PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();
            // Serial.printf("[PHOTO] Sent chunk %u (%zu bytes). %zu bytes remaining.\n", g_sent_photo_frames + 1, bytes_to_copy + PHOTO_CHUNK_HEADER_LEN, remaining - bytes_to_copy); // Commented to reduce log noise

            g_sent_photo_bytes += bytes_to_copy;
            g_sent_photo_frames++;
            // Serial.printf("[PHOTO] Uploading chunk %u (%zu bytes). %zu bytes remaining.\n", g_sent_photo_frames, bytes_to_copy, remaining - bytes_to_copy);
        } else {
            s_photo_chunk_buffer[0] = 0xFF; // End of photo marker
            s_photo_chunk_buffer[1] = 0xFF;
            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();
            // Serial.printf("[PHOTO] Sent end-of-photo marker. Total chunks: %u, Total bytes: %zu\n", g_sent_photo_frames, g_sent_photo_bytes); // Commented to reduce log noise
            g_is_photo_uploading = false;
            release_photo_buffer(); // release_photo_buffer is from camera_handler.h
            // Serial.println("[PHOTO] Photo upload complete."); // Commented to reduce log noise
        }
    }
}