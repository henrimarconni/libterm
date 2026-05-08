# Lesson: posix special-key parsing
Mode: concept
Refs: termbox2 (external/termbox2/termbox2.h:2612, external/termbox2/termbox2.h:3449, external/termbox2/termbox2.h:3513)
Started: 2026-04-27

## Curriculum
- [x] 1. Clarify libterm event model (`type` vs `key` vs `ch`) with current POSIX limitations
- [x] 2. Add minimal ESC-sequence state handling shape in POSIX input path
- [x] 3. Parse arrow key escape sequences (`ESC [ A/B/C/D`) into `LT_KEY_ARROW_*`
- [x] 4. Keep printable-byte path intact (`ev->ch`) when sequence is not special-key input
- [x] 5. Handle bare `ESC` consistently (timeout or direct mapping policy)
- [x] 6. Add focused tests for POSIX key mapping behavior
- [x] 7. Validate via example program and inspect returned `type/key/ch`

## Notes
- Keep platform-specific behavior inside `src/platform/posix/`.
- Preserve public API semantics: `type` is category, `key/ch` carry identity.
- Prefer termbox2-compatible behavior where practical.
