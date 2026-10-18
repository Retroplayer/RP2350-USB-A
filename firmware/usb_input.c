#include "usb_input.h"
#include "board.h"
#include "translator/keyboard.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/time.h"
#include "pio_usb.h"
#include "tusb.h"
#include <stdatomic.h>
#include <string.h>

static queue_t reports;
static atomic_bool overflow, resync, ready;
static atomic_uint rejected, receive_failures;
typedef struct {
    uint8_t address, instance, report[8];
    bool receiving;
} input_slot;
static input_slot slots[KB_SOURCES]; /* exclusively owned by USB host core */
static void post(unsigned i) {
    usb_key_report r = {.source = (uint8_t)i, .time_us = time_us_64()};
    memcpy(r.report, slots[i].report, 8);
    if (!queue_try_add(&reports, &r)) atomic_store(&overflow, true);
}
static int find(uint8_t address, uint8_t instance) {
    for (unsigned i = 0; i < KB_SOURCES; ++i)
        if (slots[i].address == address && slots[i].instance == instance) return (int)i;
    return -1;
}
static void receive(input_slot *s) {
    s->receiving = tuh_hid_receive_report(s->address, s->instance);
    if (!s->receiving) atomic_fetch_add(&receive_failures, 1);
}
void tuh_hid_mount_cb(uint8_t address, uint8_t instance, const uint8_t *desc, uint16_t len) {
    (void)desc; (void)len;
    if (tuh_hid_interface_protocol(address, instance) != HID_ITF_PROTOCOL_KEYBOARD ||
        tuh_hid_get_protocol(address, instance) != HID_PROTOCOL_BOOT) {
        atomic_fetch_add(&rejected, 1);
        return;
    }
    for (unsigned i = 0; i < KB_SOURCES; ++i) {
        if (!slots[i].address) {
            slots[i] = (input_slot){.address = address, .instance = instance};
            post(i);
            receive(&slots[i]);
            return;
        }
    }
    atomic_fetch_add(&rejected, 1);
}
void tuh_hid_umount_cb(uint8_t address, uint8_t instance) {
    int i = find(address, instance);
    if (i < 0) return;
    memset(&slots[i], 0, sizeof(slots[i]));
    post((unsigned)i); /* empty snapshot releases only this interface */
}
void tuh_hid_report_received_cb(uint8_t address, uint8_t instance,
                                const uint8_t *report, uint16_t len) {
    int i = find(address, instance);
    if (i < 0) return;
    if (len == 8) {
        memcpy(slots[i].report, report, 8);
        post((unsigned)i);
    } else atomic_fetch_add(&rejected, 1);
    receive(&slots[i]);
}
static void host_core(void) {
    pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
    config.pin_dp = BOARD_USB_DP;
    /* Reserve PIO0 SM0..2 and DMA0 for USB. Target example uses hardware UART. */
    if (!tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &config) || !tuh_init(1))
        return;
    atomic_store(&ready, true);
    for (;;) {
        tuh_task();
        if (atomic_exchange(&resync, false))
            for (unsigned i=0; i<KB_SOURCES; ++i) post(i);
        for (unsigned i=0; i<KB_SOURCES; ++i)
            if (slots[i].address && !slots[i].receiving) receive(&slots[i]);
    }
}
void usb_input_start(void) {
    queue_init(&reports, sizeof(usb_key_report), 64);
    multicore_launch_core1(host_core);
}
bool usb_input_pop(usb_key_report *r) { return queue_try_remove(&reports, r); }
bool usb_input_recover(void) {
    if (!atomic_exchange(&overflow, false)) return false;
    usb_key_report r;
    while (usb_input_pop(&r)) {}
    atomic_store(&resync, true);
    return true;
}
unsigned usb_input_rejected(void) { return atomic_load(&rejected); }
unsigned usb_input_receive_failures(void) { return atomic_load(&receive_failures); }
bool usb_input_ready(void) { return atomic_load(&ready); }
