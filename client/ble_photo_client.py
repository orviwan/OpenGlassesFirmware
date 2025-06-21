import asyncio
import datetime
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

# --- Configuration ---
# UUIDs should match the ones in your firmware's config.h
DEVICE_NAME = "OpenGlass"
SERVICE_UUID = "19b10000-e8f2-537e-4f6c-d104768a1214"
PHOTO_DATA_UUID = "19b10005-e8f2-537e-4f6c-d104768a1214"
PHOTO_CONTROL_UUID = "19b10006-e8f2-537e-4f6c-d104768a1214"

# Global state
photo_buffer = bytearray()
is_receiving = False
received_frames = set()
last_frame_number = -1

def notification_handler(characteristic: BleakGATTCharacteristic, data: bytearray):
    """Handles incoming data from the photo data characteristic."""
    global photo_buffer, is_receiving, received_frames, last_frame_number

    if not is_receiving:
        return

    header = data[:2]
    payload = data[2:]
    frame_number = int.from_bytes(header, 'little')

    # End-of-Photo marker (0xFFFF)
    if frame_number == 0xFFFF:
        print("\n[CLIENT] End-of-photo marker received.")
        is_receiving = False  # Stop processing further packets

        # Validate the received JPEG data by checking for SOI and EOI markers.
        if len(photo_buffer) > 4 and photo_buffer.startswith(b'\xff\xd8') and photo_buffer.endswith(b'\xff\xd9'):
            print("[CLIENT] JPEG validation successful (SOI and EOI markers found).")
            try:
                timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"received_photo_{timestamp}.jpg"
                with open(filename, "wb") as f:
                    f.write(photo_buffer)
                print(f"\n[SUCCESS] Photo saved as {filename} ({len(photo_buffer)} bytes)")
            except IOError as e:
                print(f"[ERROR] Failed to save photo: {e}")
        else:
            print("\n[ERROR] JPEG validation FAILED. The received data is not a valid image.")
            print(f"  [DEBUG] Total bytes received: {len(photo_buffer)}")
            if len(photo_buffer) > 4:
                print(f"  [DEBUG] Start bytes: {photo_buffer[:2].hex()}")
                print(f"  [DEBUG] End bytes:   {photo_buffer[-2:].hex()}")
        return

    # Regular photo data frame
    if frame_number in received_frames:
        print(f"\n[WARNING] Duplicate frame received: {frame_number}. Discarding.")
        return

    if frame_number != last_frame_number + 1 and last_frame_number != -1:
        print(f"\n[FATAL] PACKET LOSS DETECTED! Expected frame {last_frame_number + 1}, but got {frame_number}.")

    photo_buffer.extend(payload)
    received_frames.add(frame_number)
    last_frame_number = frame_number
    print(f"\r[CLIENT] Receiving chunk {frame_number}... ({len(photo_buffer)} bytes total)", end="")

async def main():
    """Main function to scan, connect, and receive the photo."""
    global is_receiving, photo_buffer, received_frames, last_frame_number

    print(f"Scanning for '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME)

    if not device:
        print(f"Could not find device with name '{DEVICE_NAME}'. Please check if it's advertising.")
        return

    print(f"Found device: {device.name} ({device.address})")

    async with BleakClient(device) as client:
        if not client.is_connected:
            print(f"Failed to connect to {device.address}")
            return

        print(f"Connected to {client.address}")

        # Negotiate a larger MTU for faster transfers.
        try:
            new_mtu = await client.exchange_mtu(247)
            print(f"[CLIENT] MTU successfully negotiated to: {new_mtu} bytes")
        except Exception as e:
            print(f"[CLIENT] MTU negotiation failed: {e}. Using default.")

        # Add a delay for service discovery to complete
        await asyncio.sleep(1.0)

        # Reset state for this run
        photo_buffer.clear()
        received_frames.clear()
        last_frame_number = -1
        is_receiving = False

        try:
            # 1. Enable notifications
            print(f"Enabling notifications for Photo Data ({PHOTO_DATA_UUID})...")
            await client.start_notify(PHOTO_DATA_UUID, notification_handler)
            print("[CLIENT] Notifications enabled.")

            # 2. Request single photo
            print(f"Requesting single photo via Photo Control ({PHOTO_CONTROL_UUID})...")
            is_receiving = True # Set flag to start processing notifications
            await client.write_gatt_char(PHOTO_CONTROL_UUID, b'\xff', response=True) # -1 signed byte
            print("[CLIENT] Photo request sent. Waiting for data...")

            # 3. Wait for the transfer to complete
            while is_receiving:
                await asyncio.sleep(0.1)
            
            await asyncio.sleep(1.0) 

        except Exception as e:
            print(f"An error occurred: {e}")
        finally:
            print("Stopping notifications and disconnecting...")
            try:
                await client.stop_notify(PHOTO_DATA_UUID)
            except Exception as e:
                print(f"Error stopping notifications: {e}")
            print("Disconnected.")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Script stopped by user.")

