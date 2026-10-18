#ifndef BOARDS_WAVESHARE_RP2350_USB_A_H
#define BOARDS_WAVESHARE_RP2350_USB_A_H
// pico_cmake_set PICO_PLATFORM=rp2350
#include "boards/pico2.h"
/* Waveshare has W25Q16 (2 MiB), unlike Pico 2's 4 MiB. */
#undef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#undef PICO_DEFAULT_LED_PIN
#endif
