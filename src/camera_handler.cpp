#include "camera_handler.h"
#include "logger.h"
#include "camera_pins.h"
#include <Arduino.h>
#include <esp_camera.h>

// Definition of the global frame buffer pointer
static camera_fb_t *g_fb = nullptr;
static bool camera_initialized = false;
SemaphoreHandle_t g_camera_mutex = nullptr; // Mutex for camera access
SemaphoreHandle_t g_camera_request_semaphore = nullptr; // Signals the camera task
volatile bool g_is_photo_ready = false; // Flag indicates a photo is ready in the buffer
static photo_mode_t g_requested_photo_mode = PHOTO_MODE_MEDIUM_QUALITY; // Global to hold the requested mode

// Forward declaration for the internal, non-locking version
static void release_photo_buffer_internal();

// The dedicated camera task function
void camera_task(void *pvParameters) {
    while (true) {
        // Wait for a signal to take a photo
        if (xSemaphoreTake(g_camera_request_semaphore, portMAX_DELAY) == pdTRUE) {
            logger_printf("[CAM_TASK] Received photo request.");

            // Ensure camera is initialized
            if (!is_camera_initialized()) {
                configure_camera();
            }

            // Take the photo using the globally stored mode
            if (take_photo(g_requested_photo_mode)) {
                logger_printf("[CAM_TASK] Photo captured successfully. Setting flag.");
                g_is_photo_ready = true; // Signal that the photo is ready
            } else {
                logger_printf("[CAM_TASK] Failed to capture photo.");
                g_is_photo_ready = false;
                deinit_camera();
            }
        }
    }
}

void initialize_camera_mutex_and_task() {
    g_camera_mutex = xSemaphoreCreateMutex();
    if (g_camera_mutex == nullptr) {
        logger_printf("[MUTEX] ERROR: Failed to create camera mutex! Halting.");
        while(1);
    }

    g_camera_request_semaphore = xSemaphoreCreateBinary();
    if (g_camera_request_semaphore == nullptr) {
        logger_printf("[MUTEX] ERROR: Failed to create camera semaphore! Halting.");
        while(1);
    }
    logger_printf("[MUTEX] Camera mutex and semaphore created successfully.");
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
    logger_printf("[TASK] Dedicated camera task started.");
}

void request_photo(photo_mode_t mode) {
    g_is_photo_ready = false; // Reset the flag immediately
    g_requested_photo_mode = mode;
    xSemaphoreGive(g_camera_request_semaphore); // Signal the camera task to take a photo
}

bool is_photo_ready() {
    return g_is_photo_ready;
}

camera_fb_t* get_photo_buffer() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (g_is_photo_ready && g_fb) {
            xSemaphoreGive(g_camera_mutex);
            return g_fb;
        }
        xSemaphoreGive(g_camera_mutex);
    }
    return nullptr;
}

void configure_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (camera_initialized) {
            logger_printf("[CAM] Already initialized.");
            xSemaphoreGive(g_camera_mutex);
            return;
        }

        logger_printf("[CAM] Initializing...");
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

        // CRITICAL FIX: Initialize with the LARGEST possible frame size.
        // The buffers are allocated based on this initial setting.
        config.frame_size = FRAMESIZE_UXGA;        // Allocate for High Quality
        config.pixel_format = PIXFORMAT_JPEG;      // Output format JPEG
        config.jpeg_quality = 10;                  // Default to high quality
        config.fb_location = CAMERA_FB_IN_PSRAM;   // Store frame buffer in PSRAM
        config.fb_count = 2;                       // Use 2 frame buffers for stability
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Use WHEN_EMPTY for more stability

        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            logger_printf("[CAM] ERROR: Failed to initialize camera! Code: 0x%x", err);
            camera_initialized = false;
            xSemaphoreGive(g_camera_mutex);
            return;
        }
        camera_initialized = true;
        logger_printf("[CAM] Camera initialized successfully (UXGA buffer).");

        sensor_t * s = esp_camera_sensor_get();
        if (s) {
            // After init, set the default operating size to a smaller, faster one.
            // The buffer is already big enough for UXGA if requested later.
            s->set_framesize(s, FRAMESIZE_XGA);
            s->set_pixformat(s, PIXFORMAT_JPEG);
            logger_printf("[CAM] Sensor configured to default XGA size post-init.");
        } else {
            logger_printf("[CAM] WARNING: Could not get sensor handle post-init.");
        }

        vTaskDelay(pdMS_TO_TICKS(250));
        xSemaphoreGive(g_camera_mutex);
    }
}

bool take_photo(photo_mode_t mode) {
    bool success = false;
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer_internal();

        sensor_t * s = esp_camera_sensor_get();
        if (!s) {
            logger_printf("[CAM] ERROR: Could not get sensor handle!");
            xSemaphoreGive(g_camera_mutex);
            return false;
        }

        // Configure sensor based on the requested mode
        switch (mode) {
            case PHOTO_MODE_HIGH_QUALITY:
                logger_printf("[CAM] Setting mode: HIGH_QUALITY (UXGA, Q10)");
                s->set_framesize(s, FRAMESIZE_UXGA); // 1600x1200
                s->set_quality(s, 10); // 0-63, lower is higher quality
                break;
            case PHOTO_MODE_MEDIUM_QUALITY:
                logger_printf("[CAM] Setting mode: MEDIUM_QUALITY (XGA, Q20)");
                s->set_framesize(s, FRAMESIZE_XGA); // 1024x768
                s->set_quality(s, 20);
                break;
            case PHOTO_MODE_FAST_TRANSFER:
                logger_printf("[CAM] Setting mode: FAST_TRANSFER (XGA, Q35)");
                s->set_framesize(s, FRAMESIZE_XGA); // 1024x768
                s->set_quality(s, 35);
                break;
            default:
                logger_printf("[CAM] WARNING: Unknown photo mode requested. Defaulting to Medium.");
                s->set_framesize(s, FRAMESIZE_XGA);
                s->set_quality(s, 20);
                break;
        }
        
        // Give the sensor a moment to apply new settings
        vTaskDelay(pdMS_TO_TICKS(250));

        g_fb = esp_camera_fb_get();
        if (!g_fb) {
            logger_printf("[CAM] ERROR: Failed to get frame buffer!");
            success = false;
        } else {
            logger_printf("[CAM] Photo captured: %zu bytes.", g_fb->len);
            success = true;
        }
        xSemaphoreGive(g_camera_mutex);
    }
    return success;
}

static void release_photo_buffer_internal() {
    if (g_fb) {
        esp_camera_fb_return(g_fb);
        g_fb = nullptr;
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
        logger_printf("[CAM] Deinitialized successfully.");
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
        logger_printf("[CAM] Warming up camera...");
        // Capture and discard a few frames to allow sensor to stabilize
        for (int i = 0; i < 3; ++i) {
            camera_fb_t *warmup_fb = esp_camera_fb_get();
            if (warmup_fb) {
                logger_printf("[CAM] Discarding warmup frame %d.", i + 1);
                esp_camera_fb_return(warmup_fb);
            } else {
                logger_printf("[CAM] WARNING: Failed to get a warmup frame.");
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between captures
        }
        logger_printf("[CAM] Camera warmup complete.");
        xSemaphoreGive(g_camera_mutex);
    }
}
