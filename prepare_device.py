#!/usr/bin/env python3
"""Generate unique build credentials and a private setup card for one device."""
import argparse
import base64
import os
from pathlib import Path
import secrets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', type=int, help='Serial number for this device, from 1 to 16777215, for example a retired genuine Harvi serial number (see README.md). Default: a random serial number.')
    args = parser.parse_args()
    serial = args.serial if args.serial is not None else secrets.randbelow(0xFFFFFF) + 1
    if not 1 <= serial <= 0xFFFFFF:
        parser.error('--serial must be between 1 and 16777215')
    root = Path(__file__).resolve().parent
    config = root / 'secrets.yaml'
    card = root / 'device-card.txt'
    if config.exists() or card.exists():
        parser.error('secrets.yaml or device-card.txt already exists. Keep the existing credentials, or prepare this device in a new directory.')
    api_key = base64.b64encode(secrets.token_bytes(32)).decode()
    setup_password = secrets.token_hex(8)
    ota_password = secrets.token_hex(24)
    name = f'virtual-ct-harvester-{serial:06x}'
    ssid = f'Virtual CT Harvester {serial:06x}'
    yaml = f'''# Private credentials for this device only. Keep them for future updates.
device_name: "{name}"
setup_ap_ssid: "{ssid}"
setup_password: "{setup_password}"
harvi_radio_api_key: "{api_key}"
harvi_radio_ota_password: "{ota_password}"
virtual_harvi_serial: {serial}
'''
    instructions = f'''Virtual CT Harvester — private device setup card

Keep this card private. It contains the credentials for this device.

Virtual serial: {serial}
Setup Wi-Fi: {ssid}
Setup Wi-Fi password: {setup_password}
Wi-Fi setup page: http://192.168.4.1

After you join your home Wi-Fi network:
Device address: http://{name}.local (or the IP address shown on the display)
Settings username: admin
Settings password: {setup_password}
Home Assistant API encryption key: {api_key}
Firmware update password: {ota_password}

Set up the device:
1. Add the device in the Home Assistant ESPHome integration.
2. On the settings page, enter up to three power entity IDs and set each unit
   to W or kW.
3. Click Apply input settings and restart.
4. Pair the device with the Zappi and set its CT types.
5. Turn on Virtual CT output.
For details, see README.md.

Reset saved settings: after the device starts normally, hold BOOT for
10 seconds. This erases saved settings, including Wi-Fi, and returns the device
to setup. Don't hold BOOT while you power on the device.
'''
    os.umask(0o077)
    for path, value in ((config, yaml), (card, instructions)):
        with path.open('x') as output:
            output.write(value)
    print('Created secrets.yaml and device-card.txt. Keep both private, and give the setup card to the device owner.')


if __name__ == '__main__':
    main()
