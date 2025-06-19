#include "photo_manager.h"
#include "config.h" // For photo constants
#include "camera_handler.h" // For take_photo(), release_photo_buffer(), and fb
#include "ble_handler.h"    // For g_photo_data_characteristic and g_is_ble_connected
#include "led_handler.h"    // For LED status indicators
#include "logger.h"         // For thread-safe logging
#include <Arduino.h> // For Serial, millis(), memcpy()
#include <esp_rom_crc.h> // For esp_rom_crc32_le (ESP32 ROM CRC32 hardware)

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

void start_photo_upload(); // Forward declaration

uint8_t *s_photo_chunk_buffer = nullptr;

void initialize_photo_manager() {
    logger_printf(" ");
    logger_printf("[MEM] Free PSRAM before photo chunk buffer alloc: %u bytes", ESP.getFreePsram());
    s_photo_chunk_buffer = (uint8_t *)ps_calloc(PHOTO_CHUNK_BUFFER_SIZE, sizeof(uint8_t));
    if (!s_photo_chunk_buffer) {
        logger_printf("[MEM] ERROR: Failed to allocate photo chunk buffer! Halting.");
        while (1);
    } else {
        logger_printf("[MEM] Photo chunk buffer allocated.");
    }

    // Start in stop mode by default, client will set mode
    g_capture_mode = MODE_STOP;
    g_capture_interval_ms = 0;
    g_last_capture_time_ms = 0;
    logger_printf("[PHOTO] Photo manager initialized. Starting in STOP mode.");
}

