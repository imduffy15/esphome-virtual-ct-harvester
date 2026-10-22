#!/usr/bin/env python3
"""Generate unique build credentials and a private setup card for one device."""
import argparse
import base64
import os
from pathlib import Path
import secrets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', type=int, help='Optional built-in identity, 1..16777215. The owner can also change it later on the web page.')
    args = parser.parse_args()
    serial = args.serial if args.serial is not None else secrets.randbelow(0xFFFFFF) + 1
    if not 1 <= serial <= 0xFFFFFF:
        parser.error('--serial must be 1..16777215')
    root = Path(__file__).resolve().parent
    config = root / 'secrets.yaml'
    card = root / 'device-card.txt'
    if config.exists() or card.exists():
        parser.error('Existing secrets.yaml or device-card.txt found; preserve those credentials instead of regenerating them')
    api_key = base64.b64encode(secrets.token_bytes(32)).decode()
    setup_password = secrets.token_hex(8)
    name = f'virtual-ct-harvester-{serial:06x}'
    ssid = f'Virtual CT Harvester {serial:06x}'
    yaml = f'''# Private, unique to this device. Preserve across updates.
device_name: "{name}"
setup_ap_ssid: "{ssid}"
setup_password: "{setup_password}"
harvi_radio_api_key: "{api_key}"
virtual_harvi_serial: {serial}
'''
    instructions = f'''Virtual CT Harvester — private device setup card

Virtual serial: {serial}
Setup Wi-Fi: {ssid}
Setup Wi-Fi password: {setup_password}
Wi-Fi setup page: http://192.168.4.1

After joining your home Wi-Fi:
Device address: http://{name}.local (or use the IP shown on the OLED)
Web page username: admin
Web page password: {setup_password}
Encryption key (Home Assistant and firmware updates): {api_key}

Open the device address in a browser and follow the setup guide there. It
works with or without Home Assistant. To use Home Assistant readings, add the
device in its ESPHome integration with the encryption key above.

Wi-Fi recovery: hold BOOT for 10 seconds after normal startup to reset saved
settings and return to setup. Do not hold BOOT while powering on.
Keep this card private.
'''
    os.umask(0o077)
    for path, value in ((config, yaml), (card, instructions)):
        with path.open('x') as output:
            output.write(value)
    print('Created private secrets.yaml and device-card.txt. Give the card to the device owner.')


if __name__ == '__main__':
    main()
