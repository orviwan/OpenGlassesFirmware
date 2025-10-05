import asyncio
import logging
from bleak import BleakClient, BleakScanner
from datetime import datetime

# Setup basic logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# --- Configuration ---
DEVICE_NAME = "OpenGlass"
PHOTO_CONTROL_UUID = "19b10006-e8f2-537e-4f6c-d104768a1214"
PHOTO_DATA_UUID = "19b10005-e8f2-537e-4f6c-d104768a1214"

# --- Global State ---
photo_data = bytearray()
transfer_complete = asyncio.Event()
last_sequence_number = -1

def notification_handler(sender, data: bytearray):
    """Handles incoming data from the photo data characteristic."""
    global photo_data, last_sequence_number

    # Check for the end-of-transfer marker (0xFFFF)
    if len(data) == 2 and data == b'\xff\xff':
        logger.info("End-of-transfer marker received.")
        transfer_complete.set()
        return

    if len(data) < 2:
        logger.warning(f"Received a runt packet of size {len(data)}. Ignoring.")
        return

    # Extract sequence number (little-endian) and payload
    sequence_number = int.from_bytes(data[:2], 'little')
    payload = data[2:]

    if last_sequence_number != -1 and sequence_number != last_sequence_number + 1:
        logger.warning(f"Missed a packet! Expected sequence {last_sequence_number + 1}, but got {sequence_number}.")

    last_sequence_number = sequence_number
    photo_data.extend(payload)
    logger.debug(f"Received chunk {sequence_number}, payload size: {len(payload)}, total size: {len(photo_data)}")

async def main():
    """Main function to connect, take a photo, and save it."""
    logger.info(f"Scanning for '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME)
    if not device:
        logger.error(f"Could not find device with name '{DEVICE_NAME}'. Please check if it's advertising.")
        return
    
    logger.info(f"Found device: {device.name} ({device.address})")
    
    async with BleakClient(device, timeout=20.0) as client:
        if not client.is_connected:
            logger.error(f"Failed to connect to {DEVICE_NAME}.")
            return

        logger.info(f"Connected to {DEVICE_NAME}")

        # Attempt to pair with the device
        logger.info("Attempting to pair with the device...")
        try:
            paired = await client.pair()
            if paired:
                logger.info("Pairing successful.")
            else:
                logger.warning("Pairing was not successful. The device may already be paired or does not require pairing.")
        except Exception as e:
            logger.warning(f"An error occurred during pairing: {e}. This may be expected on some OSes. Continuing...")

        # Subscribe to the data characteristic
        logger.info(f"Subscribing to photo data notifications on {PHOTO_DATA_UUID}...")
        await client.start_notify(PHOTO_DATA_UUID, notification_handler)
        logger.info("Subscription successful.")

        # Give the subscription a moment to settle
        await asyncio.sleep(1)

        # Send the command to start the photo transfer
        logger.info(f"Sending 'take photo' command (0xFF) to {PHOTO_CONTROL_UUID}...")
        await client.write_gatt_char(PHOTO_CONTROL_UUID, b'\xff', response=True)
        logger.info("Command sent.")

        # Wait for the transfer to complete (with a timeout)
        try:
            logger.info("Waiting for photo data transfer to complete...")
            await asyncio.wait_for(transfer_complete.wait(), timeout=30.0)
            logger.info("Photo transfer complete.")
        except asyncio.TimeoutError:
            logger.error("Timeout occurred while waiting for photo data.")
            return
        finally:
            # Always unsubscribe, but handle potential errors on cleanup
            try:
                logger.info("Unsubscribing from notifications...")
                await client.stop_notify(PHOTO_DATA_UUID)
                logger.info("Unsubscribed successfully.")
            except Exception as e:
                logger.warning(f"An error occurred while unsubscribing: {e}. This can sometimes be ignored.")

        # Save the received data to a file
        if photo_data:
            filename = f"photo_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
            with open(filename, "wb") as f:
                f.write(photo_data)
            logger.info(f"Photo saved as {filename} ({len(photo_data)} bytes)")
        else:
            logger.warning("No photo data was received.")

    logger.info("Disconnected.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        logger.error(f"An error occurred: {e}")
