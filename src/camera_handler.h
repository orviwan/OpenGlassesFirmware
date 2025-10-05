#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "photo_types.h"
#include "esp_camera.h"
#include <cstddef> // For size_t
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Extern declarations for global camera variables
extern camera_fb_t *fb;
extern SemaphoreHandle_t g_photo_ready_semaphore;

// Function declarations
void initialize_camera_mutex_and_task();
void start_camera_task();
void request_photo();
camera_fb_t* get_photo_buffer();
void configure_camera();
bool take_photo();
void release_photo_buffer();
void deinit_camera();
bool is_camera_initialized();
void warm_up_camera();

#endif // CAMERA_HANDLER_H
