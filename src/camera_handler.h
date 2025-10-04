#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "esp_camera.h"
#include <cstddef> // For size_t

void configure_camera();
bool take_photo(uint8_t** pic_buf, size_t* pic_len);
void release_photo_buffer();
void deinit_camera();
bool is_camera_initialized();

#endif // CAMERA_HANDLER_H
