#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "esp_camera.h" // For camera_fb_t
#include "camera_pins.h" // If camera_pins.h is specific to camera setup

// Global camera frame buffer pointer - consider encapsulating this
extern camera_fb_t *fb; 

void configure_camera();
bool take_photo();
void release_photo_buffer(); // New helper function

#endif // CAMERA_HANDLER_H
