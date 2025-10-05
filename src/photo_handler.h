#ifndef PHOTO_HANDLER_H
#define PHOTO_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "camera_handler.h"

extern SemaphoreHandle_t g_photo_request_semaphore;

void initialize_photo_handler();
void start_photo_transfer_task(photo_mode_t mode);
void photo_sender_task(void *pvParameters);
void start_interval_photo(uint32_t interval_ms);
void stop_interval_photo();

#endif // PHOTO_HANDLER_H
