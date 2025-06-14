# OpenGlass Firmware Blueprint (2025)

## Overview
- BLE Device Name: **OpenGlass**
- Main BLE Service UUID (advertised): `a1b20000-7c4d-4e2a-9f1b-1234567890ab`
- BLE streaming for photo and audio (μ-law/G.711)
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
    - Camera and microphone are de-initialized when no client is connected to save power and reduce heat.
    - Peripherals are re-initialized upon client connection.
    - Device advertises when no client is connected.

## Streaming Logic
- Each streaming type runs in its own FreeRTOS task
- Tasks check connection and notification state before sending data
- All streaming is non-blocking and parallel
- **Audio and photo streaming can run simultaneously, but BLE bandwidth is shared.**
- For best reliability, avoid high-frequency photo capture during audio streaming.

## Client Usage
- Connect to the BLE device named "OpenGlass", or scan for devices advertising the Main BLE Service UUID (`a1b20000-7c4d-4e2a-9f1b-1234567890ab`).
- Subscribe to the desired audio characteristic (μ-law)
- Subscribe to Photo Data for photo streaming
- Use Photo Control to trigger photos (single/interval)
- **Audio and photo can be streamed at the same time, but BLE bandwidth is shared.**
- μ-law stream requires G.711 μ-law decoder on client

## File Map
- `src/ble_handler.cpp` – BLE setup, task creation, connection logic
- `src/audio_handler.cpp` – Mic setup, buffer
- `src/audio_ulaw.cpp` – μ-law streaming logic
- `src/photo_manager.cpp` – Photo streaming logic

## See README.md for quickstart and UUIDs

| Stream Type      | Identifier              | UUID                                     |
|------------------|--------------------------------------------------------------------|
| Photo Data       | PHOTO_DATA_UUID         | a1b20001-7c4d-4e2a-9f1b-1234567890ab     |
| Photo Control    | PHOTO_CONTROL_UUID      | a1b20002-7c4d-4e2a-9f1b-1234567890ab     |
| μ-law Audio      | AUDIO_CODEC_ULAW_UUID   | a1b20003-7c4d-4e2a-9f1b-1234567890ab     |
