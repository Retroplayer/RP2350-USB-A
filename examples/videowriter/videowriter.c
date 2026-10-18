#include "videowriter.h"
#include <string.h>
/* Values transcribed from Janssen's layout, not copied from HIDman.
 * F1..F10 select the ten top-row keys from left to right. Arrow keys select
 * the cardinal positions of the four-way cursor cluster. Application=Style. */
static const vw_mapping defaults[] = {
    {0x28,0xc6,0xc7,true}, {0x58,0xc6,0xc7,true}, /* Enter, keypad Enter */
    {0x2a,0xc4,0xc5,true}, {0x2b,0xc2,0xc3,true},
    {0x2c,0x20,0x49,true}, /* Shift+Space: published value, needs hardware check */
    {0x39,0x90,0x91,false}, {0x49,0x92,0x93,true}, {0x4c,0xc0,0xc1,true},
    {0x52,0xb0,0xb1,true}, {0x51,0xb2,0xb3,true},
    {0x50,0xb4,0xb5,true}, {0x4f,0xb6,0xb7,true},
    {0x65,0x94,0x95,true},
    {0x3a,0x96,0x97,true}, {0x3b,0x98,0x99,true}, {0x3c,0x9a,0x9b,true},
    {0x3d,0x9c,0x9d,true}, {0x3e,0x9e,0x9f,true}, {0x3f,0xa0,0xa1,true},
    {0x40,0xb8,0xb9,true}, {0x41,0xa2,0xa3,true},
    {0x42,0xa4,0xa5,true}, {0x43,0xa6,0xa7,true},
};
vw_config vw_default_config(void) {
    return (vw_config){VW_REQUEST_EDGE, 10, 500, 500000, 100000, 20,
                       defaults, sizeof(defaults)/sizeof(defaults[0])};
}
bool vw_init(vw_keyboard *v, const vw_config *c, vw_transmit tx, void *io) {
    if (!c || !tx || !c->fifo_capacity || c->fifo_capacity > VW_FIFO_MAX ||
        !c->repeat_interval_us || c->response_timeout_us < c->response_delay_us ||
        (c->mapping_count && !c->mappings) || c->mode > VW_ENABLE_LEVEL) return false;
    memset(v, 0, sizeof(*v));
    v->config = *c;
    v->transmit = tx;
    v->io = io;
    return true;
}
bool vw_encode(const vw_keyboard *v, uint8_t key, const kb_state *s,
               uint8_t *byte, bool *repeat) {
    bool shift = kb_shift(s);
    *repeat = true;
    /* Ctrl/Alt/GUI chords have no documented automatic translation.
     * Explicit custom mappings may define them in a future richer keymap. */
    if (kb_ctrl(s) || kb_alt(s) || kb_down(s,0xe3) || kb_down(s,0xe7)) return false;
    for (size_t i = 0; i < v->config.mapping_count; ++i) {
        const vw_mapping *m = &v->config.mappings[i];
        if (m->key == key) {
            *byte = shift ? m->shifted : m->plain;
            *repeat = m->repeat;
            return true;
        }
    }
    if (key >= 0x04 && key <= 0x1d) {
        *byte = (uint8_t)((v->caps || shift ? 'A' : 'a') + key - 0x04);
        return true;
    }
    if (key >= 0x1e && key <= 0x27) {
        *byte = (uint8_t)(shift ? "!@#$%^&*()"[key-0x1e] : "1234567890"[key-0x1e]);
        return true;
    }
    static const struct {uint8_t key; char plain, shifted;} punctuation[] = {
        {0x2d,'-','_'},{0x2e,'=','+'},{0x2f,'[','{'},{0x30,']','}'},
        {0x33,';',':'},{0x34,'\'', '"'},{0x36,',','<'},{0x37,'.','>'},{0x38,'/','?'}
    };
    for (size_t i=0; i<sizeof(punctuation)/sizeof(punctuation[0]); ++i)
        if (punctuation[i].key == key) {
            *byte = (uint8_t)(shift ? punctuation[i].shifted : punctuation[i].plain);
            return true;
        }
    return false;
}
static void enqueue(vw_keyboard *v, uint8_t byte) {
    if (v->count == v->config.fifo_capacity) { ++v->dropped; return; }
    v->fifo[(v->head + v->count) % v->config.fifo_capacity] = byte;
    ++v->count;
    ++v->queued;
}
static void key(void *ctx, const kb_event *e, const kb_state *s) {
    vw_keyboard *v = ctx;
    if (!e->pressed) {
        if (e->key == v->repeat_key) v->repeat_key = 0;
        return;
    }
    if (e->key == 0x39) v->caps = !v->caps;
    uint8_t byte;
    bool repeat;
    if (!vw_encode(v, e->key, s, &byte, &repeat)) return;
    enqueue(v, byte);
    if (repeat) {
        v->repeat_key = e->key;
        v->repeat_due = e->time_us + v->config.repeat_delay_us;
    }
}
void vw_enable(vw_keyboard *v, bool low, bool falling, uint64_t now) {
    bool was_low = v->enable_low;
    v->enable_low = low;
    if (falling || (low && !was_low)) {
        ++v->requests;
        if (v->pending) ++v->missed;
        /* An empty request never becomes credit for a future key. */
        v->pending = v->count != 0;
        v->response_due = now + v->config.response_delay_us;
        v->response_expires = now + v->config.response_timeout_us;
    }
}
static void tick(void *ctx, uint64_t now, const kb_state *s) {
    vw_keyboard *v = ctx;
    if (v->repeat_key && now >= v->repeat_due) {
        uint8_t byte;
        bool repeat;
        if (kb_down(s, v->repeat_key) && vw_encode(v, v->repeat_key, s, &byte, &repeat))
            enqueue(v, byte);
        /* No catch-up burst after a delayed scheduler iteration. */
        v->repeat_due = now + v->config.repeat_interval_us;
    }
    if (!v->pending && v->config.mode == VW_ENABLE_LEVEL && v->enable_low &&
        v->count && now >= v->response_due) {
        if (v->transmit(v->io, v->fifo[v->head])) {
            v->head = (uint8_t)((v->head + 1) % v->config.fifo_capacity);
            --v->count;
            ++v->transmitted;
        }
        return;
    }
    if (!v->pending) return;
    if (now > v->response_expires) { v->pending = false; ++v->missed; return; }
    if (now < v->response_due || !v->count) return;
    if (v->transmit(v->io, v->fifo[v->head])) {
        v->head = (uint8_t)((v->head + 1) % v->config.fifo_capacity);
        --v->count;
        ++v->transmitted;
        v->pending = false;
    }
}
static void reset(void *ctx) {
    vw_keyboard *v = ctx;
    v->head = v->count = v->repeat_key = 0;
    v->pending = v->caps = false;
}
kb_target vw_target(vw_keyboard *v) {
    static const kb_target_api api = {key, tick, reset};
    return (kb_target){&api, v};
}
