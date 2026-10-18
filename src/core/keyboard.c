#include "translator/keyboard.h"
#include <string.h>

void kb_pipeline_init(kb_pipeline *p, kb_target target) {
    memset(p, 0, sizeof(*p));
    p->target = target;
    for (unsigned i = 0; i < KB_KEYS; ++i) p->map[i] = (uint8_t)i;
}
void kb_pipeline_map(kb_pipeline *p, uint8_t from, uint8_t to) { p->map[from] = to; }
void kb_pipeline_event(kb_pipeline *p, const kb_event *e) {
    if (e->source >= KB_SOURCES || !e->key) return;
    uint16_t *active = &p->active[e->source][e->key];
    uint8_t mapped;
    if (e->pressed) {
        if (*active) return; /* duplicate make */
        mapped = p->map[e->key];
        *active = (uint16_t)mapped + 1u;
        if (!mapped) return;
        if (p->state.held[mapped]++ != 0) return;
    } else {
        if (!*active) return;
        mapped = (uint8_t)(*active - 1u);
        *active = 0;
        if (!mapped) return;
        if (--p->state.held[mapped] != 0) return;
    }
    kb_event out = *e;
    out.key = mapped;
    if (p->target.api->key) p->target.api->key(p->target.ctx, &out, &p->state);
}
void kb_pipeline_tick(kb_pipeline *p, uint64_t now_us) {
    if (p->target.api->tick) p->target.api->tick(p->target.ctx, now_us, &p->state);
}
void kb_pipeline_release_source(kb_pipeline *p, uint8_t source, uint64_t now_us) {
    if (source >= KB_SOURCES) return;
    for (unsigned k = 1; k < KB_KEYS; ++k) {
        kb_event e = {now_us, source, (uint8_t)k, false};
        kb_pipeline_event(p, &e);
    }
}
void kb_pipeline_reset(kb_pipeline *p) {
    memset(p->active, 0, sizeof(p->active));
    memset(&p->state, 0, sizeof(p->state));
    if (p->target.api->reset) p->target.api->reset(p->target.ctx);
}
