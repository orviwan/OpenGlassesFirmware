#include "photo_handler.h"
#include "logger.h"
#include "ble_handler.h"
#include "camera_handler.h"
#include "state_handler.h"
#include "config.h"
#include <algorithm>
#include <Arduino.h>

// Task handle for the photo sender task
static TaskHandle_t g_photo_sender_task_handle = NULL;
static TaskHandle_t g_interval_photo_task_handle = NULL;
static uint32_t g_photo_interval_ms = 0;

// Global variable to hold the mode for the photo sender task
static photo_mode_t g_current_photo_mode;

SemaphoreHandle_t g_photo_request_semaphore = NULL;

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

    // Create the ACK semaphore
    g_photo_ack_semaphore = xSemaphoreCreateBinary();
    if (g_photo_ack_semaphore == nullptr) {
        logger_printf("[PHOTO] ERROR: Failed to create photo ACK semaphore!");
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

void start_photo_transfer_task(photo_mode_t mode) {
    // Store the requested mode for the persistent task
    g_current_photo_mode = mode;

    // Check if the handler is initialized
    if (g_photo_sender_task_handle == NULL || g_photo_request_semaphore == NULL) {
        logger_printf("[PHOTO] ERROR: Photo handler not initialized before request!");
        return;
    }

    // Give the semaphore to unblock the photo_sender_task
    logger_printf("[PHOTO] Signaling photo sender task to start.");
    xSemaphoreGive(g_photo_request_semaphore);
}

// The core task that waits for a signal, takes a photo, and sends it.
void photo_sender_task(void *pvParameters) {
    while (true) {
        // Wait indefinitely for a signal to start the photo process
        if (xSemaphoreTake(g_photo_request_semaphore, portMAX_DELAY) == pdTRUE) {
            logger_printf("[PHOTO] Received photo request, starting process.");
            set_current_state(STATE_TAKING_PHOTO);
            
            // Request a photo with the stored mode
            request_photo(g_current_photo_mode);

            // Wait for the photo to be ready
            unsigned long start_time = millis();
            while (!is_photo_ready()) {
                if (millis() - start_time > 5000) { // 5-second timeout
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
            set_current_state(STATE_TRANSFERRING_PHOTO_BLE);

            // --- New Protocol: Send Start-of-Transfer marker with file size ---
            uint8_t start_marker[6];
            start_marker[0] = 0xFE; // Start marker byte 1
            start_marker[1] = 0xFF; // Start marker byte 2
            uint32_t file_size = fb->len;
            memcpy(&start_marker[2], &file_size, sizeof(file_size)); // Copy 4-byte size
            
            // Send the start marker first and assume it's received.
            notify_photo_data(start_marker, sizeof(start_marker));
            delay(20); // Small delay for the packet to be sent.
            // --------------------------------------------------------------------

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
        delay(50); // Delay to prevent overwhelming the client
    }            // 5. Send the end-of-transfer marker
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

// --- Interval Photo (Not yet implemented) ---
void start_interval_photo(uint32_t interval_ms) {
    logger_printf("[PHOTO] Interval photo mode not implemented.");
}

void stop_interval_photo() {
    logger_printf("[PHOTO] Interval photo mode not implemented.");
}
