# Dongle firmware

The 2.4 GHz receiver for the Dachshund60F: a second nRF52840 on USB. To the computer it is the keyboard, with the same three HID interfaces (boot keyboard, NKRO and media, config) and its own USB IDs (1209:0002, "Dachshund60F Receiver"). Over the air it listens for the keyboard with Nordic's Enhanced ShockBurst (ESB) at 2 Mbps on 2475 MHz.

- Reports from the keyboard go straight to USB, so a key reaches the computer at the next 1 ms USB poll after the keyboard sends it.
- The computer's lock LEDs and USB state, and config requests from Doghouse, ride back to the keyboard in ESB acknowledgements. The keyboard polls for them every 10 ms while in use and every 100 ms while idle.
- If the keyboard goes quiet for 400 ms (asleep, off, out of range), every key is released on the computer. Doghouse requests then get `unreachable`, and Doghouse waits for the keyboard to come back.
- Packets are defined in `firmware/core/include/kb/link.h` and tested on the host with the other core tests.

It runs on Nordic's nRF52840 Dongle (PCA10059). The Dachshund60F-Dongle PCB uses the same button and LED pins (`boards/Dachshund60F/board.json`), so this firmware is meant for both.

## Build

From an NCS v3.4.1 shell, in the repo:

```
west build -p -b nrf52840dongle/nrf52840 -d build/dongle dongle
```

Add `-- -DEXTRA_CONF_FILE=release.conf` for a release build (no logging). Debug logs go to SEGGER RTT, which needs an SWD probe.

## Flash an nRF52840 Dongle (PCA10059)

The PCA10059 ships with Nordic's USB bootloader, and this build is linked to sit after it (at 0x1000).

1. Package the firmware: `nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req=0x00 --application build/dongle/dongle/zephyr/zephyr.hex --application-version 1 dongle.zip` (install the tool with `nrfutil install nrf5sdk-tools`).
2. Press the dongle's sideways RESET button. The red LED pulses and a serial port appears.
3. `nrfutil nrf5sdk-tools dfu usb-serial -pkg dongle.zip -p <port>` (`COM5` on Windows, `/dev/ttyACM0` on Linux).

nRF Connect for Desktop's Programmer app does the same from a window.

## Pair

1. Plug the dongle in. Its LED shows white for a moment.
2. Press the dongle's button (SW1 on the PCA10059, PAIR on the Dachshund dongle). The LED blinks blue for 30 s; press again to stop.
3. On the keyboard, press `2.4G Pair` (`RF_PAIR`), or use Doghouse: Connection → Pair a dongle.
4. The LED blinks green three times. The keyboard keeps the dongle across power cycles.

To use it, prefer it with the `2.4G` key (`OUT_2G4`) or in Doghouse. Keys also fall back to it when the preferred output isn't ready: USB first, then the dongle, then Bluetooth.

## LED

| LED | Meaning |
| --- | --- |
| White, briefly | Powered up |
| Blue, blinking | Pairing mode |
| Green, three blinks | Paired |
| Green for a second | The keyboard is in range |
| Red for a second | The keyboard went quiet |
| Off | Normal use, or the computer is asleep |

## Limits

- The link isn't encrypted yet, so someone nearby with the right radio could read keystrokes. Don't type passwords over the dongle until that's done (`SCHEDULED_TASKS.md`).
- One fixed channel. Wi-Fi channel 11 and Bluetooth advertising on 2480 MHz are close, so check range and dropouts on real hardware.
- Doghouse can't update the dongle's firmware yet; use nrfutil as above.
