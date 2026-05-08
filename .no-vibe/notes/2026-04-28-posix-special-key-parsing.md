# Session Synthesis: posix special-key parsing
Date: 2026-04-28
Mode: concept

## What You Built
1. Made event-shape output explicit (`type/key/ch/mod/shape`) in the keyboard example.
2. Added ESC-tail lookahead helper for short escape-sequence capture.
3. Mapped `ESC [ A/B/C/D` to `LT_KEY_ARROW_*`.
4. Preserved printable-input invariant via dedicated char-event emission (`key=0`, `ch!=0`).
5. Added standalone Escape handling (`LT_KEY_ESC`) including canonical-mode newline case.
6. Added and corrected a guard assertion so key-events never carry both `key` and `ch` simultaneously.
7. Validated three concrete paths: printable char, Escape, and arrow key.

## Mental Model
For keyboard events, `type` answers "what category", while `key` and `ch` answer "which input identity". Named/special keys populate `key` and clear `ch`; printable characters populate `ch` and keep `key=0`. ESC-prefixed input is a dispatch point: inspect following bytes to decide whether it is a standalone Escape key or a special-key sequence.

## Advanced Next Directions
- Add extended ESC sequence mapping (Home/End, PgUp/PgDn, F-keys).
- Move POSIX input to full raw-mode initialization for immediate non-canonical key delivery.
- Support modifier-aware parsing where terminal protocol provides it.
- Add automated tests for ESC disambiguation and sequence fallbacks.
- Normalize POSIX/Windows key semantics across shared expectations.
