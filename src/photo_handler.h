#ifndef PHOTO_HANDLER_H
#define PHOTO_HANDLER_H

#include <cstdint>

void initialize_photo_handler();
void start_photo_transfer_task();
void photo_sender_task(void *pvParameters);
void start_interval_photo(uint32_t interval_ms);
void stop_interval_photo();

#endif // PHOTO_HANDLER_H
