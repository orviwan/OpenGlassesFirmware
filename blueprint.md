# OpenGlass Firmware Blueprint (2025)

## Overview
- BLE streaming for photo and audio (PCM and Opus)
- Modular, task-based architecture using FreeRTOS
- Optimized for ESP32-S3 (Seeed Xiao ESP32S3 Sense)

## BLE Service/Characteristic Map
| Service/Feature | Characteristic Name      | UUID                                    |
|----------------|-------------------------|------------------------------------------|
| Photo Data     | PHOTO_DATA_UUID         | b7e2f7e0-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| Photo Control  | PHOTO_CONTROL_UUID      | b7e2f7e3-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| PCM Audio      | AUDIO_DATA_UUID         | b7e2f7e1-7e2f-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| Opus Audio     | AUDIO_CODEC_OPUS_UUID   | b7e2f7e2-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| Audio Codec    | AUDIO_CODEC_UUID        | b7e2f7e4-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| Battery Level  | BATTERY_LEVEL_CHAR_UUID | 00002a19-0000-1000-8000-00805f9b34fb     |

## Architecture
- **FreeRTOS Tasks:**
    - Opus audio streaming
    - PCM audio streaming
    - Photo streaming
- **Modular Handlers:**
    - `audio_handler` (mic setup, buffer)
    - `audio_pcm` (PCM streaming)
    - `audio_opus` (Opus streaming)
    - `photo_manager` (photo logic)
- **BLE Optimizations:**
    - MTU 247
    - Fast connection interval (7.5–25ms)
- **Idle Behavior:**
    - Device is idle (no streaming) when no client is connected or notifications are off

## Streaming Logic
- Each streaming type runs in its own FreeRTOS task
- Tasks check connection and notification state before sending data
- All streaming is non-blocking and parallel

## Client Usage
- Subscribe to the desired audio characteristic (PCM or Opus)
- Subscribe to Photo Data for photo streaming
- Use Photo Control to trigger photos (single/interval)
- Only one audio stream should be active at a time
- Opus stream requires Opus decoder on client

## File Map
- `src/ble_handler.cpp` – BLE setup, task creation, connection logic
- `src/audio_handler.cpp` – Mic setup, buffer
- `src/audio_pcm.cpp` – PCM streaming logic
- `src/audio_opus.cpp` – Opus streaming logic
- `src/photo_manager.cpp` – Photo streaming logic

## See README.md for quickstart and UUIDs