void handle_photo_control(int8_t control_value) {
    logger_printf("[PHOTO] handle_photo_control received: %d", control_value);
    if (control_value == -1) {
        logger_printf("[PHOTO] Control: Single photo requested.");
        g_single_shot_pending = true;
    } else if (control_value == 0) {
        logger_printf("[PHOTO] Control: Stop capture requested.");
        g_capture_mode = MODE_STOP;
        g_capture_interval_ms = 0;
        g_single_shot_pending = false;
    } else if (control_value >= 5 && control_value <= 127) // Only allow intervals >= 5s
    {
        logger_printf("[PHOTO] Control: Interval capture requested. Interval: %d s.", control_value);
        g_capture_interval_ms = (unsigned long)control_value * 1000;
        g_capture_mode = MODE_INTERVAL;
        // g_last_capture_time_ms = millis(); // Start timer from now, so first image is after interval
        // Instead, trigger an immediate photo, and the timer will be set after that photo is taken.
        g_single_shot_pending = true; // Trigger an immediate photo
        g_last_capture_time_ms = millis(); // Set last capture time to now to ensure the *next* interval photo is after the full interval
        logger_printf("[PHOTO] Interval mode set. First photo will be taken immediately.");
    } else {
        logger_printf("[PHOTO] Ignoring invalid or too-short interval: %d", control_value);
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
            logger_printf("[PHOTO] Single photo request."); // Simplified log
        }
        // If no single shot pending, check for interval capture if in interval mode
        else if (g_capture_mode == MODE_INTERVAL) {
            if (current_time_ms - g_last_capture_time_ms >= (unsigned long)g_capture_interval_ms) {
                trigger_capture = true;
                // g_last_capture_time_ms = current_time_ms; // Update timer *after* capture attempt succeeds
                logger_printf("[PHOTO] Interval capture triggered by timer."); // Clarified log
            }
        }

        if (trigger_capture) {
            g_is_processing_capture_request = true;
            // Always release previous frame buffer before taking a new photo
            release_photo_buffer();

            configure_camera(); // Only initialize camera when needed
            if (take_photo()) {
                logger_printf("[PHOTO] Captured photo, starting upload...\n");
                set_led_status(LED_STATUS_PHOTO_CAPTURING); // Blink LED red
                start_photo_upload();
                g_last_capture_time_ms = current_time_ms; // Update time after successful capture
                // deinit_camera(); // Do NOT deinitialize camera here
            } else {
                logger_printf("[PHOTO] take_photo failed.\n");
                g_is_photo_uploading = false; // Ensure this is false if take_photo failed
                deinit_camera(); // Clean up even on failure
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
            size_t bytes_to_copy = (remaining > g_photo_chunk_payload_size) ? g_photo_chunk_payload_size : remaining;
            memcpy(&s_photo_chunk_buffer[PHOTO_CHUNK_HEADER_LEN], &fb->buf[g_sent_photo_bytes], bytes_to_copy);

            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, bytes_to_copy + PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();
            logger_printf("[PHOTO][CHUNK] Frame: %u, Bytes: %zu, Offset: %zu, Remaining: %zu", g_sent_photo_frames, bytes_to_copy, g_sent_photo_bytes, remaining - bytes_to_copy);
            delay(10); // Add a 10ms delay to help client processing

            g_sent_photo_bytes += bytes_to_copy;
            g_sent_photo_frames++;
        } else {
            // End-of-photo marker
            s_photo_chunk_buffer[0] = 0xFF;
            s_photo_chunk_buffer[1] = 0xFF;
            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();
            logger_printf("[PHOTO][END] Sent end-of-photo marker. Total chunks: %u, Total bytes: %zu", g_sent_photo_frames, g_sent_photo_bytes);

            // Calculate CRC32 of the JPEG buffer
            uint32_t crc = esp_rom_crc32_le(0, fb->buf, fb->len);
            logger_printf("[PHOTO][CRC32] Calculated CRC32: 0x%08lX", crc);
            logger_printf(" ");
            // Send CRC32 as a special chunk: header 0xFE 0xFE, 4 bytes CRC32 (little-endian)
            s_photo_chunk_buffer[0] = 0xFE;
            s_photo_chunk_buffer[1] = 0xFE;
            s_photo_chunk_buffer[2] = (uint8_t)(crc & 0xFF);
            s_photo_chunk_buffer[3] = (uint8_t)((crc >> 8) & 0xFF);
            s_photo_chunk_buffer[4] = (uint8_t)((crc >> 16) & 0xFF);
            s_photo_chunk_buffer[5] = (uint8_t)((crc >> 24) & 0xFF);
            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, 6);
            g_photo_data_characteristic->notify();
            logger_printf("[PHOTO][CRC32] Sent CRC32 chunk to client.");

            g_is_photo_uploading = false;
            logger_printf("[PHOTO][UPLOAD] Upload complete, g_is_photo_uploading set to 0");
            release_photo_buffer(); // release_photo_buffer is from camera_handler.h
            deinit_camera(); // Deinitialize camera only after upload is fully complete
        }
    }

    // Watchdog: if stuck uploading for >30s, reset state
    static unsigned long upload_start_time = 0;
    if (g_is_photo_uploading && upload_start_time == 0) {
        upload_start_time = millis();
    }
    if (!g_is_photo_uploading) {
        upload_start_time = 0;
    }
    if (g_is_photo_uploading && (millis() - upload_start_time > 30000)) {
        logger_printf("[PHOTO][WATCHDOG] Upload stuck >30s, forcing reset\n");
        g_is_photo_uploading = false;
        upload_start_time = 0;
        release_photo_buffer();
    }
}

void reset_photo_manager_state() {
    logger_printf("[PHOTO] Resetting photo manager state.");
    g_capture_mode = MODE_STOP;
    g_capture_interval_ms = 0;
    g_last_capture_time_ms = 0;
    g_sent_photo_bytes = 0;
    g_sent_photo_frames = 0;
    g_is_photo_uploading = false;
    g_is_processing_capture_request = false;
    g_single_shot_pending = false;

    // Also release any held camera frame buffer
    release_photo_buffer();
}

void start_photo_upload() {
    if (fb && fb->len > 0) {
        g_is_photo_uploading = true;
        g_sent_photo_bytes = 0;
        g_sent_photo_frames = 0;
        logger_printf("[PHOTO] Starting photo upload. Total size: %zu bytes\n", fb->len);
    } else {
        logger_printf("[PHOTO] ERROR: Cannot start upload, no valid photo buffer.\n");
        g_is_photo_uploading = false;
    }
}