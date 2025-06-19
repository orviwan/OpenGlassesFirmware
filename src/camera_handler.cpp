#include <Arduino.h> // For Serial
#include "camera_handler.h"
#include "camera_pins.h"
#include "logger.h"
#include <esp_camera.h>

// Definition of the global frame buffer pointer
camera_fb_t *fb = nullptr;
static bool camera_initialized = false;
SemaphoreHandle_t g_camera_mutex = nullptr; // Mutex for camera access

void initialize_camera_mutex() {
    g_camera_mutex = xSemaphoreCreateMutex();
    if (g_camera_mutex == nullptr) {
        logger_printf("[MUTEX] ERROR: Failed to create camera mutex! Halting.\n");
        while(1);
    } else {
        logger_printf("[MUTEX] Camera mutex created successfully.\n");
    }
}

void configure_camera() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
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
        config.fb_count = 1;                  // Number of frame buffers (1 for single buffer mode)
        config.jpeg_quality = 10;             // JPEG quality (0-63, lower means higher quality, smaller size)
        config.fb_location = CAMERA_FB_IN_PSRAM; // Store frame buffer in PSRAM
        config.grab_mode = CAMERA_GRAB_LATEST;


        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            logger_printf("[CAM] ERROR: Failed to initialize camera! Code: 0x%x\n", err);
            camera_initialized = false;
            xSemaphoreGive(g_camera_mutex);
            return;
        }
        camera_initialized = true;
        logger_printf("[CAM] Camera initialized successfully.\n");

        // Get a reference to the sensor
        xSemaphoreGive(g_camera_mutex);
    }
}

bool take_photo() {
    bool success = false;
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer(); // Ensure previous buffer is released

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

void release_photo_buffer() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (fb) {
            logger_printf("[CAM] Releasing previous frame buffer.\n");
            esp_camera_fb_return(fb);
            fb = nullptr;
        }
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
        release_photo_buffer(); // Ensure buffer is released
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
