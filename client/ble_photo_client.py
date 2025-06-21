import asyncio
import zlib
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
total_frames_expected = -1 # We don't know this beforehand
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
        return # Wait for the CRC frame

    # CRC32 marker (0xFEFE)
    if frame_number == 0xFEFE:
        is_receiving = False # Stop processing further packets
        received_crc = int.from_bytes(payload, 'little')
        print(f"\n[CLIENT] CRC32 marker received. Device-side CRC: 0x{received_crc:08X}")
        
        # Verify CRC using the standard zlib library, which is compatible
        # with the ESP32's hardware CRC32 implementation.
        calculated_crc = zlib.crc32(photo_buffer)
        print(f"[CLIENT] Calculated client-side CRC32: 0x{calculated_crc:08X}")

        if received_crc == calculated_crc:
            print("[CLIENT] CRC32 checksum MATCHES. Image is valid.")
            try:
                with open("received_photo.jpg", "wb") as f:
                    f.write(photo_buffer)
                print(f"\n[SUCCESS] Photo saved as received_photo.jpg ({len(photo_buffer)} bytes)")
            except IOError as e:
                print(f"[ERROR] Failed to save photo: {e}")
        else:
            print("[ERROR] CRC32 checksum MISMATCH. Image is corrupt.")
        return

    # Regular photo data frame
    if frame_number not in received_frames:
        # This simple logic assumes frames arrive in order.
        # A more robust client would handle out-of-order frames.
        if frame_number != last_frame_number + 1 and last_frame_number != -1:
            print(f"\n[WARNING] Missed frame! Expected {last_frame_number + 1}, got {frame_number}")

        photo_buffer.extend(payload)
        received_frames.add(frame_number)
        last_frame_number = frame_number
        print(f"\r[CLIENT] Receiving chunk {frame_number}... ({len(photo_buffer)} bytes)", end="")

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

        # Add a delay for service discovery to complete
        print("Waiting for service discovery...")
        await asyncio.sleep(2.0)

        # DEBUG: List all services and characteristics
        print("--- Discovered Services and Characteristics ---")
        for service in client.services:
            print(f"[Service] {service.uuid}: {service.description}")
            for char in service.characteristics:
                print(f"  [Characteristic] {char.uuid}: {char.description}, Properties: {char.properties}")
        print("---------------------------------------------")

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
            # The notification handler will set is_receiving to False when done
            while is_receiving:
                await asyncio.sleep(0.1)
            
            # Add a small delay to ensure the final messages are processed
            await asyncio.sleep(1.0) 

        except Exception as e:
            print(f"An error occurred: {e}")
        finally:
            print("Stopping notifications and disconnecting...")
            try:
                await client.stop_notify(PHOTO_DATA_UUID)
            except Exception as e:
                print(f"Error stopping notifications: {e}") # Can happen if already disconnected
            print("Disconnected.")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Script stopped by user.")

