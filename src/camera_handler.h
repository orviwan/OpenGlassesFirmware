#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "esp_camera.h" // For camera_fb_t
#include "camera_pins.h" // If camera_pins.h is specific to camera setup
#include <freertos/semphr.h> // For mutex

// Global camera frame buffer pointer - consider encapsulating this
extern camera_fb_t *fb;
extern SemaphoreHandle_t g_camera_mutex; // Mutex to protect camera access
extern SemaphoreHandle_t g_camera_request_semaphore; // Signals the camera task to take a photo
extern volatile bool g_is_photo_ready; // Flag to indicate a photo is ready in the buffer

void initialize_camera_mutex(); // Function to create the mutex
void start_camera_task(); // Function to create the dedicated camera task
void configure_camera();
bool take_photo();
void release_photo_buffer(); // New helper function
void deinit_camera(); // Add deinit function
bool is_camera_initialized();

#endif // CAMERA_HANDLER_H
