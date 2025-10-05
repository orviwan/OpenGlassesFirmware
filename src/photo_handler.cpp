#include "photo_handler.h"
#include "logger.h"
#include "ble_handler.h"
#include "camera_handler.h"
#include "state_handler.h"
#include "config.h"
#include <algorithm>
#include <Arduino.h>

static SemaphoreHandle_t g_photo_request_semaphore = nullptr;
static TaskHandle_t g_photo_sender_task_handle = nullptr;

// The core task that waits for a signal, takes a photo, and sends it.
void photo_sender_task(void *pvParameters) {
    while (true) {
        // Wait indefinitely for a signal to start the photo process
        if (xSemaphoreTake(g_photo_request_semaphore, portMAX_DELAY) == pdTRUE) {
            logger_printf("[PHOTO] Received photo request, starting process.");
            set_current_state(STATE_TAKING_PHOTO);

            // 1. Request photo from the camera handler
            request_photo();

            // 2. Wait for the photo to be ready (with a timeout)
            unsigned long startTime = millis();
            while (!is_photo_ready()) {
                if (millis() - startTime > 5000) { // 5-second timeout
                    logger_printf("[PHOTO] ERROR: Timeout waiting for photo from camera task.");
                    release_photo_buffer(); // Clean up just in case
                    set_current_state(STATE_IDLE);
                    continue; // Go back to waiting for a new request
                }
                delay(50);
            }

            // 3. Get the framebuffer
            camera_fb_t* fb = get_photo_buffer();
            if (!fb || !fb->buf || fb->len == 0) {
                logger_printf("[PHOTO] ERROR: Photo buffer was not valid after wait.");
                release_photo_buffer();
                set_current_state(STATE_IDLE);
                continue; // Go back to waiting for a new request
            }

            logger_printf("[PHOTO] Photo is ready (%d bytes). Sending over BLE...\n", fb->len);
            set_current_state(STATE_SENDING_PHOTO);

            // 4. Send the photo data in framed chunks
            uint8_t chunk_buffer[PHOTO_CHUNK_BUFFER_SIZE];
            uint16_t sequence_number = 0;
            size_t bytes_sent = 0;

            while (bytes_sent < fb->len) {
                size_t payload_size = std::min((size_t)MAX_PHOTO_CHUNK_PAYLOAD_SIZE, fb->len - bytes_sent);
                
                // First 2 bytes are the sequence number (little-endian)
                chunk_buffer[0] = sequence_number & 0xFF;
                chunk_buffer[1] = (sequence_number >> 8) & 0xFF;

                // Copy the image data payload
                memcpy(&chunk_buffer[PHOTO_CHUNK_HEADER_LEN], fb->buf + bytes_sent, payload_size);

                // Send the complete chunk (header + payload)
                notify_photo_data(chunk_buffer, payload_size + PHOTO_CHUNK_HEADER_LEN);

                bytes_sent += payload_size;
                sequence_number++;
                delay(1); // Yield to prevent watchdog timeouts and allow BLE stack to work
            }

            // 5. Send the end-of-transfer marker
            uint16_t end_marker = 0xFFFF;
            notify_photo_data((uint8_t*)&end_marker, sizeof(end_marker));
            delay(100); // Give BLE stack time to send the final packet

            logger_printf("[PHOTO] Photo transfer complete.");

            // 6. Clean up
            release_photo_buffer();
            set_current_state(STATE_IDLE);
        }
    }
}

// Initializes the semaphore and creates the sender task.
void initialize_photo_handler() {
    if (g_photo_sender_task_handle != nullptr) {
        logger_printf("[PHOTO] Handler already initialized.");
        return;
    }

    g_photo_request_semaphore = xSemaphoreCreateBinary();
    if (g_photo_request_semaphore == nullptr) {
        logger_printf("[PHOTO] ERROR: Failed to create photo request semaphore!");
        return;
    }

    xTaskCreate(
        photo_sender_task,
        "PhotoSenderTask",
        4096, // Stack size
        NULL, // Task parameters
        1,    // Priority
        &g_photo_sender_task_handle
    );
    logger_printf("[PHOTO] Photo sender task created.");
}

// Public function to be called by other modules (e.g., BLE handler) to trigger a photo.
void start_photo_transfer_task() {
    if (g_photo_request_semaphore != nullptr) {
        logger_printf("[PHOTO] Signaling photo sender task to start.");
        xSemaphoreGive(g_photo_request_semaphore);
    } else {
        logger_printf("[PHOTO] ERROR: Cannot start transfer, handler not initialized.");
    }
}


// --- Interval Photo (Not yet implemented) ---
void start_interval_photo(uint32_t interval_ms) {
    logger_printf("[PHOTO] Interval photo mode not implemented.");
}

void stop_interval_photo() {
    logger_printf("[PHOTO] Interval photo mode not implemented.");
}
