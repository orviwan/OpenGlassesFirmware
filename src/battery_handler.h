#ifndef BATTERY_HANDLER_H
#define BATTERY_HANDLER_H

#include <stdint.h> // For uint8_t

// Forward declaration
class BLECharacteristic;

extern uint8_t g_battery_level_percent;
extern unsigned long g_last_battery_update_ms;

void initialize_battery_handler(BLECharacteristic *ble_char);
void update_battery_level();
// void process_battery_update(); // This logic will be in loop() checking interval


#endif // BATTERY_HANDLER_H