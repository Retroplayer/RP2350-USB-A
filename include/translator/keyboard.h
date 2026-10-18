#ifndef TRANSLATOR_KEYBOARD_H
#define TRANSLATOR_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

/* Transport-neutral key identities use the USB HID Keyboard/Keypad usage values.
 * No USB reports, device addresses, SDK types or target bytes cross this API. */
#define KB_KEYS 256
#define KB_SOURCES 4

typedef struct {
    uint64_t time_us; /* monotonic; caller supplies the clock */
    uint8_t source;   /* stable input slot, 0 .. KB_SOURCES-1 */
    uint8_t key;      /* keyboard/keypad usage; 0 means disabled mapping */
    bool pressed;
} kb_event;
typedef struct { uint16_t held[KB_KEYS]; } kb_state;
static inline bool kb_down(const kb_state *s, uint8_t key) { return s->held[key] != 0; }
static inline bool kb_shift(const kb_state *s) { return kb_down(s, 0xe1) || kb_down(s, 0xe5); }
static inline bool kb_ctrl(const kb_state *s) { return kb_down(s, 0xe0) || kb_down(s, 0xe4); }
static inline bool kb_alt(const kb_state *s) { return kb_down(s, 0xe2) || kb_down(s, 0xe6); }

typedef struct {
    void (*key)(void *ctx, const kb_event *event, const kb_state *state);
    void (*tick)(void *ctx, uint64_t now_us, const kb_state *state);
    void (*reset)(void *ctx); /* clear queued output and repeat after input loss */
} kb_target_api;
typedef struct { const kb_target_api *api; void *ctx; } kb_target;

typedef struct {
    uint8_t map[KB_KEYS];
    uint16_t active[KB_SOURCES][KB_KEYS]; /* destination+1; 0 means not held */
    kb_state state;
    kb_target target;
} kb_pipeline;
void kb_pipeline_init(kb_pipeline *p, kb_target target);
void kb_pipeline_map(kb_pipeline *p, uint8_t from, uint8_t to);
void kb_pipeline_event(kb_pipeline *p, const kb_event *event);
void kb_pipeline_tick(kb_pipeline *p, uint64_t now_us);
void kb_pipeline_release_source(kb_pipeline *p, uint8_t source, uint64_t now_us);
void kb_pipeline_reset(kb_pipeline *p); /* retains configured mappings */
#endif
