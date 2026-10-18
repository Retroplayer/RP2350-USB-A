# Development environment

Use the existing checkout. Cloud tasks already run in an isolated environment;
there is no need to create a Git worktree for setup or development.

## Dependencies

| Dependency | Tested version / revision |
|---|---|
| CMake | 3.31.6 |
| Ninja | 1.11.1 |
| Native GCC | 14.2.0 |
| Arm GNU compiler | 14.2.1 (Debian 15:14.2.rel1-1) |
| Pico SDK | 2.2.0 / `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` |
| TinyUSB | SDK submodule `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` |
| Pico-PIO-USB | 0.7.2 / `3c1eec341a5232640e4c00628b889b641af34b28` |
| picotool | 2.2.0 / `a7eb3988f0645239185fadb4e25d8279478c2dbb` |

The SDK and PIO USB versions build together without the source modifications
suggested for the older SDK 2.1.0/PIO USB 0.5.3 demo on Waveshare's wiki. No
upstream source is patched, and no verification is disabled.

## Debian cloud setup

Run `./scripts/setup-cloud.sh`. It installs a private rootless toolchain under
`/workspace/tooling`, dependencies under `/workspace/deps`, and builds in ignored
project directories. Override `TRANSLATOR_TOOLS`/`TRANSLATOR_DEPS` to choose other
cache locations. It preserves existing dependency checkouts and rejects changed
or unexpected revisions. Debian package metadata is signature-verified and APT
verifies downloaded package hashes; extracting packages does not run maintainer
scripts. Debian's normal newlib alternative links are recreated privately.
CMake/Ninja wheels use HTTPS and pinned versions. The rootless script requires
Debian 13-compatible system libraries, archive keyring, Python/pip, Git, make,
GCC/G++, dpkg-deb, and network access to deb.debian.org, GitHub, PyPI and
files.pythonhosted.org. Those shared tools are supplied in this cloud image.
APT package versions follow the Debian trixie repository; the table records the
versions actually tested rather than claiming a frozen compiler package mirror.

New shells need activation; no long-running services are required:

```sh
source /workspace/tooling/activate.sh
cd /workspace/RP2350-USB-A
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure
cmake --build build-pico -j4
```

The setup script itself runs these checks and verifies the UF2 exists. On an
already installed environment it reuses valid tools and dependency outputs.

## Other Linux installations

Install CMake, Ninja, Git, Python3, a native C/C++ toolchain, Arm GNU embedded
GCC/G++, and newlib using your distribution's packages or verified official
releases. Clone Pico SDK 2.2.0 and initialize `lib/tinyusb`; clone Pico-PIO-USB
0.7.2. Set `PICO_SDK_PATH` and `PICO_PIO_USB_PATH` to their absolute paths.
If the Arm compiler is not on PATH, set `PICO_TOOLCHAIN_PATH` to its installation
prefix. SDK CMake builds picotool 2.2.0 and pioasm as host utilities as needed.

From the project root:

```sh
cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure
cmake -S . -B build-pico -G Ninja -DBUILD_FIRMWARE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-pico -j4
```

The custom `waveshare_rp2350_usb_a` board header uses Pico 2 silicon defaults
with the correct 2 MiB flash size and no Pico LED assumption. The firmware
selects the Waveshare USB pins explicitly. Native and firmware builds use
separate build directories because they have different compilers.

For the experimental enable-level mode:

```sh
cmake -S . -B build-pico -DVW_CONTINUOUS=1
cmake --build build-pico -j4
```

Restore `-DVW_CONTINUOUS=0` for the default. Response delay and timeout are CMake
cache variables too. Edit logical remaps in the composition root or call the
public API from your own application. No credentials or server processes are
required. GitHub access uses the existing platform HTTPS authentication.

The native test executable uses explicit checks that remain enabled in Release.
For memory/undefined-behavior checking, configure a separate native build:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitize -j4
ctest --test-dir build-sanitize --output-on-failure
```

## Flash and inspect

Read `hardware.md` first. Hold BOOT and connect USB-C (or use BOOT/RESET) to enter
the RP2350 boot drive, then copy `build-pico/firmware/videowriter_usb.uf2` onto it.
USB-C CDC emits one status line per second when connected; its baud selection
does not control target UART speed. `host=1` means USB host initialization
completed, not that a keyboard was enumerated. `rejected` counts unsupported
interfaces or wrong-length reports, `invalid` counts rejected boot packets,
`rx_fail` counts report submission failures, and `resets` counts input queue
recovery. Target counters report queued depth, transmitted bytes, request count,
FIFO drops, and expired/replaced requests. `irq_drop` counts lost request timestamps.
The CDC VID/PID is a development placeholder; assign a legitimate identity before
product distribution. The locally built picotool is used for UF2 generation and
inspection and is intentionally compiled without live USB support.
