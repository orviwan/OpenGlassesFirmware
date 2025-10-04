#ifndef PHOTO_HANDLER_H
#define PHOTO_HANDLER_H

#include <cstdint>

void take_and_send_photo();
void start_interval_photo(uint32_t interval_ms);
void stop_interval_photo();

#endif // PHOTO_HANDLER_H
