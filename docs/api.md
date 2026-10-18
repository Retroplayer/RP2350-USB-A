# Module boundaries and reusable API

`include/translator/keyboard.h` is the shared contract. It uses C11 standard
integer and boolean types only. Key identities are Keyboard/Keypad HID usage
numbers (page 0x07), used as a transport-independent naming convention. A future
PS/2 or synthetic input adapter can produce exactly the same `kb_event` objects.

| Module | Inputs | Outputs / dependencies |
|---|---|---|
| `firmware/usb_input.c` | TinyUSB host callbacks | Timestamped boot-report packets; no target dependency |
| `src/input/boot_keyboard.c` | Eight-byte boot report | Normalized press/release events; no target dependency |
| `src/core/keyboard.c` | Events + keymap | Logical events + aggregate key state; no USB/board dependency |
| Target API consumer | Logical events + state + ticks | Target-specific behavior; no USB dependency |
| `firmware/main.c` | Board signals and input packets | Composition, clock, scheduling, UART callback, CDC status |

## Adding a target

Implement any needed members of `kb_target_api`:

```c
static void key(void *ctx, const kb_event *e, const kb_state *state);
static void tick(void *ctx, uint64_t now_us, const kb_state *state);
static void reset(void *ctx);
static const kb_target_api api = { key, tick, reset };

kb_pipeline_init(&pipeline, (kb_target){ &api, &my_target });
```

The API table and context must outlive the pipeline. Callback pointers may be
NULL. `state` is updated before `key()` runs; its storage remains owned by the
pipeline. All callbacks and pipeline operations execute serially on one core.
Do not call them from an interrupt or retain an event pointer. Board interrupt
handlers should capture signals into a queue and let the composition layer
invoke target-specific signal functions. A target can use injected board
services such as a nonblocking transmitter, GPIO access, or a PIO driver.

`tick()` gets a monotonic 64-bit microsecond time, not an implicit SDK clock.
Callbacks must return promptly. A target owns protocol commands, framing,
repeat policy, output queues, and any target-specific key translation. The
pipeline owns input state and remapping. USB reports never enter a target.

A target with bidirectional signaling can expose additional functions on its
own module, just as VideoWriter exposes `vw_enable()`. This keeps unrelated
protocol signals out of the common keyboard API.

## Remapping

Identity mapping is the default. Configure it independently of input handling:

```c
kb_pipeline_map(&pipeline, 0x39, 0xe0); /* Caps Lock → Left Control */
kb_pipeline_map(&pipeline, 0x29, 0x65); /* Escape → Application (VideoWriter Style) */
kb_pipeline_map(&pipeline, 0x46, 0);    /* disable Print Screen */
```

Keys and mappings are 8-bit Keyboard/Keypad usage IDs. Zero is reserved for
no key. Source IDs are 0..3, stable until disconnect. Configuration is source
code/API driven; there is no interactive remapping UI or persistent settings.

Each input key remembers its destination while held, so changing the map cannot
leave the previous destination stuck. Destination reference counts combine
multiple physical keys and keyboards: targets see one press on the first holder
and one release on the last. The event's source identifies the transition's
origin, not every holder. `kb_pipeline_release_source()` releases one source.
`kb_pipeline_reset()` clears all state and tells the target to discard pending
output/repeat; configured mappings survive. A frame already started in hardware
cannot be recalled by reset.

Modifiers are normal key identities 0xe0..0xe7. Boot input delivers modifier
changes before ordinary key changes from the same report. Macros, chords as
mapping rules, and arbitrary usage pages are future API extensions.

## USB adapter and overload

Only keyboard interfaces successfully enumerated in boot protocol are accepted.
Report-only interfaces are counted as rejected; no descriptor guessing occurs.
Malformed reports and rollover/error reports preserve the last valid key state.
Repeated identical reports generate no duplicate press events.

Core 1 owns TinyUSB host calls, PIO USB, and a cache per keyboard interface.
Core 0 owns decoding, remapping, and the target. A 64-packet SDK queue crosses
cores. USB unmount posts an empty report, releasing that interface's held keys.
If the queue overflows, core 0 clears target/key state, drains input and asks
core 1 to post fresh snapshots. This deliberately sacrifices queued keystrokes
under overload to recover held state and avoid endless repeat. Recovery may
re-emit a held key; it is not lossless input delivery. Counters expose recovery
and malformed-report events. No automatic idle FIFO flushing is performed.
