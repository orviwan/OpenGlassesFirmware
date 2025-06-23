import asyncio
import struct
import time
import wave
from datetime import datetime
from bleak import BleakClient, BleakScanner

# --- Pure Python u-law decoder ---
# This is used as a fallback if the 'audioop' module is not available.
# The table is from the CPython source code for the 'audioop' module.
_ulaw2lin_table = [
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364, -9852,  -9340,  -8828,  -8316,
    -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
    -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
    -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
    -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
    -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
    -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
    -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
    -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
    -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
    -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
    -120,   -112,   -104,   -96,    -88,    -80,    -72,    -64,
    -56,    -48,    -40,    -32,    -24,    -16,    -8,     0,
    32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
    23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
    15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
    11900,  11388,  10876,  10364,  9852,   9340,   8828,   8316,
    7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
    5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
    3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
    2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
    1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
    1372,   1308,   1244,   1180,   1116,   1052,   988,    924,
    876,    844,    812,    780,    748,    716,    684,    652,
    620,    588,    556,    524,    492,    460,    428,    396,
    372,    356,    340,    324,    308,    292,    276,    260,
    244,    228,    212,    196,    180,    164,    148,    132,
    120,    112,    104,    96,     88,     80,     72,     64,
    56,     48,     40,     32,     24,     16,     8,      0,
]

def ulaw2lin(data, width):
    """
    Decode a u-law encoded byte string to a linear PCM byte string.
    This is a pure Python implementation.
    """
    if width != 2:
        raise NotImplementedError("Only 16-bit (width=2) output is supported")
    
    # Use a list comprehension and struct.pack for efficient conversion
    pcm_samples = [_ulaw2lin_table[ulaw_byte] for ulaw_byte in data]
    return struct.pack(f'<{len(pcm_samples)}h', *pcm_samples)

# --- Configuration ---
DEVICE_NAME = "OpenGlass"
AUDIO_ULAW_UUID = "19b10001-e8f2-537e-4f6c-d104768a1214"

# Audio parameters for the output WAV file
SAMPLE_RATE = 16000
SAMPLE_WIDTH = 2  # 16-bit PCM
CHANNELS = 1
CAPTURE_DURATION_S = 10  # seconds

# --- Global State ---
stats = {}
audio_data = []
total_bytes_received = 0

def notification_handler(sender, data):
    """Handles incoming BLE notifications, decodes u-law data, and appends it."""
    global total_bytes_received

    # Decode the u-law data to 16-bit PCM and append it
    pcm_data = ulaw2lin(data, SAMPLE_WIDTH)
    audio_data.append(pcm_data)

    # Update and display progress
    total_bytes_received += len(data)
    print(f"\r[CLIENT] Receiving audio... ({total_bytes_received} bytes received)", end="")

async def main():
    """Main function to scan, connect, and record audio for a fixed duration."""
    global stats

    print(f"Scanning for '{DEVICE_NAME}'...")
    scan_start_time = time.monotonic()
    device = await BleakScanner.find_device_by_name(DEVICE_NAME)
    scan_end_time = time.monotonic()
    stats['scan_time_s'] = scan_end_time - scan_start_time

    if not device:
        print(f"\nCould not find '{DEVICE_NAME}'. Please ensure it is advertising.")
        return

    print(f"\nFound device: {device.name} ({device.address})")

    connect_start_time = time.monotonic()
    async with BleakClient(device.address) as client:
        connect_end_time = time.monotonic()
        stats['connect_time_s'] = connect_end_time - connect_start_time

        if not client.is_connected:
            print("[ERROR] Failed to connect to the device.")
            return

        print(f"Connected. Capturing audio for {CAPTURE_DURATION_S} seconds...")
        
        download_start_time = time.monotonic()
        await client.start_notify(AUDIO_ULAW_UUID, notification_handler)
        await asyncio.sleep(CAPTURE_DURATION_S)
        await client.stop_notify(AUDIO_ULAW_UUID)
        download_end_time = time.monotonic()

        # --- Calculate download stats ---
        duration = download_end_time - download_start_time
        # The final size is the decoded PCM data
        final_pcm_data = b''.join(audio_data)
        size_bytes = len(final_pcm_data)

        stats['download_duration_s'] = duration
        stats['file_size_kb'] = size_bytes / 1024
        if duration > 0:
            stats['transfer_speed_kbps'] = (size_bytes / 1024) / duration
        else:
            stats['transfer_speed_kbps'] = 0

def save_wav_file():
    """Saves the collected audio data to a WAV file."""
    if not audio_data:
        print("\n[INFO] No audio data was captured, cannot save file.")
        return

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"audio_capture_{timestamp}.wav"
    full_audio_data = b''.join(audio_data)

    try:
        with wave.open(filename, 'wb') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(SAMPLE_WIDTH)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(full_audio_data)
        print(f"\n[SUCCESS] Audio saved as {filename} ({len(full_audio_data)} bytes)")
    except Exception as e:
        print(f"\n[ERROR] Failed to save WAV file: {e}")

def print_summary():
    """Prints a formatted summary of the connection and transfer stats."""
    print("\n\n--- Connection and Download Summary ---")
    print(f"Scan time:              {stats.get('scan_time_s', 0):.2f} s")
    print(f"Connect time:           {stats.get('connect_time_s', 0):.2f} s")
    print(f"Download duration:      {stats.get('download_duration_s', 0):.2f} s")
    print(f"Total file size:        {stats.get('file_size_kb', 0):.2f} KB")
    print(f"Transfer speed:         {stats.get('transfer_speed_kbps', 0):.2f} KB/s")
    print("-------------------------------------")

if __name__ == "__main__":
    print("-" * 50)
    print("--- OpenGlass BLE Audio Test Client ---")
    print("-------------------------------------")
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"\n[ERROR] An error occurred during the process: {e}")
    finally:
        if audio_data:
            save_wav_file()
            print_summary()
        else:
            print("\n[INFO] No data was received. Exiting.")

