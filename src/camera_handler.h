#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "esp_camera.h"
#include <cstddef> // For size_t

// Extern declarations for global camera variables
extern camera_fb_t *fb;
extern volatile bool g_is_photo_ready;

// Enum for photo quality modes
typedef enum {
    PHOTO_MODE_HIGH_QUALITY, // Best quality, largest size
    PHOTO_MODE_MEDIUM_QUALITY, // Good balance
    PHOTO_MODE_FAST_TRANSFER   // Lower quality, smaller size
} photo_mode_t;

// Function declarations
void initialize_camera_mutex_and_task();
void start_camera_task();
void request_photo(photo_mode_t mode);
bool is_photo_ready();
camera_fb_t* get_photo_buffer();
void configure_camera();
bool take_photo(photo_mode_t mode);
void release_photo_buffer();
void deinit_camera();
bool is_camera_initialized();
void warm_up_camera();

#endif // CAMERA_HANDLER_H
