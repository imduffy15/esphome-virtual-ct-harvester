# Licenses and third-party notices

## Project license

The project code is distributed under the GNU General Public License,
version 3 only (GPL-3.0-only). For the full text, see [LICENSE](LICENSE).

## SX127x radio component

The SX127x component in `components/sx127x` is based on the
[ESPHome 2026.9.0 SX127x component](https://github.com/esphome/esphome/tree/2026.9.0/esphome/components/sx127x).
Its original license text, MIT for the Python code and GPLv3 for the C++ code,
is in [components/sx127x/LICENSE](components/sx127x/LICENSE).

This copy adds the following changes:

- Interrupt request (IRQ) timestamps
- Register diagnostics
- Bounds and cyclic redundancy check (CRC) checks for the frequency-shift
  keying (FSK) first-in, first-out (FIFO) buffer
- Bounded handling of transmit completion
- Transmission at a scheduled microsecond
- A receiver profile that sets maximum low-noise amplifier (LNA) gain and an
  automatic frequency correction (AFC) bandwidth equal to the channel filter
- Rewritten code comments, log messages, and configuration error messages

This copy is intended for this project's FSK profile. It isn't a
general-purpose replacement for the ESPHome SX127x component.

## Font

The repository includes the DejaVu Sans Mono font. Its license is in
[fonts/LICENSE](fonts/LICENSE).

## Trademarks and affiliation

This project is independent of myenergi, ESPHome, and LILYGO. Harvi and Zappi
are myenergi trademarks. This project uses those names only to identify the
devices and the radio protocol that it targets. It doesn't claim any
affiliation with or endorsement by myenergi.
