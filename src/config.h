#ifndef CONFIG_H
#define CONFIG_H

#include <BLEUUID.h> // For BLEUUID

// Select camera model (this should ideally be a build flag)
#define CAMERA_MODEL_XIAO_ESP32S3

// ---------------------------------------------------------------------------------
// PCM Audio
// ---------------------------------------------------------------------------------
constexpr int FRAME_SIZE = 160;     // Number of audio samples per frame
constexpr int SAMPLE_RATE = 16000;  // Audio sample rate in Hz (16kHz)
constexpr int SAMPLE_BITS = 16;     // Audio sample bit depth (16-bit)
constexpr int VOLUME_GAIN = 2;      // Audio volume gain factor (applied as bit shift: 1 << 2 = 4x gain)
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

// Main Custom Service ("Friend Service")
static BLEUUID SERVICE_UUID("19B10000-E8F2-537E-4F6C-D104768A1214");
static BLEUUID PHOTO_DATA_UUID("19B10005-E8F2-537E-4F6C-D104768A1214");
static BLEUUID PHOTO_CONTROL_UUID("19B10006-E8F2-537E-4F6C-D104768A1214");
static BLEUUID AUDIO_DATA_UUID("19B10001-E8F2-537E-4F6C-D104768A1214");
static BLEUUID AUDIO_CODEC_UUID("19B10002-E8F2-537E-4F6C-D104768A1214");

// Audio Codec ID
constexpr uint8_t AUDIO_CODEC_ID_PCM_16KHZ_16BIT = 1;

// ---------------------------------------------------------------------------------
// Photo Chunking
// ---------------------------------------------------------------------------------
constexpr size_t MAX_PHOTO_CHUNK_PAYLOAD_SIZE = 200;
constexpr size_t PHOTO_CHUNK_HEADER_LEN = 2;
constexpr size_t PHOTO_CHUNK_BUFFER_SIZE = MAX_PHOTO_CHUNK_PAYLOAD_SIZE + PHOTO_CHUNK_HEADER_LEN;

// ---------------------------------------------------------------------------------
// Timings and Intervals
// ---------------------------------------------------------------------------------
constexpr unsigned long BATTERY_UPDATE_INTERVAL_MS = 60000; // 60 seconds
constexpr unsigned long LOOP_DELAY_MS = 20;                 // Delay at the end of the main loop

#endif // CONFIG_H