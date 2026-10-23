# Virtual CT Harvester for ESPHome

Virtual CT Harvester turns a LILYGO T3 LoRa32 board into a virtual myenergi
Harvi. It takes up to three power readings and sends them to a myenergi Zappi
electric vehicle (EV) charger over radio as current transformer (CT) readings.
The readings can come from Home Assistant, or from any device or script that
can send an HTTP request.

A Harvi is a myenergi wireless sensor that measures power with clamp-on CTs and
reports it to a Zappi. With Virtual CT Harvester, you can use power readings
you already have, such as from a smart meter, solar inverter, or home battery,
instead of installing CT clamps. You set up and run the device from its own
web page. The device has no vendor-specific dependencies and needs no extra
hardware.

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
  it. Each device has its own credentials. Keep the card for future setup and
  recovery.
- The password for your 2.4 GHz Wi-Fi network.
- A phone or computer with Wi-Fi.
- Optional: a Home Assistant installation, if your power readings are there.

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

### Open the device web page

1. Go to the address shown on the OLED, or to the `.local` address on the setup
   card.
2. Sign in with the username `admin` and the web page password from the setup
   card.

The device serves the web page itself, so the page works without an internet
connection. The page's **Setup guide** lists the remaining steps and marks
each one done when the device detects it:

1. **Find the Zappi.** The device finds the Zappi's radio channel at first
   boot. See [Find the radio channel](#find-the-radio-channel).
2. **Pair.** See [Pair with the Zappi](#pair-with-the-zappi).
3. **Set this device's CT types on the Zappi.** See
   [Configure the virtual CTs](#configure-the-virtual-cts).
4. **Connect your power readings.** See
   [Connect your power readings](#connect-your-power-readings).
5. **Turn on Send to Zappi.** See [Turn on the output](#turn-on-the-output).

## Use the device web page

The web page at `http://<device address>/` shows everything the device does,
with or without Home Assistant:

| Section | Shows |
| --- | --- |
| Status | The device's current state, the next step, and a hint if a step is taking too long |
| **Readings** | The three readings the device sends, where each one came from, and how old it is |
| **Send to Zappi** | The output switch |
| **Setup guide** | The setup steps, marked done as the device detects them |
| **Readings source** | Where readings come from, and each reading's entity, unit, and sign |
| **Advanced** | The device serial number, grid feedback, and the reference Harvi serial number |
| **Radio channel** | The receive channel, and a button to search again |
| **Diagnostics** | Phase, radio, and packet counters |
| **Device** | Network details, firmware version, restart, and factory reset |

Changes take effect when you click **Save**, and the device saves them to
flash immediately. If you change a Home Assistant entity, the device restarts,
which takes about 15 seconds. The page reconnects automatically.

### Use the web page API

The web page uses a JSON API that you can also call from scripts. Requests use
digest authentication with the username `admin` and the web page password.

- `GET /api/status` returns the device's state.
- `POST /api/COMMAND` with a JSON body runs a command, where `COMMAND` is one
  of the following:

| Command | Body | Does |
| --- | --- | --- |
| `output` | `{"on": true}` | Turns **Send to Zappi** on or off |
| `pair` | `{}` | Starts pairing |
| `stop_pairing` | `{}` | Stops pairing |
| `scan` | `{}` | Searches for the Zappi's channel |
| `channel` | `{"channel": 0}` | Sets the receive channel, from 0 to 7 |
| `settings` | The settings the page sends | Saves settings |
| `restart` | `{}` | Restarts the device |
| `factory_reset` | `{}` | Erases all saved settings |

## Find the radio channel

The device must listen on the same radio channel as your Zappi. It searches
for the channel automatically at first boot. The search stops at the first
channel that carries status broadcasts from a Zappi, and takes up to 96
seconds.

To search again, for example after the Zappi has moved, open **Radio channel**
on the web page and click **Search for Zappi**.

To set the channel manually, choose a value in **Receive channel**. Do this,
for example, if a neighbor's myenergi system uses a different channel and the
search finds that one first. The channel numbers in the list aren't verified
Zappi channel labels.

If a working genuine Harvi is still installed, you can search for its channel
instead. Enter its serial number in **Advanced > Reference Harvi serial**,
click **Save**, then search again. The search then waits for two distinct
packets from that Harvi. The Harvi needs enough current through its CT clamps
to power its transmitter.

## Pair with the Zappi

Pairing registers the device's virtual serial number on the Zappi. Pairing
packets contain no CT readings. The device sends measurements only after you
turn on **Send to Zappi**.

1. On the Zappi, open **Linked Devices > Pairing**.
2. Start pairing on the device in one of these ways:
   - Click **Start pairing** on the web page or in Home Assistant.
   - After the device has started, press and hold the **BOOT** button for
     0.5–3 seconds, then release it.
3. On the Zappi, select the virtual serial number shown on the web page and the
   OLED, then confirm it.

The device sends pairing packets for two minutes. It stops sooner if a
transmission fails or if you click **Stop pairing**. The **Pair** step in the
setup guide is marked done when the Zappi's broadcasts list the device's
serial number.

## Configure the virtual CTs

Set each Zappi CT type and reading sign as shown in the following table:

| Reading | Sent as | Zappi CT type | Sign |
| --- | --- | --- | --- |
| Grid | CT1 | Grid | Positive for import, negative for export |
| Solar | CT2 | **Generation (required)** | Positive for generation |
| Battery | CT3 | **AC Battery**, Storage, or any other type | Positive for discharging, negative for charging |

myenergi CTs read positive when power flows toward the house. Many battery
sensors, such as Anker SOLIX, report charging as positive. For those sensors,
select **Invert sign** for the battery reading. If the sign is wrong, the
myenergi app shows a charging battery as supplying the house and overstates
the house load.

### Prerequisites

- The device is paired with the Zappi. See [Pair with the Zappi](#pair-with-the-zappi).

### Steps

1. On the Zappi, set this device's CT2 to **Generation**, and set CT1 and CT3
   as shown in the table.

   CT2 must be the only Generation CT on the Zappi, because the device uses the
   Zappi's generation total as phase feedback. If you have no solar, leave the
   solar reading unused, but still set CT2 to **Generation**.
2. If any other device has a Grid CT on the Zappi, change that CT to a
   different type. Otherwise the Zappi adds both grid readings together.

## Connect your power readings

Choose where the readings come from in **Readings source** on the web page:

- **Home Assistant:** the device reads Home Assistant sensors.
- **HTTP push:** another device or script on your network sends readings to
  the device. You don't need Home Assistant.

Leave out readings you don't have. The device sends an unused reading as 0 W.
If a reading you set up isn't available, the device withholds all
measurements until it is.

### Read from Home Assistant

1. In Home Assistant, go to **Settings > Devices & services**.
2. Configure the discovered ESPHome device. If Home Assistant doesn't discover
   it, add the ESPHome integration and enter the device's IP address.
3. When prompted, enter the **encryption key** from the setup card.
4. On the device web page, under **Readings source**, click **Home Assistant**.
5. For each reading you have, enter its Home Assistant entity ID, for example
   `sensor.grid_power`.

   To find an entity ID, open the entity's settings in Home Assistant.
6. Leave the unit at **Auto** to take W or kW from the entity. If the entity
   has no unit, choose **W** or **kW**. The device converts kW readings to W.
7. If a reading's sign is the opposite of the table in
   [Configure the virtual CTs](#configure-the-virtual-cts), select
   **Invert sign**.
8. Click **Save**. The device restarts to subscribe to the new entities.
9. Compare each value under **Readings** with its source sensor.

> [!NOTE]
> Home Assistant might not resend a reading that hasn't changed, so the device
> doesn't time out Home Assistant readings. If a source sensor stops updating
> without becoming unavailable, the device keeps sending its last value.

### Push readings over HTTP

1. On the device web page, under **Readings source**, click **HTTP push**.
2. If a reading's sign is the opposite of the table in
   [Configure the virtual CTs](#configure-the-virtual-cts), select
   **Invert sign**.
3. Set **Readings expire after** to how long a reading stays valid. The
   default is 30 seconds.
4. Click **Save**.
5. Set up a device or script to send the readings to `/api/power`, in watts,
   every few seconds. Authenticate with the web page password as the `key`
   field, or with digest authentication. For example:

   ```sh
   curl -X POST http://virtual-ct-harvester-XXXXXX.local/api/power \
     -d key=SETUP_PASSWORD -d grid=1520 -d solar=2310 -d battery=-400
   ```

   The fields are `grid`, `solar`, and `battery`. The device accepts form
   fields, query parameters, or a JSON body. **How to send readings** on the web
   page shows an example script for a Shelly Gen2 or later meter, such as a
   Pro 3EM.
6. Compare each value under **Readings** with its source.

Send all the readings you have in one request. A reading you leave out of a
request keeps its last value. The device treats a reading it has never
received as unused.

> [!IMPORTANT]
> If no new reading arrives within **Readings expire after**, the device
> withholds all measurements until one does.

## Turn on the output

### Prerequisites

- The device is paired, and the Zappi CT types are set. See
  [Configure the virtual CTs](#configure-the-virtual-cts).
- The readings show values. See
  [Connect your power readings](#connect-your-power-readings).

### Steps

1. On the web page, turn on **Send to Zappi**. The status shows these states in
   order:
   1. **Waiting for Zappi**: the Zappi hasn't listed this device's serial
      number yet.
   2. **Locking on**: the device is matching its readings to the Zappi's mains
      phase.
   3. **Live**: the phase is locked, and the Zappi receives the readings.

   Phase lock usually takes 10–45 seconds. If it takes longer, check that CT2
   is set to **Generation** and is the only Generation CT on the Zappi.
2. Optional: under **Advanced**, select **Grid total comes only from this
   device**, then click **Save**. Do this only when no other device has a Grid
   CT on the Zappi. The device then uses the Zappi's grid total as a second
   feedback channel.
3. Compare the Zappi's readings with your meter, both while you import power
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
3. On the device web page, open **Advanced**, and enter the genuine Harvi's
   serial number in **Device serial**. The serial number is printed on the
   Harvi's label and shown in the Zappi's **Linked Devices** list.
4. Leave **Reference Harvi serial** at `0`, then click **Save**.

   You don't need to rebuild or reflash the device. A factory reset returns the
   device to its built-in serial number.
5. Pair the device and set its CT types. See
   [Pair with the Zappi](#pair-with-the-zappi) and
   [Configure the virtual CTs](#configure-the-virtual-cts).
6. In the myenergi app, check that the solar and battery bubbles appear. Check
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
  78° ± 3° to its measurement. Any phase error appears in the generation
  total, so the device can track the phase even at night, when there's no
  solar generation. The pilot adds about 60 W of real power, and the device
  subtracts that from solar. So generation reads accurately once solar is
  above about 60 W, and reads about 60 W at night instead of jittering below
  0 W, which the myenergi app would show as power flowing back into
  generation.
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
  the phase uncertainty stays under 25°. The status shows **Live (holding)**.
  If the feedback shows that the phase has moved, the device withholds the
  readings immediately, because the held phase is then wrong and can flip a
  reading's sign.
- While the device acquires the phase, CT2 carries only the pilot, so the
  Zappi's CT2 reading stays within ±300 W.
- If any reading you set up has no usable value, the device withholds all
  measurements. A reading has no usable value when the Home Assistant
  connection has restarted and no value has arrived since, when its unit is
  unknown, or when a pushed reading has expired. The pilot keeps the phase
  locked. The device sends an unused reading as 0 W.
- If the device stops transmitting, the Zappi keeps the last values for at
  least 34 seconds, then clears them.
- Each packet takes about 4 ms of airtime. The device sends one packet every
  1–3 seconds, at a transmit power of 2 decibel-milliwatts (dBm).

### Known limitations

- The device infers phase lock from feedback. It doesn't measure the phase
  against mains.
- If a Home Assistant source sensor stops updating without becoming
  unavailable, the device keeps sending its last value.
- After any firmware change, check the Grid reading again before you rely on it
  for charging control.

## Monitor the device

Weak reception of the Zappi's broadcasts is the main cause of lost phase lock.
Keep the antenna vertical and away from inverters and metal enclosures.

The web page's **Diagnostics** section shows the phase uncertainty, the phase
fit gain, held packets, the radio noise floor, receiver restarts, and the
Zappi's serial number. A phase fit gain of about 1.00 means the scaling is
correct.

### Home Assistant entities

The device adds these entities to Home Assistant:

| Entity | Type | Shows or does |
| --- | --- | --- |
| **Status** | Sensor | The device's state, such as **Live**, **Locking on**, **Waiting for readings**, or **Not paired** |
| **Zappi link** | Binary sensor (connectivity) | On while live readings reach the Zappi |
| **Send to Zappi** | Switch | Turns the output on or off |
| **Start pairing** | Button (configuration) | Starts pairing |
| **Restart** | Button (diagnostic) | Restarts the device |
| **Phase uncertainty**, **Radio noise floor**, **Wi-Fi signal**, **Uptime** | Sensors (diagnostic) | Disabled by default. Turn them on in Home Assistant if you need them. |

Settings and detailed counters are on the device web page.

## Troubleshooting

### The Wi-Fi network changed or is unavailable

Join the device's setup network again and enter the new Wi-Fi details. The
setup Wi-Fi password is on the original setup card.

### Reset saved settings

On the web page, open **Device** and click **Reset to factory settings**. Or,
after the device has started normally, hold the **BOOT** button for 10
seconds. The device returns to Wi-Fi setup.

> [!CAUTION]
> Resetting erases all saved settings, including Wi-Fi, readings, the radio
> channel, and a changed serial number. This can't be undone. The device's
> built-in serial number and the credentials on the setup card don't change.

> [!NOTE]
> Don't hold **BOOT** while you power on the board. That starts the ESP32 USB
> bootloader instead.

### The status shows "Waiting for readings"

Each reading under **Readings** on the web page shows what it's waiting for:

- **Waiting for Home Assistant:** check the device's connection to the Home
  Assistant integration and the entity ID.
- **Unit unknown: choose W or kW:** the entity has no unit. Choose **W** or
  **kW** for that reading.
- **Expired:** the pushed readings stopped arriving. Check the device or
  script that sends them.

Missing readings don't prevent pairing.

### The status shows "Locking on" for more than a minute

Check that this device's CT2 is set to **Generation** on the Zappi and is the
only Generation CT. Then check the antenna position. See
[Monitor the device](#monitor-the-device).

### No packets are received, or the status shows "Radio fault"

Check the board variant, the antenna, and the receive channel.

### The OLED is blank

The cause might be an I²C (Inter-Integrated Circuit) bus or display problem.
USB logs still work.

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

The first build downloads the ESP32 build toolchain. The repository includes
the web page, the font, and the radio component.

`prepare_device.py` creates a unique `secrets.yaml` file and a private
`device-card.txt` setup card. It doesn't overwrite existing credentials.

To set a specific built-in serial number, add `--serial NUMBER`, where
`NUMBER` is between 1 and 16777215 (a nonzero 24-bit value). The owner can
also change the serial number later on the web page. For details, see
[Show solar and battery in the myenergi app](#show-solar-and-battery-in-the-myenergi-app).

> [!WARNING]
> Don't flash more than one device with the same generated secrets. Use a
> separate working directory and setup card for each device, and keep each
> device's secrets for future updates.

### Update a device built from an earlier version

Earlier versions used `harvi-radio.yaml` as the main file. The main file is now
`virtual-ct-harvester.yaml`. Existing secrets still work. When you update a
configured device, keep its existing keys, virtual serial number, and device
name.

Versions before the device web page differ in these ways:

- **Settings:** saved entities, units, signs, the radio channel, and the
  reference Harvi serial number carry over. You now edit settings on the web
  page instead of in Home Assistant.
- **Home Assistant entities:** the device now adds fewer entities. See
  [Home Assistant entities](#home-assistant-entities). Home Assistant might
  keep the old entities as unavailable. You can delete them.
- **Output switch:** **Virtual CT output** is now **Send to Zappi**. It's off
  after the update, so turn it on again.
- **Firmware updates:** over-the-air (OTA) updates now use the encryption key
  instead of a separate password. A device built with ESPHome 2026.9.0
  already accepts encrypted updates, so update it over the air as usual. The
  device no longer uses the `harvi_radio_ota_password` secret.

### Use ESPHome Device Builder

1. Copy the following into the Device Builder configuration directory:
   - `virtual-ct-harvester.yaml`
   - The `packages`, `components`, and `fonts` directories
   - The device's secrets. Add them to the existing `secrets.yaml` file, and
     keep any unrelated secrets that are already there.
2. For the first flash, click **Install**, then choose USB.

To use a separate USB installer, download the **factory** image. Later OTA
updates are encrypted with the device's encryption key
(`harvi_radio_api_key`).

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
