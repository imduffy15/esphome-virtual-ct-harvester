# Virtual CT Harvester for ESPHome

Virtual CT Harvester turns a LILYGO T3 LoRa32 board into a virtual myenergi
Harvi. It reads up to three power sensors from Home Assistant and sends them to
a myenergi Zappi electric vehicle (EV) charger over radio as current
transformer (CT) readings.

A Harvi is a myenergi wireless sensor that measures power with clamp-on CTs and
reports it to a Zappi. With Virtual CT Harvester, you can use power sensors from
any Home Assistant integration, such as a smart meter, solar inverter, or home
battery, instead of installing CT clamps. The device has no vendor-specific
dependencies and needs no extra hardware.

> [!NOTE]
> Virtual CT Harvester is an independent community project. It isn't affiliated
> with or endorsed by myenergi. The Harvi and Zappi names identify the myenergi
> products and the radio protocol that this project targets.

> [!WARNING]
> This project is experimental. Testing so far covers one installation (single
> phase, 230 V, 50 Hz), including an overnight test that ran from export through
> 9 kW import. Check the readings against your own meter before you rely on
> them.

## Supported hardware

- **LILYGO T3 LoRa32 V1.6.1** with a classic ESP32, an SX1276 radio, the 868 MHz
  band, and a 0.96-inch organic light-emitting diode (OLED) display.
- The board's 868 MHz antenna.
- A USB cable and power supply.

