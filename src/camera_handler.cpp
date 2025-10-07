#include "camera_handler.h"
#include "photo_types.h"
#include "logger.h"
#include "camera_pins.h"
#include "state_handler.h"
#include <esp_camera.h>

// Task handle for the camera task
static TaskHandle_t g_camera_task_handle = NULL;

// Flag to signal the camera task to take a photo
volatile bool g_camera_task_request_flag = false;

// Semaphore to signal that the photo is ready
SemaphoreHandle_t g_photo_ready_semaphore = NULL;

// Definition of the global frame buffer pointer
static camera_fb_t *g_fb = nullptr;
static bool camera_initialized = false;
SemaphoreHandle_t g_camera_mutex = nullptr; // Mutex for camera access
SemaphoreHandle_t g_camera_request_semaphore = nullptr; // Signals the camera task

// Forward declaration for the internal, non-locking version
static void release_photo_buffer_internal();

// The dedicated camera task function
void camera_task(void *pvParameters) {
    while (true) {
        // Wait for a notification to take a photo
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (g_camera_task_request_flag) {
            logger_printf("[CAM_TASK] Received photo request.");
            
            // Set camera to a single, default high-quality mode
            sensor_t *s = esp_camera_sensor_get();
            if (!s) {
                logger_printf("[CAM] ERROR: Could not get sensor in camera_task. Has init failed?");
                g_fb = nullptr;
                xSemaphoreGive(g_photo_ready_semaphore); // Signal failure to photo_sender
                g_camera_task_request_flag = false;
                continue; // Skip the rest of the loop
            }
            logger_printf("[CAM] Setting quality: 15");
            s->set_quality(s, 15);

            // Give the sensor a moment to apply new settings
            vTaskDelay(pdMS_TO_TICKS(250));

            camera_fb_t *fb = esp_camera_fb_get();

            if (!fb) {
                logger_printf("[CAM] ERROR: Failed to get a frame.");
                g_fb = nullptr;
            } else {
                logger_printf("[CAM] Photo captured: %zu bytes.", fb->len);
                g_fb = fb; // Store the final, correct frame buffer
            }
            xSemaphoreGive(g_photo_ready_semaphore); // Signal that the photo is ready (or failed)

            g_camera_task_request_flag = false; // Reset the request flag
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

    g_photo_ready_semaphore = xSemaphoreCreateBinary();
    if (g_photo_ready_semaphore == nullptr) {
        logger_printf("[MUTEX] ERROR: Failed to create photo ready semaphore! Halting.");
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
        &g_camera_task_handle, // Task handle
        1);                   // Core where the task should run
    logger_printf("[TASK] Dedicated camera task started.");
}

void request_photo() {
    if (g_camera_task_handle == NULL) {
        logger_printf("[CAM] ERROR: Camera task not initialized.");
        return;
    }
    logger_printf("[CAM] Photo request received.");
    g_camera_task_request_flag = true;
    xTaskNotifyGive(g_camera_task_handle);
}

camera_fb_t* get_photo_buffer() {
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        if (g_fb) {
            xSemaphoreGive(g_camera_mutex);
            return g_fb;
        }
        xSemaphoreGive(g_camera_mutex);
    }
    return nullptr;
}

void configure_camera() {
    // Add a delay to allow the camera power to stabilize before initialization
    vTaskDelay(pdMS_TO_TICKS(250));

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

        config.frame_size = FRAMESIZE_UXGA;
        config.pixel_format = PIXFORMAT_JPEG;
        config.jpeg_quality = 15; // Default quality
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            logger_printf("[CAM] ERROR: Failed to initialize camera! Code: 0x%x", err);
            camera_initialized = false;
            xSemaphoreGive(g_camera_mutex);
            while (1) { vTaskDelay(1000); } // Halt on critical error
        }
        camera_initialized = true;
        logger_printf("[CAM] Camera initialized successfully (UXGA buffer).");

        sensor_t * s = esp_camera_sensor_get();
        if (s) {
            s->set_pixformat(s, PIXFORMAT_JPEG);
        } else {
            logger_printf("[CAM] WARNING: Could not get sensor handle post-init.");
        }

        vTaskDelay(pdMS_TO_TICKS(250));
        xSemaphoreGive(g_camera_mutex);
    }
}

bool take_photo() {
    bool success = false;
    if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY)) {
        release_photo_buffer_internal();

        sensor_t * s = esp_camera_sensor_get();
        if (!s) {
            logger_printf("[CAM] ERROR: Could not get sensor handle!");
            xSemaphoreGive(g_camera_mutex);
            return false;
        }

        // Configure sensor to default settings
        logger_printf("[CAM] Setting single photo mode (UXGA, Q15)");
        s->set_framesize(s, FRAMESIZE_UXGA);
        s->set_quality(s, 15);

        
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