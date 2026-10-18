#ifndef USB_INPUT_H
#define USB_INPUT_H
#include <stdbool.h>
#include <stdint.h>
/* Application-facing input transport boundary. No target references. */
typedef struct { uint8_t source, report[8]; uint64_t time_us; } usb_key_report;
void usb_input_start(void);
bool usb_input_pop(usb_key_report *report);
bool usb_input_recover(void); /* drops queued reports and requests fresh snapshots */
unsigned usb_input_rejected(void);
unsigned usb_input_receive_failures(void);
bool usb_input_ready(void);
#endif
