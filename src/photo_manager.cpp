#include "photo_manager.h"
#include "config.h"       // For photo constants
#include "camera_handler.h" // For take_photo(), release_photo_buffer(), and fb
#include "ble_handler.h"    // For g_photo_data_characteristic and g_is_ble_connected
#include "led_handler.h"    // For LED status indicators
#include "logger.h"         // For thread-safe logging
#include <Arduino.h> // For Serial, millis(), memcpy()

// Define global photo state variables here
// bool g_is_capturing_photos = false; // Replaced by g_capture_mode
// int g_capture_interval_ms = 0; // Replaced by g_capture_interval_ms
PhotoCaptureMode g_capture_mode = MODE_STOP;
int g_capture_interval_ms = 0;
unsigned long g_last_capture_time_ms = 0;
size_t g_sent_photo_bytes = 0;
uint16_t g_sent_photo_frames = 0;
bool g_is_photo_uploading = false;
volatile bool g_photo_notifications_enabled = false;
volatile bool g_single_shot_pending = false;

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

    g_capture_mode = MODE_STOP;
    g_capture_interval_ms = 0;
    g_last_capture_time_ms = 0;
    logger_printf("[PHOTO] Photo manager initialized. Starting in STOP mode.");
}

void handle_photo_control(int8_t control_value) {
    logger_printf("[PHOTO] handle_photo_control received: %d", control_value);
    if (control_value == -1) {
        logger_printf("[PHOTO] Control: Single photo requested.");
        // Add a delay to give the client time to prepare for the data stream.
        // This helps prevent a race condition where the client isn't ready for the first chunk.
        delay(200);
        g_single_shot_pending = true;
    } else if (control_value == 0) {
        logger_printf("[PHOTO] Control: Stop capture requested.");
        g_capture_mode = MODE_STOP;
        g_capture_interval_ms = 0;
        g_single_shot_pending = false;
    } else if (control_value >= 5 && control_value <= 127) {
        logger_printf("[PHOTO] Control: Interval capture requested. Interval: %d s.", control_value);
        g_capture_interval_ms = (unsigned long)control_value * 1000;
        g_capture_mode = MODE_INTERVAL;
        g_single_shot_pending = true; // Trigger an immediate photo
        g_last_capture_time_ms = millis();
        logger_printf("[PHOTO] Interval mode set. First photo will be taken immediately.");
    } else {
        logger_printf("[PHOTO] Ignoring invalid or too-short interval: %d", control_value);
    }
}

void process_photo_capture_and_upload(unsigned long current_time_ms) {
    // --- Step 1: Request a photo from the camera task if needed ---
    if (!g_is_photo_uploading && g_is_ble_connected && g_photo_notifications_enabled) {
        bool trigger_capture = false;
        if (g_single_shot_pending) {
            trigger_capture = true;
            g_single_shot_pending = false; // Consume flag immediately
            logger_printf("[PHOTO_MGR] Single shot triggered. Signaling camera task.");
        } else if (g_capture_mode == MODE_INTERVAL) {
            if (current_time_ms - g_last_capture_time_ms >= (unsigned long)g_capture_interval_ms) {
                trigger_capture = true;
                g_last_capture_time_ms = current_time_ms;
                logger_printf("[PHOTO_MGR] Interval triggered. Signaling camera task.");
            }
        }

        if (trigger_capture) {
            xSemaphoreGive(g_camera_request_semaphore); // Signal the dedicated camera task
        }
    }

    // --- Step 2: Check if a photo is ready and start the upload ---
    if (g_is_photo_ready && !g_is_photo_uploading) {
        logger_printf("[PHOTO_MGR] Photo is ready. Starting upload.");
        set_led_status(LED_STATUS_PHOTO_CAPTURING);
        start_photo_upload();
        g_is_photo_ready = false; // Consume the flag
    }

    // --- Step 3: Continue an ongoing photo upload ---
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
            delay(20); // Increased delay to 20ms to improve reliability with large MTU

            g_sent_photo_bytes += bytes_to_copy;
            g_sent_photo_frames++;
        } else {
            // End-of-photo marker
            s_photo_chunk_buffer[0] = 0xFF;
            s_photo_chunk_buffer[1] = 0xFF;
            g_photo_data_characteristic->setValue(s_photo_chunk_buffer, PHOTO_CHUNK_HEADER_LEN);
            g_photo_data_characteristic->notify();
            logger_printf("[PHOTO][END] Sent end-of-photo marker. Total chunks: %u, Total bytes: %zu", g_sent_photo_frames, g_sent_photo_bytes);

            // The CRC check has been removed for reliability.
            // The BLE link-layer has its own integrity checks.

            g_is_photo_uploading = false;
            logger_printf("[PHOTO][UPLOAD] Upload complete.");
            release_photo_buffer();
        }
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
    g_single_shot_pending = false;
    g_is_photo_ready = false;
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