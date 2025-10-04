#include "camera_handler.h"
#include "logger.h"
#include "camera_pins.h"
#include "state_handler.h"
#include <Arduino.h>

static camera_fb_t *g_frame_buffer = NULL;
static bool g_is_camera_initialized = false;

void configure_camera() {
    if (g_is_camera_initialized) {
        log_message("Camera already initialized.");
        return;
    }

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
    config.frame_size = FRAMESIZE_UXGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 2;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        log_message("Camera init failed with error 0x%x", err);
        g_is_camera_initialized = false;
        return;
    }

    g_is_camera_initialized = true;
    log_message("Camera initialized successfully.");
}

bool take_photo(uint8_t** pic_buf, size_t* pic_len) {
    if (!g_is_camera_initialized) {
        log_message("Camera not initialized, cannot take photo.");
        return false;
    }

    if (g_frame_buffer != NULL) {
        release_photo_buffer();
    }

    g_frame_buffer = esp_camera_fb_get();
    if (!g_frame_buffer) {
        log_message("Camera frame buffer get failed.");
        return false;
    }

    *pic_buf = g_frame_buffer->buf;
    *pic_len = g_frame_buffer->len;
    
    return true;
}

void release_photo_buffer() {
    if (g_frame_buffer != NULL) {
        esp_camera_fb_return(g_frame_buffer);
        g_frame_buffer = NULL;
    }
}

void deinit_camera() {
    if (g_is_camera_initialized) {
        esp_camera_deinit();
        g_is_camera_initialized = false;
        log_message("Camera de-initialized.");
    }
}

bool is_camera_initialized() {
    return g_is_camera_initialized;
}
