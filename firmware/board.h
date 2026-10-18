#ifndef WAVESHARE_BOARD_H
#define WAVESHARE_BOARD_H
/* Waveshare schematic: https://files.waveshare.com/wiki/RP2350-USB-A/RP2350-USB-A.pdf
 * Native USB on USB-C. USB-A uses GPIO12 (D+) / GPIO13 (D-).
 * R13 must be removed for USB-A host hot-plug/low-speed support per wiki FAQ. */
#define BOARD_USB_DP 12
/* Example external interface allocation, NOT VideoWriter connector numbering.
 * External level conversion required; see docs/hardware.md. */
#define BOARD_VW_DATA 0 /* UART0 TX */
#define BOARD_VW_ENABLE 1
#ifndef VW_CONTINUOUS
#define VW_CONTINUOUS 0
#endif
#ifndef VW_RESPONSE_DELAY_US
#define VW_RESPONSE_DELAY_US 10
#endif
#ifndef VW_RESPONSE_TIMEOUT_US
#define VW_RESPONSE_TIMEOUT_US 500
#endif
#endif
