# Contributing

Thanks for helping make Mortal Kombat Komplete Edition playable without sight.
Contributions of every size are welcome — a typo fix, a hand-verified
`known_screens/` entry once the reader exists, a tested memory offset, or
just a report that it did or didn't work on your setup.

This is a small project maintained in spare time. Please be patient with
review, and be kind in issues and PRs — see [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Things that would genuinely help

| Area | What's needed |
|---|---|
| **Porting the OCR/NVDA reader from `mkx-accessibility-mod`** | The whole `ocr_reader/` architecture (full-desktop capture, dHash screen-matching, NVDA Controller Client speech, hotkeys) is designed to be game-agnostic aside from the process name — this hasn't been done yet for MK9. See [PROGRESS.md](PROGRESS.md) for the plan. |
| **`known_screens/` library entries** | Once the reader exists, it'll need the same hand-verified reference-screen library the other projects use. |
| **Memory offsets** | No PDB ships with this game (confirmed — see PROGRESS.md), unlike MKX. Community Cheat Engine tables for `MKKE.exe` already exist and are a real lead for known-good addresses (health, timer, etc.) — cross-referencing those, or pattern-scanning fresh ones for menu/cursor state, would unlock a faster Phase 2 the same way a shipped PDB did for MKX. |
| **Confirming the engine/UI framework live** | Static analysis strongly suggests the same NetherRealm UE3-derived lineage as MKX (Coalesced.ini, renamed `.xxx` cooked assets) but this hasn't been confirmed against the running game yet - no live testing has been done. |
| **32-bit-specific tooling** | `MKKE.exe` is 32-bit (MKX's `MK10.exe` is 64-bit) - any memory-reading tooling ported from the MKX project needs pointer-size and calling-convention adjustments, not a straight copy-paste. |

### Out of scope for this repo

Mirrors the precedent set by the sister projects:

- **Online play / matchmaking** — not attempted, not planned.
- **Live round-by-round fight narration** — this project narrates menus, not gameplay.

## Development setup

You need Mortal Kombat Komplete Edition on Steam (appid `237110`) and NVDA.
No reader exists yet to install — see [PROGRESS.md](PROGRESS.md) for current
status.

**Important**: whenever there is a reader to test, run it by double-clicking,
or from a terminal *you* opened yourself — not from a script or tool that
launches it programmatically. Hotkeys (and, per the MKX project's own
findings, most interactive testing in general) rely on `GetAsyncKeyState`,
which does not see input for a process started by outside automation. If
you're testing and input doesn't seem to work, this is almost certainly why.

## Style

Match the conventions established in
[mkx-accessibility-mod](https://github.com/Zatoichi420/mkx-accessibility-mod) —
plain Python, comment *why* not what, cite how any memory offset or symbol
was verified so it's traceable later.

By contributing you agree your work is licensed under the project's
[MIT License](LICENSE).

## A note on scope and the game

This project ships **no game code or assets** and never will. It reads
pixels from the screen (and, eventually, values from the game's own process
memory) at runtime and speaks them — the same no-modification stance as the
sister projects. You need your own legally owned copy of Mortal Kombat
Komplete Edition. Please don't attach installers or extracted game files to
issues or PRs — they'll be removed. See the legal note in the
[README](README.md#legal).
