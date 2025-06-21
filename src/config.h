#ifndef CONFIG_H
#define CONFIG_H

#include <BLEUUID.h> // For BLEUUID

// Select camera model (this should ideally be a build flag)
#define CAMERA_MODEL_XIAO_ESP32S3

// ---------------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------------
constexpr int FRAME_SIZE = 80;      // Number of audio samples per frame (for 8kHz, 10ms packets)
constexpr int SAMPLE_RATE = 8000;   // Audio sample rate in Hz (8kHz for I2S)
constexpr int SAMPLE_BITS = 16;     // Audio sample bit depth (16-bit)
constexpr int VOLUME_GAIN = 2;      // Audio volume gain factor (applied as bit shift: e.g., 1 for 2x, 2 for 4x gain)
constexpr size_t AUDIO_FRAME_HEADER_LEN = 3; // Bytes for audio frame header

// ---------------------------------------------------------------------------------
// BLE Service and Characteristic UUIDs
// ---------------------------------------------------------------------------------
// Device Information Service (Standard)
const uint16_t DEVICE_INFORMATION_SERVICE_UUID = 0x180A;
const uint16_t MANUFACTURER_NAME_STRING_CHAR_UUID = 0x2A29;
const uint16_t MODEL_NUMBER_STRING_CHAR_UUID = 0x2A24;
const uint16_t FIRMWARE_REVISION_STRING_CHAR_UUID = 0x2A26;
const uint16_t HARDWARE_REVISION_STRING_CHAR_UUID = 0x2A27;

// Battery Level Service (Standard)
const uint16_t BATTERY_SERVICE_UUID = 0x180F;
const uint16_t BATTERY_LEVEL_CHAR_UUID = 0x2A19;

// Main Custom Service (OpenGlass Service)
static BLEUUID SERVICE_UUID("19b10000-e8f2-537e-4f6c-d104768a1214");
static BLEUUID PHOTO_DATA_UUID("19b10005-e8f2-537e-4f6c-d104768a1214");
static BLEUUID PHOTO_CONTROL_UUID("19b10006-e8f2-537e-4f6c-d104768a1214");
static BLEUUID AUDIO_CODEC_ULAW_UUID("19b10001-e8f2-537e-4f6c-d104768a1214");

// ---------------------------------------------------------------------------------
// BLE Characteristic User Descriptions (UUID 0x2901)
// ---------------------------------------------------------------------------------
constexpr const char* PHOTO_DATA_USER_DESCRIPTION = "Photo JPEG Data Stream";
constexpr const char* PHOTO_CONTROL_USER_DESCRIPTION = "Photo Capture Control";
constexpr const char* AUDIO_ULAW_USER_DESCRIPTION = "μ-law (G.711) Audio Stream";
// Note: Battery Level is a standard characteristic, but a user description can still be added.
constexpr const char* BATTERY_LEVEL_USER_DESCRIPTION = "Device Battery Level";

// ---------------------------------------------------------------------------------
// Device Information Strings
// ---------------------------------------------------------------------------------
constexpr const char* DEVICE_MANUFACTURER_NAME = "Based Hardware";
constexpr const char* DEVICE_MODEL_NUMBER = "OpenGlass";
constexpr const char* DEVICE_FIRMWARE_REVISION = "1.0.4"; // Updated version
constexpr const char* DEVICE_HARDWARE_REVISION = "Seeed Xiao ESP32S3 Sense";

// ---------------------------------------------------------------------------------
// Photo Chunking
// ---------------------------------------------------------------------------------
// This must be large enough to hold the maximum possible payload from a negotiated MTU.
// For a 247-byte MTU, the payload is 244 bytes (247 - 3 bytes for ATT header).
constexpr size_t MAX_PHOTO_CHUNK_PAYLOAD_SIZE = 244;
constexpr size_t PHOTO_CHUNK_HEADER_LEN = 2;
constexpr size_t PHOTO_CHUNK_BUFFER_SIZE = MAX_PHOTO_CHUNK_PAYLOAD_SIZE + PHOTO_CHUNK_HEADER_LEN;

// ---------------------------------------------------------------------------------
// Timings and Intervals (all in milliseconds)
// ---------------------------------------------------------------------------------
constexpr unsigned long BATTERY_UPDATE_INTERVAL_MS = 60000;      // 60 seconds
constexpr unsigned long LOOP_DELAY_MS = 20;                      // Delay at the end of the main loop
constexpr unsigned long DEEP_SLEEP_DISCONNECT_DELAY_MS = 600000; // 10 minutes
constexpr unsigned long DEEP_SLEEP_WAKE_INTERVAL_MS = 10000;     // 10 seconds
constexpr unsigned long DEBUG_LOG_INTERVAL_MS = 10000;           // 10 seconds
constexpr unsigned long PHOTO_INTERVAL_MS = 5000;                // 5 seconds

// ---------------------------------------------------------------------------------
// Pin Definitions
// ---------------------------------------------------------------------------------
#define PIN_LED 21 // single-color orange user LED

// Camera warm-up: number of frames to discard after init (for AWB/gain to settle)
constexpr int CAMERA_WARMUP_FRAMES = 5;

#endif // CONFIG_H