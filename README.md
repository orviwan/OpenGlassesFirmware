# OpenGlass Firmware - BLE Streaming Architecture (2025)

## Features
- BLE photo streaming (single-shot and interval)
- BLE audio streaming: PCM and μ-law (G.711, client-selectable)
- Modular audio handler structure (audio_handler, audio_pcm, audio_ulaw)
- FreeRTOS task offloading for PCM, μ-law, and photo streaming
- Optimized BLE throughput (MTU 247, fast connection interval)
- Battery and device info services

## BLE Characteristics (UUIDs)
| Feature         | Characteristic Name      | UUID                                    |
|----------------|-------------------------|------------------------------------------|
| Photo Data     | PHOTO_DATA_UUID         | b7e2f7e0-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| Photo Control  | PHOTO_CONTROL_UUID      | b7e2f7e3-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| PCM Audio      | AUDIO_DATA_UUID         | b7e2f7e1-7e2f-7e2f-7e2f-b7e2f7e2f7e2     |
| μ-law Audio    | AUDIO_CODEC_ULAW_UUID   | 19b10005-e8f2-537e-4f6c-d104768a1214    |

## How to Use
- Subscribe to the desired audio characteristic (PCM or μ-law) for streaming.
- Only one audio stream should be active at a time for best performance.
- Photo streaming is controlled via the Photo Control characteristic.
- All streaming is handled by FreeRTOS tasks for maximum efficiency.

## Performance
- BLE MTU set to 247 for high throughput
- Connection interval set to 7.5ms–25ms
- All streaming (audio/photo) is offloaded to FreeRTOS tasks
- Device is idle when no client is connected or notifications are disabled

## Client Instructions
- Subscribe to the μ-law Audio Characteristic for G.711 μ-law audio (decoder required)
- Subscribe to the PCM Audio Characteristic for raw PCM audio
- Subscribe to the Photo Data Characteristic for photo streaming
- Use the Photo Control Characteristic to trigger single-shot or interval photos

## Advanced
- Modular code: see `src/audio_handler.cpp`, `audio_pcm.cpp`, `audio_ulaw.cpp`, `photo_manager.cpp`
- All streaming logic is non-blocking and runs in parallel tasks
- See `blueprint.md` for detailed architecture
