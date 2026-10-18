#ifndef EXAMPLE_VIDEOWRITER_H
#define EXAMPLE_VIDEOWRITER_H
#include "translator/keyboard.h"
#include <stddef.h>
#define VW_FIFO_MAX 64
/* A board implements this nonblocking byte transmitter. false means busy;
 * true means exactly one complete byte has been accepted for transmission. */
typedef bool (*vw_transmit)(void *ctx, uint8_t byte);
typedef enum { VW_REQUEST_EDGE, VW_ENABLE_LEVEL } vw_enable_mode;
typedef struct { uint8_t key, plain, shifted; bool repeat; } vw_mapping;
typedef struct {
    vw_enable_mode mode;
    uint32_t response_delay_us; /* tunable: exact original delay is unmeasured */
    uint32_t response_timeout_us;
    uint32_t repeat_delay_us, repeat_interval_us;
    uint8_t fifo_capacity; /* 1 .. VW_FIFO_MAX; original keyboard reportedly 20 */
    const vw_mapping *mappings;
    size_t mapping_count;
} vw_config;
typedef struct {
    vw_config config;
    vw_transmit transmit;
    void *io;
    uint8_t fifo[VW_FIFO_MAX], head, count;
    uint8_t repeat_key;
    uint64_t repeat_due, response_due, response_expires;
    bool caps, enable_low, pending;
    uint32_t queued, transmitted, dropped, requests, missed;
} vw_keyboard;
vw_config vw_default_config(void);
bool vw_init(vw_keyboard *v, const vw_config *config, vw_transmit tx, void *io);
kb_target vw_target(vw_keyboard *v);
/* Target-side signals, called in the same execution context as target callbacks.
 * falling=true records a pulse even if the line has already returned high. */
void vw_enable(vw_keyboard *v, bool low, bool falling, uint64_t now_us);
bool vw_encode(const vw_keyboard *v, uint8_t key, const kb_state *state,
               uint8_t *byte, bool *repeat);
#endif
