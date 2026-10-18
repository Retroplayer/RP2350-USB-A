# VideoWriter example protocol

This module is an example client of `translator/keyboard.h`. It has no USB or
Pico includes and is testable with a fake clock and transmitter.

## Evidence and unresolved behavior

Primary source: Maarten Janssen's [The Keyboard](https://hackaday.io/project/25664/log/172155-the-keyboard),
part of [Anatomy of a Philips VideoWRITER](https://hackaday.io/project/25664-anatomy-of-a-philips-videowriter).
It describes a 20-keystroke FIFO, active-low `/Enable`, one byte per request,
9600 baud 8N1, no response when empty, and no release codes. Its layout gives
unshifted/shifted bytes. Original firmware may retry missed polls.

The user's [prior VideoWriter 250 research](https://chatgpt.com/share/6ad4fdfe-fdec-83ea-91d0-0d6841c3bd65)
also records [a Deskthority account](https://deskthority.net/viewtopic.php?t=19687)
of continued transmission while enable is held low, approximately 500 ms before
repeat and 100 ms between repeats. That account is carried forward from the
shared research, not independently measured here. The VGA32 prototype received
PS/2 keys and queued translations, but its supplied logs showed zero requests
and zero transmitted bytes. It did not validate VideoWriter communication.

Two modes are therefore available:

- `VW_REQUEST_EDGE` (default): one queued byte per captured request. A short
  pulse is remembered even if the line returns high before servicing it.
- `VW_ENABLE_LEVEL`: the same pulse response, plus successive queued bytes while
  the input stays low. This interpretation is experimental.

An empty request does not reserve a future byte in edge mode. The transmitter
callback is nonblocking; a busy callback retains the head byte. Responses after
the configured timeout are abandoned, retaining the byte for a later request.
The default **10 µs delay and 500 µs timeout are engineering placeholders**;
the article only says “a few µs.” They are not measured compatibility claims.
If multiple requests arrive before service, the latest replaces the pending
one and the missed counter increases; no unbounded response backlog is created.

## Mapping and repeat

The portable default map provides:

- US letters, digits and the punctuation documented in the original layout.
- Enter/keypad Enter `C6/C7`, Backspace `C4/C5`, Tab `C2/C3`, Delete `C0/C1`.
- Insert `92/93`, Application → Style `94/95`.
- Caps Lock `90/91` selected by Shift, following the source's byte pair; internal
  caps state toggles on logical press. Caps forces letters uppercase even with Shift.
- F1..F10 → the ten top-row VideoWriter positions, left to right:
  `96,98,9A,9C,9E,A0,B8,A2,A4,A6`, each shifted byte one greater.
- Up/Down/Left/Right → the cursor cluster's `B0/B2/B4/B6`, shifted bytes one greater.
- Space `20`; shifted Space `49`, exactly as published. This collides with `I`
  and needs verification on the machine.

These PC-to-VideoWriter physical key choices are example bindings. They are not
claims that PC function-key labels match the original key labels. The unlabeled
right-hand vertical keys (`A8/AA/AC`) have no default PC binding. Add entries to
an application-owned `vw_mapping` table if needed. A supplied table **replaces**
the default special-key table; letters/digits/punctuation remain fallback mappings.
Mappings can supply byte value zero without using it as a sentinel.

Ctrl/Alt/GUI chords, keypad digits, Escape, unsupported punctuation and unknown
keys produce no VideoWriter byte. They can first be mapped to supported logical
keys with the shared remapper. Caps byte interpretation and locale-specific ROM
translation still need hardware confirmation. USB keyboard LEDs are not synchronized.

Logical press bytes are queued immediately; logical releases only stop repeat.
The most recent repeatable key becomes the repeat key. Releasing it stops repeat
without resuming an older held key. Shift/caps changes affect subsequent repeat
bytes. Caps Lock itself and modifiers do not repeat. Delayed ticks generate at
most one repeat, avoiding catch-up bursts. FIFO capacity defaults to 20, can be
1..64, and drops new bytes when full while counting drops. Already queued bytes
retain the modifiers they had when encoded. No idle flushing occurs.

## Bench validation

Start with the VideoWriter disconnected and safe logic-level stimulus:

1. Confirm GPIO0 idle high. Queue `a`, pulse GPIO1 low, decode exactly `0x61` at
   9600 8N1. A frame takes about 1.042 ms. No byte may appear without a request.
2. Poll empty, then type a key: edge mode must wait for a new request.
3. Queue several keys; check one byte per pulse. Test a pulse shorter than a loop
   iteration; it must still be captured by the IRQ.
4. Hold enable low. Edge mode sends only one; level mode drains at UART speed.
   Raise enable and check that no additional frame starts except a pending pulse
   response. An in-flight UART frame completes normally.
5. Hold/release a key; measure repeat delay/interval. Exercise Shift, Caps Lock,
   special keys, remapping, unplug and FIFO overflow.

With a verified adapter and real machine, capture `/Enable` and Data together.
Measure pulse width, assertion-to-start-bit latency, accepted response window,
and whether the machine ever holds enable low across multiple frames. Adjust
`VW_RESPONSE_DELAY_US`, `VW_RESPONSE_TIMEOUT_US` and `VW_CONTINUOUS` accordingly.
Measure worst-case latency during USB enumeration, keyboard traffic, and CDC
connection/disconnection; the current foreground scheduler has no proven
microsecond latency bound. If measurements require tighter response timing,
move the target-side start scheduling into an alarm/PIO driver behind the same
API. Verify Shift+Space, Caps Lock bytes and locale behavior on the actual ROM.

Cloud tests validate logic and buildability; they cannot validate those waveforms.
