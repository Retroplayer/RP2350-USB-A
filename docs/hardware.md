# Waveshare RP2350-USB-A hardware

Verified against [Waveshare's wiki](https://www.waveshare.com/wiki/RP2350-USB-A)
and its [schematic](https://files.waveshare.com/wiki/RP2350-USB-A/RP2350-USB-A.pdf).

| Function | Wiring |
|---|---|
| USB-A D+ | GPIO12 via R12, 27 Ω |
| USB-A D− | GPIO13 via R11, 27 Ω |
| USB-A VBUS | VSYS, directly connected |
| USB-C D+/D− | RP2350 dedicated USB_DP/USB_DM via 27 Ω resistors |
| USB-C VBUS | VSYS |
| R13 | 1.5 kΩ GPIO12/D+ pull-up to 3.3 V |
| RGB LED | WS2812 on GPIO16; unused by this firmware |
| Flash | W25Q16, 2 MiB |

The Waveshare FAQ says **remove R13** for PIO USB host hot-plugging and for
low-speed keyboards/mice. It also says removing it prevents use of that PIO USB
port as a device. This concerns USB-A, not native USB-C programming/CDC. Inspect
your actual board revision and resistor population before reworking hardware.

The USB-A port is a GPIO/PIO USB interface, not the native controller. Firmware
uses Pico-PIO-USB on core 1 at 120 MHz, reserving PIO0 SM0/1/2, DMA0, and timer
alarm 2 through the upstream default configuration. Do not allocate those
resources to a target protocol. Other PIO blocks remain available. The
VideoWriter example uses UART0 instead. USB-C runs the native device CDC stack
on core 0. CDC output is nonblocking and can drop diagnostics if the PC is slow.

## Proposed VideoWriter adapter

These are **firmware GPIO allocations**, not modular connector contact numbers:

| Board pin | Direction | Target signal |
|---|---|---|
| GPIO0 | UART0 TX, 9600 8N1, idle high | VideoWriter keyboard Data |
| GPIO1 | input, falling-edge IRQ + sampled level | VideoWriter `/Enable` |
| GND | common | VideoWriter GND |

The board does not contain the VGA32 prototype's target-side level converters.
Use a suitable external interface and validate it before connecting the machine.
GPIO0 is a 3.3 V push-pull UART output; do not attach it to a circuit requiring
open-drain behavior without an appropriate driver. GPIO1 has a weak pull-up so
an unattached input stays quiet.

The [RP2350 datasheet mirrored by Waveshare](https://files.waveshare.com/wiki/common/Rp2350-datasheet.pdf)
identifies GPIO0/1 as FT digital I/O in §14.4. Tables 1430/1433 in §14.9 specify
up to 5.5 V at FT inputs **when IOVDD is 3.3 V**, but only 3.63 V with IOVDD off.
This conditional input tolerance does not provide 5 V output drive or make
arbitrary power sequencing safe. A directional input buffer with suitable
powered-off protection and a target-compatible output driver is the preferred
adapter approach until the actual electrical requirements are verified.

For initial testing, power the board/USB keyboard from USB-C and leave the
VideoWriter +5 V conductor disconnected. VSYS is directly tied to both USB
connectors: tying another supply to VSYS can tie it to the PC's USB supply.
There is no separately controlled USB-A VBUS switch shown in the schematic.
Do not assume the VideoWriter keyboard supply has enough current for a modern
USB keyboard. A target-powered design needs a separate power-budget and
supply-isolation decision.

Janssen's internal J7 pin order is GND, `/Enable`, Data, +5 V. It does **not**
establish contact numbering or orientation at the external modular connector.
Confirm the connector type, continuity and voltage polarity on the actual unit.

## First board checks

1. Inspect R13 and prepare the host port per Waveshare's instructions.
2. With the VideoWriter disconnected, enter BOOTSEL and copy the UF2 via USB-C.
3. Connect a boot-compatible USB keyboard to USB-A. Open USB-C CDC and inspect
   `host`, `queued`, `rejected`, `invalid`, and `rx_fail` counters.
4. Type and release a key; confirm one initial queued byte, software repeat while
   held, and no further repeat after release. Check unplug/replug and modifier keys.
5. With safe 3.3 V bench stimulus on GPIO1, pulse `/Enable`; observe GPIO0 with
   a logic analyzer configured for 9600 baud 8N1. Do not drive GPIO0 externally.
6. Only after verifying the external adapter and power arrangement, connect the
   real VideoWriter and perform the timing checks in `videowriter.md`.

No board or VideoWriter was physically attached during cloud validation.
