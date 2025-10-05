#include "camera_handler.h"
#include "logger.h"
#include "camera_pins.h"
#include <Arduino.h>
#include <esp_camera.h>

// Definition of the global frame buffer pointer
camera_fb_t *fb = nullptr;
static bool camera_initialized = false;
SemaphoreHandle_t g_camera_mutex = nullptr; // Mutex for camera access
SemaphoreHandle_t g_camera_request_semaphore = nullptr; // Signals the camera task
volatile bool g_is_photo_ready = false; // Flag indicates a photo is ready in the buffer

// Forward declaration for the internal, non-locking version
static void release_photo_buffer_internal();

// The dedicated camera task function
void camera_task(void *pvParameters) {
    while (true) {
        // Wait for a signal to take a photo
        if (xSemaphoreTake(g_camera_request_semaphore, portMAX_DELAY) == pdTRUE) {
            log_message("[CAM_TASK] Received photo request.");

            // Ensure camera is initialized
            if (!is_camera_initialized()) {
                configure_camera();
            }

            // Take the photo
            if (take_photo()) {
                log_message("[CAM_TASK] Photo captured successfully. Setting flag.");
                g_is_photo_ready = true; // Signal that the photo is ready
            } else {
                log_message("[CAM_TASK] Failed to capture photo.");
                g_is_photo_ready = false;
                deinit_camera();
            }
        }
    }
}

void initialize_camera_mutex_and_task() {
    g_camera_mutex = xSemaphoreCreateMutex();
    if (g_camera_mutex == nullptr) {
        log_message("[MUTEX] ERROR: Failed to create camera mutex! Halting.");
        while(1);
    }

    g_camera_request_semaphore = xSemaphoreCreateBinary();
    if (g_camera_request_semaphore == nullptr) {
        log_message("[MUTEX] ERROR: Failed to create camera semaphore! Halting.");
        while(1);
    }
    log_message("[MUTEX] Camera mutex and semaphore created successfully.");
    start_camera_task();
}

void start_camera_task() {
    xTaskCreatePinnedToCore(
        camera_task,          // Task function
        "CameraTask",         // Name of the task
        8192,                 // Stack size (increased for camera)
        NULL,                 // Parameter of the task
        2,                    // Priority of the task
        NULL,                 // Task handle
        1);                   // Core where the task should run
    log_message("[TASK] Dedicated camera task started.");
}

void request_photo() {
    g_is_photo_ready = false; // Reset the flag
    xSemaphoreGive(g_camera_request_semaphore); // Signal the camera task to take a photo
}

bool is_photo_ready() {
    return g_is_photo_ready;
}

camera_fb_t* get_photo_buffer() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (g_is_photo_ready && fb) {
            xSemaphoreGive(g_camera_mutex);
            return fb;
        }
        xSemaphoreGive(g_camera_mutex);
    }
    return nullptr;
}

void configure_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (camera_initialized) {
            log_message("[CAM] Already initialized.");
            xSemaphoreGive(g_camera_mutex);
            return;
        }

        log_message("[CAM] Initializing...");
        camera_config_t config;
        config.ledc_channel = LEDC_CHANNEL_0;
        config.ledc_timer = LEDC_TIMER_0;
        config.pin_d0 = Y2_GPIO_NUM;
        config.pin_d1 = Y3_GPIO_NUM;
        config.pin_d2 = Y4_GPIO_NUM;
        config.pin_d3 = Y5_GPIO_NUM;
        config.pin_d4 = Y6_GPIO_NUM;
        config.pin_d5 = Y7_GPIO_NUM;
        config.pin_d6 = Y8_GPIO_NUM;
        config.pin_d7 = Y9_GPIO_NUM;
        config.pin_xclk = XCLK_GPIO_NUM;
        config.pin_pclk = PCLK_GPIO_NUM;
        config.pin_vsync = VSYNC_GPIO_NUM;
        config.pin_href = HREF_GPIO_NUM;
        config.pin_sccb_sda = SIOD_GPIO_NUM;
        config.pin_sccb_scl = SIOC_GPIO_NUM;
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = 20000000;

        // Camera settings from old, stable code
        config.frame_size = FRAMESIZE_XGA;         // 1024x768 resolution
        config.pixel_format = PIXFORMAT_JPEG;      // Output format JPEG
        config.jpeg_quality = 20;                  // JPEG quality (0-63, lower means higher quality)
        config.fb_location = CAMERA_FB_IN_PSRAM;   // Store frame buffer in PSRAM
        config.fb_count = 2;                       // Use 2 frame buffers for stability
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Use WHEN_EMPTY for more stability

        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            log_message("[CAM] ERROR: Failed to initialize camera! Code: 0x%x", err);
            camera_initialized = false;
            xSemaphoreGive(g_camera_mutex);
            return;
        }
        camera_initialized = true;
        log_message("[CAM] Camera initialized successfully.");

        sensor_t * s = esp_camera_sensor_get();
        if (s) {
            s->reset(s);
            s->set_framesize(s, FRAMESIZE_XGA);
            s->set_pixformat(s, PIXFORMAT_JPEG);
            log_message("[CAM] Sensor reset, framesize and pixformat set post-init.");
        } else {
            log_message("[CAM] WARNING: Could not get sensor handle post-init.");
        }

        vTaskDelay(pdMS_TO_TICKS(250));
        xSemaphoreGive(g_camera_mutex);
    }
}

bool take_photo() {
    bool success = false;
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer_internal();

        for (int i = 0; i < 2; ++i) { // Warm-up frames
            camera_fb_t *tmp_fb = esp_camera_fb_get();
            if (tmp_fb) {
                esp_camera_fb_return(tmp_fb);
            }
        }

        fb = esp_camera_fb_get();
        if (!fb) {
            log_message("[CAM] ERROR: Failed to get frame buffer!");
            success = false;
        } else {
            log_message("[CAM] Photo captured: %zu bytes.", fb->len);
            success = true;
        }
        xSemaphoreGive(g_camera_mutex);
    }
    return success;
}

static void release_photo_buffer_internal() {
    if (fb) {
        esp_camera_fb_return(fb);
        fb = nullptr;
    }
}

void release_photo_buffer() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer_internal();
        xSemaphoreGive(g_camera_mutex);
    }
}

void deinit_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (!camera_initialized) {
            xSemaphoreGive(g_camera_mutex);
            return;
        }
        release_photo_buffer_internal();
        esp_camera_deinit();
        camera_initialized = false;
        log_message("[CAM] Deinitialized successfully.");
        xSemaphoreGive(g_camera_mutex);
    }
}

bool is_camera_initialized() {
    bool status;
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        status = camera_initialized;
        xSemaphoreGive(g_camera_mutex);
    }
    return status;
}

void warm_up_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        log_message("[CAM] Warming up camera...");
        // Capture and discard a few frames to allow sensor to stabilize
        for (int i = 0; i < 3; ++i) {
            camera_fb_t *warmup_fb = esp_camera_fb_get();
            if (warmup_fb) {
                log_message("[CAM] Discarding warmup frame %d.", i + 1);
                esp_camera_fb_return(warmup_fb);
            } else {
                log_message("[CAM] WARNING: Failed to get a warmup frame.");
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between captures
        }
        log_message("[CAM] Camera warmup complete.");
        xSemaphoreGive(g_camera_mutex);
    }
}
