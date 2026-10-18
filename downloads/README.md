# Firmware download

[Download videowriter_usb.uf2](https://github.com/Retroplayer/RP2350-USB-A/raw/refs/heads/main/downloads/videowriter_usb.uf2)

Built from source commit `9c79050fc12f4422eccb93525a510c2692a341ff` for the
Waveshare RP2350-USB-A. Verify the file against `SHA256SUMS`.

This is a hardware bring-up prototype. The native tests and firmware build pass;
physical USB enumeration and VideoWriter operation have not been validated.
USB input currently accepts boot-protocol keyboards only.

Read [hardware requirements](../docs/hardware.md) before flashing or connecting
the target, including Waveshare's R13 guidance and the external VideoWriter
adapter and power requirements.
