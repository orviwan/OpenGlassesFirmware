#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <cstdint>
#include <cstddef>

void start_wifi_hotspot();
void stop_wifi_hotspot();
void send_audio_data_to_wifi_clients(const uint8_t* data, size_t len);

#endif // WIFI_HANDLER_H
