#include <Arduino.h> // For Serial
#include "camera_handler.h"
#include "camera_pins.h"
#include "logger.h"
#include <esp_camera.h>

// Definition of the global frame buffer pointer
camera_fb_t *fb = nullptr;
static bool camera_initialized = false;
SemaphoreHandle_t g_camera_mutex = nullptr; // Mutex for camera access
SemaphoreHandle_t g_camera_request_semaphore = nullptr; // Signals the camera task
volatile bool g_is_photo_ready = false; // Flag indicates a photo is ready in the buffer

// Forward declaration for the internal, non-locking version
static void release_photo_buffer_internal();

// The new dedicated camera task function
void camera_task(void *pvParameters) {
    while (true) {
        // Wait for a signal to take a photo
        if (xSemaphoreTake(g_camera_request_semaphore, portMAX_DELAY) == pdTRUE) {
            logger_printf("[CAM_TASK] Received photo request.\n");

            // Ensure camera is initialized
            if (!is_camera_initialized()) {
                configure_camera();
            }

            // Take the photo
            if (take_photo()) {
                logger_printf("[CAM_TASK] Photo captured successfully. Setting flag.\n");
                g_is_photo_ready = true; // Signal that the photo is ready
            } else {
                logger_printf("[CAM_TASK] Failed to capture photo.\n");
                g_is_photo_ready = false;
                // Optional: de-init camera on failure to try and recover
                deinit_camera();
            }
        }
    }
}

void initialize_camera_mutex() {
    g_camera_mutex = xSemaphoreCreateMutex();
    if (g_camera_mutex == nullptr) {
        logger_printf("[MUTEX] ERROR: Failed to create camera mutex! Halting.\n");
        while(1);
    }

    // Create the semaphore for the camera task
    g_camera_request_semaphore = xSemaphoreCreateBinary();
    if (g_camera_request_semaphore == nullptr) {
        logger_printf("[MUTEX] ERROR: Failed to create camera semaphore! Halting.\n");
        while(1);
    }
    logger_printf("[MUTEX] Camera mutex and semaphore created successfully.\n");
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
    logger_printf("[TASK] Dedicated camera task started.\n");
}

void configure_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (camera_initialized) {
            logger_printf("[CAM] Already initialized.\n");
            xSemaphoreGive(g_camera_mutex);
            return;
        }

        logger_printf("\n");
        logger_printf("[CAM] Initializing...\n");
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
        config.pin_sscb_sda = SIOD_GPIO_NUM;
        config.pin_sscb_scl = SIOC_GPIO_NUM;
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = 20000000;

        // Camera settings
        config.frame_size = FRAMESIZE_SVGA;   // 800x600 resolution
        config.pixel_format = PIXFORMAT_JPEG; // Output format JPEG
        config.jpeg_quality = 10;             // JPEG quality (0-63, lower means higher quality)
        config.fb_location = CAMERA_FB_IN_PSRAM; // Store frame buffer in PSRAM
        config.fb_count = 2;                  // Use 2 frame buffers for stability
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Use WHEN_EMPTY for more stability


        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            logger_printf("[CAM] ERROR: Failed to initialize camera! Code: 0x%x\n", err);
            camera_initialized = false;
            xSemaphoreGive(g_camera_mutex);
            return;
        }
        camera_initialized = true;
        logger_printf("[CAM] Camera initialized successfully.\n");

        // Get a reference to the sensor and apply settings to fix potential init issues
        sensor_t * s = esp_camera_sensor_get();
        if (s) {
            s->reset(s); // Reset the sensor to ensure it's in a known state
            s->set_framesize(s, FRAMESIZE_SVGA);
            s->set_pixformat(s, PIXFORMAT_JPEG);
            logger_printf("[CAM] Sensor reset, framesize and pixformat set post-init.\n");
        } else {
            logger_printf("[CAM] WARNING: Could not get sensor handle post-init.\n");
        }

        // Add a delay for the sensor to settle. This is critical.
        vTaskDelay(pdMS_TO_TICKS(250));

        // Get a reference to the sensor
        xSemaphoreGive(g_camera_mutex);
    }
}

bool take_photo() {
    bool success = false;
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer_internal(); // Ensure previous buffer is released without causing a deadlock

        // Camera warm-up: discard frames to allow AWB/gain to settle
        for (int i = 0; i < CAMERA_WARMUP_FRAMES; ++i) {
            camera_fb_t *tmp_fb = esp_camera_fb_get();
            if (tmp_fb) {
                esp_camera_fb_return(tmp_fb);
            } else {
                logger_printf("[CAM] WARNING: Warm-up frame %d failed.\n", i+1);
            }
        }

        fb = esp_camera_fb_get();
        if (!fb) {
            logger_printf("[CAM] ERROR: Failed to get frame buffer!\n");
            success = false;
        } else {
            logger_printf("[CAM] Photo captured: %zu bytes.\n", fb->len);
            success = true;
        }
        xSemaphoreGive(g_camera_mutex);
    }
    return success;
}

// New internal function that assumes the mutex is already held
static void release_photo_buffer_internal() {
    if (fb) {
        logger_printf("[CAM] Releasing previous frame buffer (internal).\n");
        esp_camera_fb_return(fb);
        fb = nullptr;
    }
}

// The public function still takes the mutex for safe external calls
void release_photo_buffer() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer_internal();
        xSemaphoreGive(g_camera_mutex);
    }
}

void deinit_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (!camera_initialized) {
            logger_printf("[CAM] Camera was not initialized, skipping deinit.\n");
            xSemaphoreGive(g_camera_mutex);
            return;
        }
        // Use the internal function that does NOT lock the mutex
        release_photo_buffer_internal();
        esp_err_t err = esp_camera_deinit();
        if (err == ESP_OK) {
            logger_printf("[CAM] Deinitialized successfully.\n");
        } else {
            logger_printf("[CAM] ERROR: Failed to deinitialize camera! Code: 0x%x\n", err);
        }
        camera_initialized = false;
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
