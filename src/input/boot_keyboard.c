#include "translator/boot_keyboard.h"
#include <string.h>
void kb_boot_init(kb_boot_keyboard *k) { memset(k, 0, sizeof(*k)); }
static void emit(uint8_t source, uint8_t key, bool pressed, uint64_t now,
                 kb_event_sink sink, void *ctx) {
    kb_event e = {now, source, key, pressed};
    sink(ctx, &e);
}
bool kb_boot_report(kb_boot_keyboard *k, uint8_t source, const uint8_t *r,
                    size_t len, uint64_t now, kb_event_sink sink, void *ctx) {
    if (source >= KB_SOURCES || !r || len != 8) return false;
    uint8_t next[KB_KEYS] = {0};
    for (unsigned i = 2; i < 8; ++i) {
        if (r[i] >= 1 && r[i] <= 3) return false; /* rollover: retain last valid state */
        if (r[i] >= 0xe0) return false; /* modifiers belong in byte 0 */
        if (r[i]) next[r[i]] = 1;
    }
    for (unsigned i = 0; i < 8; ++i) next[0xe0 + i] = (r[0] >> i) & 1u;
    /* Modifier state is delivered before normal keys from the same report. */
    for (unsigned i = 0xe0; i < KB_KEYS; ++i)
        if (next[i] != k->down[i]) emit(source, (uint8_t)i, next[i], now, sink, ctx);
    for (unsigned i = 1; i < 0xe0; ++i)
        if (next[i] != k->down[i]) emit(source, (uint8_t)i, next[i], now, sink, ctx);
    memcpy(k->down, next, sizeof(next));
    return true;
}
void kb_boot_disconnect(kb_boot_keyboard *k, uint8_t source, uint64_t now,
                        kb_event_sink sink, void *ctx) {
    if (source >= KB_SOURCES) return;
    /* Release ordinary keys first, then modifiers. */
    for (unsigned i = 1; i < KB_KEYS; ++i)
        if (k->down[i]) emit(source, (uint8_t)i, false, now, sink, ctx);
    kb_boot_init(k);
}