Other variants, such as boards with an SX1262 radio, an ESP32-S3, or a 433 MHz
radio, aren't supported. For pin assignments, see
[Hardware reference](#hardware-reference).

The board has no connection to mains power. It infers the Zappi's mains phase,
which is the current position in the alternating current (AC) cycle of your
electricity supply, from the Zappi's own radio broadcasts. For details, see
[How it works](#how-it-works).

## Set up a preflashed device

Follow these steps if you received a device that someone else built and
flashed. To build your own, see
[Build and flash your own device](#build-and-flash-your-own-device).

### Prerequisites

- The device's private setup card. The person who built the device supplies
  it. Each device has its own credentials and a fixed virtual serial number.
  Keep the card for future setup and recovery.
- The password for your 2.4 GHz Wi-Fi network.
- A Home Assistant installation.
- A phone or computer with Wi-Fi.

You don't need to edit firmware or connect the device to a computer.

### Connect the device to Wi-Fi

1. Attach the 868 MHz antenna, then power the board with a USB cable.
2. On your phone, join the Wi-Fi network named **Virtual CT Harvester …**. Use
   the setup Wi-Fi password from the setup card.

   The device broadcasts this setup network whenever it has no working Wi-Fi
   connection. If your phone reports that the network has no internet access,
   stay connected.
3. If the Wi-Fi setup page doesn't open automatically, go to
   `http://192.168.4.1`.
4. Select your home network and enter its password.
5. Reconnect your phone to your home Wi-Fi network.

### Open the settings page

1. Go to the address shown on the OLED, or to the `.local` address on the setup
   card.
2. Sign in with the username `admin` and the settings password from the setup
   card.

The device serves the settings page itself, so the page works without an
internet connection.

### Add the device to Home Assistant

1. In Home Assistant, go to **Settings > Devices & services**.
2. Configure the discovered ESPHome device. If Home Assistant doesn't discover
   it, add the ESPHome integration and enter the device's IP address.
3. When prompted, enter the **Home Assistant API encryption key** from the
   setup card.

### Configure power inputs

Each power input reads one Home Assistant power sensor. The device sends
**Power input 1** as CT1, **Power input 2** as CT2, and **Power input 3** as
CT3. To decide which sensor goes in each input, see
[Configure the virtual CTs](#configure-the-virtual-cts).

1. On the device's settings page, enter a Home Assistant entity ID in
   **Power 1 entity**, **Power 2 entity**, and **Power 3 entity**, for example
   `sensor.grid_power`. Leave unused inputs blank.

   To find an entity ID, open the entity's settings in Home Assistant.
2. For each input, set **Power N source unit** to **W** or **kW** to match the
   source sensor. The device converts kW readings to W.
3. The device keeps each reading's sign unless you set **Power N sign** to
   **Inverted**. Invert a source if its sign is the opposite of what its CT
   needs. For the sign each CT needs, see
   [Configure the virtual CTs](#configure-the-virtual-cts).
4. Click **Apply input settings and restart**. When the device reconnects,
   refresh the page.
5. Compare **Power input 1**, **Power input 2**, and **Power input 3** with the
   source sensors.

You can also change these settings from the device's page in Home Assistant.

**Power inputs recently received** turns on when every configured input has
delivered a numeric value within **Input receipt timeout** (120 seconds by
default). It measures when the device last received a value, not when the
source last took a measurement. Home Assistant might not resend a reading that
hasn't changed, so an input with a constant value can time out.

## Find the radio channel

The device must listen on the same radio channel as your Zappi. It scans for
the channel automatically at first boot. To scan again, do one of the
following:

- **If no working genuine Harvi is installed:** leave **Reference Harvi
  serial** at `0` and click **Find channel**. The scan stops at the first
  channel that carries status broadcasts from a Zappi.
- **If a working genuine Harvi is still installed:** enter its serial number in
  **Reference Harvi serial**, then click **Find channel**. The scan waits for
  two distinct packets from that Harvi. The Harvi needs enough current through
  its CT clamps to power its transmitter.

The scan takes up to 96 seconds.

To set the channel manually, choose a value in **Receive frequency**. Do this,
for example, if a neighbor's myenergi system uses a different channel and the
scan finds that one first. The candidate numbers in the list aren't verified
Zappi channel labels.

## Pair with the Zappi

Pairing registers the device's virtual serial number on the Zappi. Pairing
packets contain no CT readings. The device sends measurements only after you
turn on **Virtual CT output**.

1. On the Zappi, open **Linked Devices > Pairing**.
2. Start pairing on the device in one of these ways:
   - Click **Start pairing** on the settings page or in Home Assistant.
   - After the device has started, press and hold the **BOOT** button for
     0.5–3 seconds, then release it.
3. On the Zappi, select the virtual serial number shown on the OLED and the
   setup card, then confirm it.

The device sends pairing packets for two minutes. It stops sooner if a
transmission fails or if you click **Stop pairing**.

> [!NOTE]
> **Pairing packets sent** counts the packets the device transmitted. It
> doesn't confirm that the Zappi accepted the pairing.

## Configure the virtual CTs

Set each Zappi CT type and source sign as shown in the following table:

| Input | Sent as | Zappi CT type | Sign |
| --- | --- | --- | --- |
| Power 1 | CT1 | Grid | Positive for import, negative for export |
| Power 2 | CT2 | **Generation (required)** | Positive for generation |
| Power 3 | CT3 | **AC Battery**, Storage, or any other type | Positive for discharging, negative for charging |

myenergi CTs read positive when power flows toward the house. Many battery
sensors, such as Anker SOLIX, report charging as positive. For those sensors,
set **Power 3 sign** to **Inverted**. If the sign is wrong, the myenergi app
shows a charging battery as supplying the house and overstates the house load.

### Prerequisites

- The device is paired with the Zappi. See [Pair with the Zappi](#pair-with-the-zappi).

### Steps

1. On the Zappi, set this device's CT2 to **Generation**, and set CT1 and CT3
   as shown in the table.

   CT2 must be the only Generation CT on the Zappi, because the device uses the
   Zappi's generation total as phase feedback. If you have no generation, leave
   **Power 2 entity** blank, but still set CT2 to **Generation**.
2. If any other device has a Grid CT on the Zappi, change that CT to a
   different type. Otherwise the Zappi adds both grid readings together.
3. Turn on **Virtual CT output**. **Gateway status** shows these states in
   order:
   1. `Virtual CT: waiting for the Zappi to list this serial`
   2. `Virtual CT: acquiring phase, CT1/CT3 withheld`
   3. `Virtual CT: phase locked, output live`

   Phase lock means the device has matched its readings to the Zappi's mains
   phase. It takes 10–45 seconds. The OLED then shows `CT out: locked`.
4. Turn on **Grid total is virtual only**. Do this only when no other device
   has a Grid CT on the Zappi. The device then uses the Zappi's grid total as a
   second feedback channel.
5. Compare the Zappi's readings with your inputs, both while you import power
   and while you export it.

## Show solar and battery in the myenergi app

The myenergi app's energy-flow screen shows a leaf, which represents the
green-energy share, surrounded by bubbles. The app draws every flow through the
leaf. The grid, house, and charger bubbles always appear. The other bubbles
depend on the CT types on your account:

- The **solar** bubble appears only when a CT has type Generation.
- The **battery** bubble appears only when a CT has type AC Battery.
- The Storage type shows a separate storage icon, and Gen & Battery shows a
  combined icon.

The myenergi cloud learns these types from the Zappi. For the bubbles to
appear, both of the following must be true:

- **This device is the only Harvi linked to the Zappi.** In each update, the
  Zappi reports the CT types of one Harvi only: the first linked Harvi that has
  new data. If a genuine Harvi is linked ahead of this device, the Zappi nearly
  always reports the genuine Harvi. This device's CT types then never reach the
  cloud, even though the Zappi uses its readings. In one installation, about
  3,000 consecutive cloud updates all reported the genuine Harvi.
- **The myenergi cloud accepts the device's serial number.** The myenergi
  website rejected a generated serial number as invalid. Reusing the serial
  number of a retired genuine Harvi works. Using a generated serial number on
  the only linked Harvi hasn't been tested.

### Replace a genuine Harvi with this device

> [!WARNING]
> Never let a genuine Harvi and this device transmit the same serial number.
> The Zappi mixes their readings.

1. Retire the genuine Harvi permanently by unclipping all of its CT clamps.
   The Harvi powers itself from its clamps, so it stops transmitting.
2. On the Zappi, remove every Harvi from **Linked Devices**.
3. Give this device the genuine Harvi's serial number. The serial number is
   printed on the Harvi's label and shown in the Zappi's **Linked Devices**
   list.
   - **New device:** run `python prepare_device.py --serial NUMBER`.
   - **Existing device:** change `virtual_harvi_serial` in the device's
     `secrets.yaml` file. Keep the other values.
4. Build and flash the device. See
   [Build and flash your own device](#build-and-flash-your-own-device).
5. Leave **Reference Harvi serial** at `0`.
6. Pair the device and set its CT types. See
   [Pair with the Zappi](#pair-with-the-zappi) and
   [Configure the virtual CTs](#configure-the-virtual-cts).
7. In the myenergi app, check that the solar and battery bubbles appear. Check
   that the house figure is roughly equal to solar + grid import − battery
   charging − charger power.

### Zappi CT type reference

The Zappi accepts a linked serial number from any transmitter, because it
stores no pairing key. In tests against an emulator of the Zappi firmware, it
also accepted packet sequence numbers that went backward or repeated.

The Zappi sends these CT type codes to the myenergi cloud:

| Code | CT type |
| --- | --- |
| 0 | None |
| 1 | Grid |
| 2 | Generation |
| 3 | Storage |
| 4 | Gen & Battery |
| 5 | Internal Load |
| 6 | Monitor |
| 7 | AC Battery |

Whether the Zappi's own charging behavior differs between Storage and AC
Battery hasn't been analyzed.

## How it works

For each CT, the Zappi computes power with this formula:

```text
power = V × floor(|phasor| / 1.19) × cos(angle − mains phase)
```

In this formula:

- `V` is the mains voltage.
- `phasor` is the CT reading as the device sends it. A phasor describes an AC
  quantity with a magnitude and an `angle`.
- `mains phase` is the Zappi's own mains phase when the packet arrives.

The board isn't connected to mains, so it can't measure that phase directly.
Instead, it rotates each reading to the Zappi's predicted mains phase at the
moment the packet arrives. It predicts the phase as follows:

- **Feedback:** the Zappi broadcasts its grid and generation totals over radio
  once per second. The device identifies its own Zappi from the record that
  lists this device's virtual serial number, and ignores broadcasts from
  neighboring installations.
- **Pilot:** CT2 always carries an extra 300 W signal, called the pilot, at
  90° ± 3° to its measurement. When the phase is locked, the pilot shifts the
  generation reading by about ±16 W. Any phase error appears in the generation
  total, so the device can track the phase even at night, when there's no
  solar generation.
- **Acquisition:** to find the phase, the device runs a least-squares fit,
  which finds the phase and frequency that best match the feedback, across
  mains frequencies. Between packets, it steps the sent angle by the
  golden angle (about 137.5°) so that the samples cover many different phases.
- **Tracking:** after acquisition, a Kalman filter tracks the phase and
  frequency. A Kalman filter is an algorithm that updates an estimate, and its
  uncertainty, with each new measurement.
- **Timing:** the device sends one packet 450 ms after each total it hears,
  when no other device is transmitting. The radio driver starts each
  transmission at a scheduled microsecond, so the device computes the angle for
  the exact send time.

## Safety behavior

- The device sends CT1 and CT3 only while the phase is locked. Otherwise it
  omits them, and the Zappi reads 0. The device never sends a reading at an
  unknown phase.
- If feedback stops briefly and the tracker is still consistent, the device
  keeps sending readings on the last good phase for up to 15 seconds, while
  the phase uncertainty stays under 25°. The OLED shows `CT out: holding`.
  If the feedback shows that the phase has moved, the device withholds the
  readings immediately, because the held phase is then wrong and can flip a
  reading's sign.
- While the device acquires the phase, CT2 carries only the pilot, so the
  Zappi's CT2 reading stays within ±300 W.
- If any configured input has had no numeric value since the Home Assistant
  connection last started, the device withholds all measurements. The pilot
  keeps the phase locked. The device sends an unconfigured input as 0 W.
- If the device stops transmitting, the Zappi keeps the last values for at
  least 34 seconds, then clears them.
- Each packet takes about 4 ms of airtime. The device sends one packet every
  1–3 seconds, at a transmit power of 2 decibel-milliwatts (dBm).

### Known limitations

- The device infers phase lock from feedback. It doesn't measure the phase
  against mains.
- If a source sensor stops updating in Home Assistant without becoming
  unavailable, the device keeps sending its last value.
- After any firmware change, check the Grid reading again before you rely on it
  for charging control.

## Monitor the device

Weak reception of the Zappi's broadcasts is the main cause of lost phase lock.
Keep the antenna vertical and away from inverters and metal enclosures.

Use these diagnostic entities to check the device:

| Entity | Shows |
| --- | --- |
| **Gateway status** | The device's current state |
| **Phase locked** | Whether the phase is locked and output is live |
| **Phase uncertainty** | The estimated phase error, in degrees |
| **Phase fit gain** | The fitted scaling. A value of about 1.00 means the scaling is correct. |
| **Virtual CT held packets** | The number of packets sent on a held phase |
| **Radio noise floor** | The channel noise between packets, in dBm |
| **Receiver restarts** | The number of times the device restarted its receiver after 10 seconds without packets |
| **Zappi serial** | The serial number of the Zappi that lists this device |

## Troubleshooting

### The Wi-Fi network changed or is unavailable

Join the device's setup network again and enter the new Wi-Fi details. The
setup Wi-Fi password is on the original setup card.

### Reset saved settings

After the device has started normally, hold the **BOOT** button for 10
seconds. The device returns to setup.

> [!CAUTION]
> Resetting erases all saved settings, including Wi-Fi, input entities, units,
> and frequency settings. This can't be undone. The device's built-in identity
> and the credentials on the setup card don't change.

> [!NOTE]
> Don't hold **BOOT** while you power on the board. That starts the ESP32 USB
> bootloader instead.

### The power inputs show no values

Check the device's connection to the Home Assistant integration, the entity IDs,
and the units. After you edit them, click **Apply input settings and restart**.

Missing inputs don't prevent a pairing test.

### No packets are received, or the gateway status shows "Radio failed"

Check the board variant, the antenna, and the receive frequency.

### The OLED is blank

The cause might be an I²C (Inter-Integrated Circuit) bus or display problem.
USB logs still work.

### Settings are lost after unplugging

The device can take up to a minute to save an ordinary setting change. Wait a
minute before you unplug it. **Apply input settings and restart** saves pending
settings before it restarts.

## Build and flash your own device

### Prerequisites

- A supported board. See [Supported hardware](#supported-hardware).
- Python 3.12, 3.13, or 3.14.
- ESPHome 2026.9.0. The repository pins this version in `requirements.txt`,
  and the complete image was build-checked with it.

### Build and flash a new device

1. Download or clone this repository into its own directory.
2. Run the following commands from that directory:

   ```sh
   # Create and activate a Python virtual environment.
   python3 -m venv .venv
   . .venv/bin/activate

   # Install the pinned ESPHome version.
   pip install -r requirements.txt

   # Generate unique credentials and a setup card for this device.
   python prepare_device.py

   # Build the firmware, then flash it over USB.
   esphome compile virtual-ct-harvester.yaml
   esphome upload virtual-ct-harvester.yaml --device /dev/ttyUSB0
   ```

   Replace `/dev/ttyUSB0` with your board's serial port.

The first build downloads the ESP32 build toolchain and the web interface
assets. The repository includes the font and the radio component.

`prepare_device.py` creates a unique `secrets.yaml` file and a private
`device-card.txt` setup card. It doesn't overwrite existing credentials.

To set a specific serial number, add `--serial NUMBER`, where `NUMBER` is
between 1 and 16777215 (a nonzero 24-bit value). To get the solar and battery
bubbles in the myenergi app, use the serial number of a retired genuine Harvi.
For details, see
[Show solar and battery in the myenergi app](#show-solar-and-battery-in-the-myenergi-app).
Never use a serial number that another working device still transmits.

> [!WARNING]
> Don't flash more than one device with the same generated secrets. Use a
> separate working directory and setup card for each device, and keep each
> device's secrets for future updates.

### Update a device built from an earlier version

Earlier versions used `harvi-radio.yaml` as the main file. The main file is now
`virtual-ct-harvester.yaml`. Existing secrets still work. When you update a
configured device, keep its existing keys, virtual serial number, and device
name.

### Use ESPHome Device Builder

1. Copy the following into the Device Builder configuration directory:
   - `virtual-ct-harvester.yaml`
   - The `packages`, `components`, and `fonts` directories
   - The device's secrets. Add them to the existing `secrets.yaml` file, and
     keep any unrelated secrets that are already there.
2. For the first flash, click **Install**, then choose USB.

To use a separate USB installer, download the **factory** image. Later
over-the-air (OTA) updates use the device's existing credentials.

> [!WARNING]
> Firmware binaries contain the device's credentials. Don't share them
> publicly. The values in `secrets.example.yaml` are only for build validation.
> Don't use them on real devices.

### Hardware reference

The board connects its components to these general-purpose input/output (GPIO)
pins. The radio uses the Serial Peripheral Interface (SPI) bus.

| Hardware connection | GPIO |
| --- | --- |
| SPI clock / MISO / MOSI | 5 / 19 / 27 |
| Radio CS / reset / DIO0 | 18 / 23 / 26 |
| OLED SDA / SCL, address 0x3c | 21 / 22 |
| BOOT button | 0 |

MISO and MOSI are the SPI data lines, CS is the radio's chip select, and DIO0 is
the radio's interrupt line. SDA and SCL are the I²C data and clock lines.

- The radio uses frequency-shift keying (FSK), not LoRa (long range)
  modulation.
- All transmissions use 2 dBm.
- GPIO5 is a strapping pin, which the ESP32 reads at startup to choose its boot
  mode. The manufacturer wires it to the radio. Don't connect other loads to
  it, because they can affect startup.
- The device doesn't need an SD card.

## Licenses

For license details and third-party notices, see [NOTICE.md](NOTICE.md).
