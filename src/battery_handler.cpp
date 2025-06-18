#include "battery_handler.h"
#include "config.h"
#include "logger.h"
#include <Arduino.h> // For millis()
#include <BLECharacteristic.h>

// Define global battery state variables here
uint8_t g_battery_level_percent = 100; // Default to 100%
unsigned long g_last_battery_update_ms = 0;

BLECharacteristic *g_battery_level_characteristic_ptr = nullptr; // Pointer to be set during BLE init

void initialize_battery_handler(BLECharacteristic *ble_char) {
    g_battery_level_characteristic_ptr = ble_char;
    // Initialize g_last_battery_update_ms to ensure first update happens correctly
    g_last_battery_update_ms = millis(); 
    logger_printf("[BATT] Battery handler initialized.");
}

void update_battery_level() {
    // TODO: Implement actual battery level reading logic here.
    // https://wiki.seeedstudio.com/check_battery_voltage/
    if (g_battery_level_characteristic_ptr) {
        g_battery_level_characteristic_ptr->setValue(&g_battery_level_percent, 1);
        g_battery_level_characteristic_ptr->notify();
    }
    logger_printf("[BATT] Level updated (static value).");
    g_last_battery_update_ms = millis();
}