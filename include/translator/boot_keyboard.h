#ifndef TRANSLATOR_BOOT_KEYBOARD_H
#define TRANSLATOR_BOOT_KEYBOARD_H
#include "translator/keyboard.h"
#include <stddef.h>
/* Input adapter only. Supports the 8-byte HID boot keyboard format, not NKRO. */
typedef void (*kb_event_sink)(void *ctx, const kb_event *event);
typedef struct { uint8_t down[KB_KEYS]; } kb_boot_keyboard;
void kb_boot_init(kb_boot_keyboard *k);
bool kb_boot_report(kb_boot_keyboard *k, uint8_t source, const uint8_t *report,
                    size_t len, uint64_t now_us, kb_event_sink sink, void *ctx);
void kb_boot_disconnect(kb_boot_keyboard *k, uint8_t source, uint64_t now_us,
                        kb_event_sink sink, void *ctx);
#endif
