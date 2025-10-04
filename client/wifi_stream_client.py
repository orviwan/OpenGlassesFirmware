import cv2
import asyncio
import websockets
import pyaudio
import numpy as np
import threading

# --- Configuration ---
GLASSES_IP = "192.168.4.1"  # Default IP address for ESP32 Soft AP
VIDEO_URL = f"http://{GLASSES_IP}/stream"
AUDIO_WS_URL = f"ws://{GLASSES_IP}/audio"

# Audio settings - must match the firmware
SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH = 2  # 2 bytes for 16-bit audio

# --- Audio Streaming Thread ---
async def audio_stream_task():
    p = pyaudio.PyAudio()
    stream = p.open(format=p.get_format_from_width(SAMPLE_WIDTH),
                    channels=CHANNELS,
                    rate=SAMPLE_RATE,
                    output=True)

    print("Connecting to audio WebSocket...")
    try:
        async with websockets.connect(AUDIO_WS_URL) as websocket:
            print("Audio WebSocket connected.")
            while True:
                try:
                    audio_data = await websocket.recv()
                    # The received data is raw bytes, write it directly to the audio stream
                    stream.write(audio_data)
                except websockets.ConnectionClosed:
                    print("Audio WebSocket connection closed.")
                    break
    except Exception as e:
        print(f"Could not connect to audio WebSocket: {e}")
    finally:
        print("Cleaning up audio stream...")
        stream.stop_stream()
        stream.close()
        p.terminate()

def run_audio_thread():
    asyncio.run(audio_stream_task())

# --- Video Streaming ---
def video_stream_task():
    print("Connecting to video stream...")
    cap = cv2.VideoCapture(VIDEO_URL)

    if not cap.isOpened():
        print("Error: Could not open video stream.")
        return

    print("Video stream opened. Displaying feed...")
    while True:
        ret, frame = cap.read()
        if not ret:
            print("End of stream or error reading frame.")
            break

        cv2.imshow('OpenGlasses Stream', frame)

        # Exit on 'q' key press
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    print("Cleaning up video stream...")
    cap.release()
    cv2.destroyAllWindows()

# --- Main ---
if __name__ == "__main__":
    print("Starting OpenGlasses Wi-Fi Client...")

    # Run audio in a separate thread
    audio_thread = threading.Thread(target=run_audio_thread)
    audio_thread.daemon = True
    audio_thread.start()

    # Run video in the main thread
    video_stream_task()

    print("Client shut down.")
