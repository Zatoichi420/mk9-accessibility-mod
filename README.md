# Mortal Kombat Komplete Edition (MK9) — talking menus (screen-reader accessibility)

A planned tool to make **Mortal Kombat Komplete Edition** (the PC release of
Mortal Kombat 9, Steam appid `237110`) usable without sight — starting with
the main menu, mode select, character select, and options, the same scope
the sister projects below started with.

**Status: planning / Phase 0 research.** No reader has been written yet — see
[PROGRESS.md](PROGRESS.md) for the phased plan and current findings.

## Sister projects (same author, same overall approach)

- [mkx-accessibility-mod](https://github.com/Zatoichi420/mkx-accessibility-mod) —
  the direct predecessor to this project: OCR + NVDA menu narration for
  Mortal Kombat X, confirmed working live. This project follows the same
  playbook.
- [mk-legacy-kollection-accessibility-mod](https://github.com/Zatoichi420/mk-legacy-kollection-accessibility-mod) —
  OCR + perceptual-hash screen-matching narrator for the Legacy Kollection
  launcher, speaks via NVDA.
- [mortal-kombat-deadly-alliance-accessibility](https://github.com/Zatoichi420/mortal-kombat-deadly-alliance-accessibility) —
  reads the GameCube game's RAM live over RetroArch's network command
  interface, matches the active screen against a reverse-engineered symbol
  map, speaks via OS TTS.
- [MK-deception-accessibility-mod](https://github.com/Zatoichi420/MK-deception-accessibility-mod) —
  same technique as Deadly Alliance, sister GameCube title.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for what would help most right now.
Be kind: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Legal

No game code or assets included or ever will be. Read-only: this reads values
from the game's own memory/screen at runtime and speaks them, the same
no-modification stance as the sister projects. You need your own legally owned
copy of Mortal Kombat Komplete Edition. Not affiliated with or endorsed by
Warner Bros. or NetherRealm Studios. See [LICENSE](LICENSE) (MIT).
