#ifndef PHOTO_HANDLER_H
#define PHOTO_HANDLER_H

#include "photo_types.h"
#include <cstdint>

// Public function declarations
void initialize_photo_handler();
void start_photo_transfer_task();
void start_interval_photo(uint32_t interval_ms);
void stop_interval_photo();
void photo_sender_task(void *pvParameters);

#endif // PHOTO_HANDLER_H
