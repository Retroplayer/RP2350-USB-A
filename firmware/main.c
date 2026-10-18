/* Composition root: the only layer that knows both input and example target. */
#include "board.h"
#include "usb_input.h"
#include "translator/boot_keyboard.h"
#include "videowriter.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "tusb.h"
#include <stdio.h>

static queue_t requests;
static volatile uint32_t request_overflows;
static kb_pipeline pipeline;
static kb_boot_keyboard keyboards[KB_SOURCES];
static vw_keyboard videowriter;
static uint32_t input_resets, invalid_reports;
static void enable_irq(uint gpio, uint32_t events) {
    (void)gpio;
    if (events & GPIO_IRQ_EDGE_FALL) {
        uint64_t now = time_us_64();
        if (!queue_try_add(&requests, &now)) ++request_overflows;
    }
}
static bool transmit(void *ctx, uint8_t byte) {
    (void)ctx;
    /* Don't queue behind another byte: protocol controls when each frame starts. */
    if ((uart_get_hw(uart0)->fr & UART_UARTFR_BUSY_BITS) || !uart_is_writable(uart0)) return false;
    uart_putc_raw(uart0, byte);
    return true;
}
static void input_event(void *ctx, const kb_event *event) { kb_pipeline_event(ctx, event); }
static void diagnostics(uint64_t now) {
    static uint64_t due;
    if (now < due) return;
    due = now + 1000000;
    if (!tud_cdc_connected()) return;
    char line[240];
    int n = snprintf(line, sizeof(line),
        "host=%u queued=%u tx=%lu requests=%lu dropped=%lu missed=%lu resets=%lu invalid=%lu rejected=%u rx_fail=%u irq_drop=%lu\r\n",
        usb_input_ready(), videowriter.count, (unsigned long)videowriter.transmitted,
        (unsigned long)videowriter.requests, (unsigned long)videowriter.dropped,
        (unsigned long)videowriter.missed, (unsigned long)input_resets,
        (unsigned long)invalid_reports, usb_input_rejected(), usb_input_receive_failures(),
        (unsigned long)request_overflows);
    if (n > 0 && (size_t)n < sizeof(line) && tud_cdc_write_available() >= (uint32_t)n)
        tud_cdc_write(line, (uint32_t)n);
    tud_cdc_write_flush();
    while (tud_cdc_available()) { char discard[32]; tud_cdc_read(discard, sizeof(discard)); }
}
int main(void) {
    /* PIO USB needs a multiple of 12 MHz. Use the upstream 120 MHz baseline. */
    set_sys_clock_khz(120000, true);
    vw_config config = vw_default_config();
    config.mode = VW_CONTINUOUS ? VW_ENABLE_LEVEL : VW_REQUEST_EDGE;
    config.response_delay_us = VW_RESPONSE_DELAY_US;
    config.response_timeout_us = VW_RESPONSE_TIMEOUT_US;
    if (!vw_init(&videowriter, &config, transmit, NULL)) panic("Invalid VideoWriter configuration");
    kb_pipeline_init(&pipeline, vw_target(&videowriter));
    /* User key remaps belong here; e.g. kb_pipeline_map(&pipeline, 0x29, 0x65)
     * maps Escape to the logical Application/Style key. Identity is default. */
    uart_init(uart0, 9600);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart0, false, false);
    uart_set_fifo_enabled(uart0, false);
    gpio_set_function(BOARD_VW_DATA, GPIO_FUNC_UART);
    gpio_init(BOARD_VW_ENABLE);
    gpio_set_dir(BOARD_VW_ENABLE, GPIO_IN);
    gpio_pull_up(BOARD_VW_ENABLE); /* quiet when external adapter is absent */
    queue_init(&requests, sizeof(uint64_t), 16);
    gpio_set_irq_enabled_with_callback(BOARD_VW_ENABLE, GPIO_IRQ_EDGE_FALL, true, enable_irq);
    tud_init(0);
    usb_input_start();
    for (;;) {
        uint64_t now = time_us_64(), requested;
        bool low = !gpio_get(BOARD_VW_ENABLE);
        /* Consume requests before new key reports: empty polls don't gain credit. */
        while (queue_try_remove(&requests, &requested))
            vw_enable(&videowriter, low, true, requested);
        vw_enable(&videowriter, low, false, now);
        kb_pipeline_tick(&pipeline, now);
        if (usb_input_recover()) {
            kb_pipeline_reset(&pipeline);
            for (unsigned i=0; i<KB_SOURCES; ++i) kb_boot_init(&keyboards[i]);
            ++input_resets;
        }
        usb_key_report report;
        /* Bound input work so target timing gets another turn promptly. */
        for (unsigned i=0; i<8 && usb_input_pop(&report); ++i)
            if (!kb_boot_report(&keyboards[report.source], report.source, report.report,
                                8, report.time_us, input_event, &pipeline)) ++invalid_reports;
        tud_task();
        diagnostics(time_us_64());
        tight_loop_contents();
    }
}
