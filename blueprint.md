# OpenGlass Firmware Blueprint (2025)

## Overview
- BLE Device Name: **OpenGlass**
- Main BLE Service UUID (advertised): `19B10000-E8F2-537E-4F6C-D104768A1214`
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
- Connect to the BLE device named "OpenGlass", or scan for devices advertising the Main BLE Service UUID (`19B10000-E8F2-537E-4F6C-D104768A1214`).
- Subscribe to the desired audio characteristic (μ-law)
- Subscribe to Photo Data for photo streaming
- Use Photo Control to trigger photos (single/interval)
- **Audio and photo can be streamed at the same time, but BLE bandwidth is shared.**
- μ-law stream requires G.711 μ-law decoder on client

### Photo Control (`PHOTO_CONTROL_UUID`: `19B10006-E8F2-537E-4F6C-D104768A1214`) - Write
Client writes a single byte to control photo capture:
- `-1` (or `0xFF`): Trigger a single photo.
- `0`: Stop any ongoing capture (interval or pending single shot).
- `5` to `127`: Start interval capture. The value is the interval in seconds (e.g., `5` for 5 seconds). Minimum 5 seconds.

### Photo Data (`PHOTO_DATA_UUID`: `19B10005-E8F2-537E-4F6C-D104768A1214`) - Notify
Device sends JPEG image data in chunks via notifications:
1.  **Data Chunks:**
    *   Header (2 bytes): 16-bit chunk counter (little-endian, starts at 0).
    *   Payload (up to 200 bytes): JPEG image data.
2.  **End-of-Photo Marker (2 bytes):**
    *   Header: `0xFF 0xFF`. Signals image completion.
3.  **CRC32 Checksum (6 bytes total, sent after End-of-Photo):**
    *   Header (2 bytes): `0xFE 0xFE`.
    *   Payload (4 bytes): 32-bit CRC32 of the entire JPEG image (little-endian). Client should verify this.

## See README.md for quickstart and UUIDs

| Stream Type      | Identifier              | UUID                                     |
|------------------|--------------------------------------------------------------------|
| Photo Data       | PHOTO_DATA_UUID         | 19B10005-E8F2-537E-4F6C-D104768A1214     |
| Photo Control    | PHOTO_CONTROL_UUID      | 19B10006-E8F2-537E-4F6C-D104768A1214     |
| μ-law Audio      | AUDIO_CODEC_ULAW_UUID   | 19B10001-E8F2-537E-4F6C-D104768A1214     |


## File Map
- `src/ble_handler.cpp` – BLE setup, task creation, connection logic
- `src/audio_handler.cpp` – Mic setup, buffer
- `src/audio_ulaw.cpp` – μ-law streaming logic
- `src/photo_manager.cpp` – Photo streaming logic