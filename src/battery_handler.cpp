#include "battery_handler.h"
#include "config.h"
#include "logger.h"
#include "ble_handler.h" // For g_is_ble_connected
#include <Arduino.h>
#include <NimBLEDevice.h>

// Global battery state variables
uint8_t g_battery_level_percent = 100;
unsigned long g_last_battery_update_ms = 0;

BLECharacteristic *g_battery_level_characteristic_ptr = nullptr;

void initialize_battery_handler(BLECharacteristic *ble_char) {
    g_battery_level_characteristic_ptr = ble_char;
    g_last_battery_update_ms = millis(); 
    logger_printf("[BATT] Battery handler initialized.\n");
    update_battery_level(); // Set initial value
}

void update_battery_level() {
    // Placeholder for actual battery reading logic
    // For now, we'll just use the static value
    
    if (g_battery_level_characteristic_ptr) {
        g_battery_level_characteristic_ptr->setValue(&g_battery_level_percent, 1);
        if (g_is_ble_connected) {
            g_battery_level_characteristic_ptr->notify();
        }
    }
    g_last_battery_update_ms = millis();
}