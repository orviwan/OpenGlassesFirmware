# OpenGlass Firmware Blueprint (2025)

## Overview
- BLE streaming for photo and audio (PCM and μ-law/G.711)
- Modular, task-based architecture using FreeRTOS
- Optimized for ESP32-S3 (Seeed Xiao ESP32S3 Sense)

## Architecture
- **FreeRTOS Tasks:**
    - μ-law (G.711) audio streaming
    - Photo streaming
- **Modular Handlers:**
    - `audio_handler` (mic setup, buffer)
    - `audio_ulaw` (μ-law streaming)
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
- Subscribe to the desired audio characteristic (μ-law)
- Subscribe to Photo Data for photo streaming
- Use Photo Control to trigger photos (single/interval)
- Only one audio stream should be active at a time
- μ-law stream requires G.711 μ-law decoder on client

## File Map
- `src/ble_handler.cpp` – BLE setup, task creation, connection logic
- `src/audio_handler.cpp` – Mic setup, buffer
- `src/audio_ulaw.cpp` – μ-law streaming logic
- `src/photo_manager.cpp` – Photo streaming logic

## See README.md for quickstart and UUIDs

| Stream Type     | UUID                                    |
|------------------|-----------------------------------------|
| Photo Data       | 19b10001-e8f2-537e-4f6c-d104768a1214 |
| Photo Control    | 19b10002-e8f2-537e-4f6c-d104768a1214 |
| μ-law Audio      | AUDIO_CODEC_ULAW_UUID                  | 19b10005-e8f2-537e-4f6c-d104768a1214    |
