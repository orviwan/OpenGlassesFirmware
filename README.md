# OpenGlass Firmware - BLE Streaming Architecture (2025)

## Features
- BLE Device Name: **OpenGlass**
- Main BLE Service UUID (advertised): `a1b20000-7c4d-4e2a-9f1b-1234567890ab`
- BLE photo streaming (single-shot and interval)
- BLE audio streaming: μ-law (G.711)
- Only μ-law audio streaming is supported; PCM has been removed for simplicity and efficiency.
- Modular audio handler structure (audio_handler, audio_ulaw)
- FreeRTOS task offloading for μ-law and photo streaming
- Optimized BLE throughput (MTU 247, fast connection interval)
- Battery and device info services

## BLE Characteristics (UUIDs)
| Feature         | Characteristic Name      | UUID                                    |
|----------------|-------------------------|------------------------------------------|
| Photo Data     | PHOTO_DATA_UUID         | a1b20001-7c4d-4e2a-9f1b-1234567890ab     |
| Photo Control  | PHOTO_CONTROL_UUID      | a1b20002-7c4d-4e2a-9f1b-1234567890ab     |
| μ-law Audio    | AUDIO_CODEC_ULAW_UUID   | a1b20003-7c4d-4e2a-9f1b-1234567890ab     |

## How to Use
- Subscribe to the desired audio characteristic (μ-law) for streaming.
- Photo streaming is controlled via the Photo Control characteristic.
- All streaming is handled by FreeRTOS tasks for maximum efficiency.
- **Audio and photo streaming can run simultaneously, but BLE bandwidth is shared. For best reliability, avoid high-frequency photo capture during audio streaming.**

## How to Use μ-law (G.711) Audio Streaming

To receive μ-law (G.711) audio from the OpenGlass device over BLE:

1. Connect to the device using BLE and discover its characteristics.
2. Locate the μ-law Audio Characteristic:
   - **Name:** AUDIO_CODEC_ULAW_UUID
   - **UUID:** a1b20003-7c4d-4e2a-9f1b-1234567890ab
3. Subscribe to notifications on this characteristic.
4. Each notification will contain a frame of G.711 μ-law encoded audio (one byte per sample, 8kHz sample rate).
5. On the client side, decode the μ-law data to PCM using a G.711 μ-law decoder (available in most audio libraries, e.g., WebAudio, Python, ffmpeg, etc.).
6. Play or process the decoded PCM audio as needed.

**Note:** Audio and photo can be streamed at the same time, but BLE bandwidth is shared. For best reliability, avoid high-frequency photo capture during audio streaming.

Example (Python, using `pyaudio` and `audioop`):
```python
import audioop
# ulaw_data = ... (received from BLE notification)
pcm_data = audioop.ulaw2lin(ulaw_data, 2)  # 2 bytes/sample for 16-bit PCM
```

## Performance
- BLE MTU set to 247 for high throughput
- Connection interval set to 7.5ms–25ms
- All streaming (audio/photo) is offloaded to FreeRTOS tasks
- Camera and microphone are de-initialized when no client is connected to save power and reduce heat. They are re-initialized upon client connection.
- Device advertises when no client is connected.

## Client Instructions
- Connect to the BLE device named "OpenGlass". Alternatively, scan for devices advertising the Main BLE Service UUID (`a1b20000-7c4d-4e2a-9f1b-1234567890ab`) for discovery.
- Subscribe to the μ-law Audio Characteristic for G.711 μ-law audio (decoder required)
- Subscribe to the Photo Data Characteristic for photo streaming
- Use the Photo Control Characteristic to trigger single-shot or interval photos

### Detailed Photo Streaming Instructions

**1. Photo Control (`PHOTO_CONTROL_UUID`: `a1b20002-7c4d-4e2a-9f1b-1234567890ab`) - Client Writes**

To control photo capture, your client application should write a single byte to this characteristic:

*   **`-1` (or `0xFF` if sending as `uint8_t`): Trigger Single Photo**
    *   The device will capture and send one photo.
*   **`0`: Stop Capture**
    *   Stops any ongoing interval capture.
*   **`5` to `127`: Start Interval Capture (Interval in Seconds)**
    *   The device will capture photos at the specified interval (e.g., a value of `10` means every 10 seconds).
    *   The firmware enforces a minimum interval of 5 seconds.

**2. Photo Data (`PHOTO_DATA_UUID`: `a1b20001-7c4d-4e2a-9f1b-1234567890ab`) - Device Sends Notifications**

Once photo capture is triggered, the device sends the JPEG image data in chunks via BLE notifications. Your client should:

1.  **Subscribe to Notifications:** Enable notifications for this characteristic.
2.  **Reassemble Chunks:** Each notification packet has the following structure:
    *   **Data Chunks:**
        *   Header (2 bytes): A 16-bit chunk counter (little-endian). This counter starts at `0` for the first chunk of an image and increments for each subsequent data chunk of that image.
        *   Payload (up to 200 bytes): Raw JPEG image data bytes.
    *   Collect these chunks and append the payload data in the order indicated by the chunk counter.
3.  **Detect End-of-Photo:**
    *   When all image data for a photo has been sent, the device will send a special 2-byte marker:
        *   `Byte 0`: `0xFF`
        *   `Byte 1`: `0xFF`
    *   This signifies that the current photo transmission is complete.
4.  **Verify CRC32 Checksum:**
    *   Immediately following the End-of-Photo marker, the device sends one more special chunk containing a CRC32 checksum for the entire original JPEG image:
        *   Header (2 bytes): `0xFE 0xFE`
        *   Payload (4 bytes): The 32-bit CRC32 checksum, sent in little-endian byte order.
    *   Your client should calculate the CRC32 of the reassembled JPEG data and compare it against this received value. If they match, the image was received correctly. If not, the image may be corrupted.

## Advanced
- Modular code: see `src/audio_handler.cpp`, `audio_ulaw.cpp`, `photo_manager.cpp`
- All streaming logic is non-blocking and runs in parallel tasks
- See `blueprint.md` for detailed architecture
