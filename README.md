# RP2350 USB keyboard protocol translator

A reusable C11 keyboard framework for the **Waveshare RP2350-USB-A**. USB input,
key remapping, and target protocol implementations are separate libraries.
VideoWriter is the first example consumer of the target API.

```text
USB-A → PIO USB / TinyUSB → boot report decoder → key events
                                                   ↓
                                           remapping + key state
                                                   ↓
                                             target API
                                                   ↓
                                       VideoWriter example → UART
```

The portable code does not depend on the Pico SDK or TinyUSB. The USB input
code does not include or call VideoWriter. `firmware/main.c` connects the modules.

## Build and test

With CMake and a native C/C++ toolchain:

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure
```

In the Debian cloud environment, install the tools and pinned SDK dependencies,
run the host tests, and generate the board firmware:

```sh
./scripts/setup-cloud.sh
source /workspace/tooling/activate.sh
```

Firmware output: `build-pico/firmware/videowriter_usb.uf2`.
See [development](docs/development.md) for normal system installations and rebuilds.

## Current scope

- Up to four boot-keyboard HID interfaces, six non-modifier keys per report.
- One-to-one key remaps, disabled keys, modifier remaps, and held-key state.
- Correct release pairing when remaps change or multiple inputs map to one key.
- VideoWriter example: key encoding, configurable mappings, repeat, bounded FIFO,
  and selectable request-edge or experimental enable-level behavior.
- USB-C CDC diagnostics, isolated from the USB-A host stack on the second core.

Generic report-descriptor/NKRO support, consumer/media usage pages, macros,
runtime configuration storage, and keyboard LED feedback are not implemented.

**Validation:** host tests and RP2350 cross-compilation can run in the cloud.
USB enumeration, physical timing, and operation with a VideoWriter require bench
validation. The included firmware is a hardware bring-up candidate.

Read [hardware](docs/hardware.md) before connecting the target: Waveshare documents
an R13 removal for low-speed USB host support; the external VideoWriter adapter
and its power arrangement are not supplied by the board.

See [the API](docs/api.md) to add a protocol and [VideoWriter notes](docs/videowriter.md)
for mappings, assumptions, sources, and the test procedure.
