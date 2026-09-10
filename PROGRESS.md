# MK9 (Komplete Edition) Accessibility — plan and progress log

## Goal

NVDA screen-reader narration for Mortal Kombat Komplete Edition's pre-match
UI: main menu, mode select, character select, and options. Not gameplay,
not online — mirrors the scope precedent set by the MKX and Deception
sister projects.

## Install facts

- Steam appid `237110`, install dir name `MortalKombat_KompleteEdition`, at
  `F:\SteamLibrary\steamapps\common\MortalKombat_KompleteEdition`.
- This is the PC "Komplete Edition" release of **Mortal Kombat (2011)**,
  commonly called **MK9** — NetherRealm's reboot title, released 2013 on PC
  (2011 on consoles), four years before MKX. Do not assume it shares MKX's
  exact engine/tooling without checking — verify independently, the same
  discipline that made the MKX investigation productive.

## What's being reused from the MKX project

This project follows the MKX accessibility project's playbook directly —
see [github.com/Zatoichi420/mkx-accessibility-mod](https://github.com/Zatoichi420/mkx-accessibility-mod)
for the full precedent. In short:
- **Phase 0 static-analysis checklist**: PDB presence (the single biggest
  factor in MKX's tractability), anti-cheat files, engine/UI-framework
  identification, community Cheat Engine table precedent.
- **OCR/NVDA narration architecture**: full-desktop capture (not
  `PrintWindow` — that silently fails once a game goes fullscreen, a real
  bug hit and fixed on MKX), dHash screen-matching against a hand-verified
  `known_screens/` library, NVDA Controller Client speech, F9/F10/F2
  hotkeys.
- **One-click setup**: a Startup-folder-shortcut installer (not Task
  Scheduler — that hit a silent "Access is denied" wall on this machine).
- **Known gotchas to watch for again**: `GetAsyncKeyState`/`SendInput`
  don't work for a process Claude launches via its own tool calls — hotkeys
  and any input-driven testing need the user to start the reader
  themselves, not Claude.

## Phase 0 findings (2026-09-10, static analysis)

- **Real executable**: `DiscContentPC\MKKE.exe` (~11MB, **32-bit/x86** — a
  real difference from MKX: any future memory-reading work needs 4-byte
  pointers, not 8-byte). `DiscContentPC\MKLauncher.exe` is a separate
  launcher/config stub; its exact function couldn't be determined from
  static strings alone. Install structure is **flat** (`DiscContentPC\`
  holds everything directly) rather than MKX's `Binaries\Retail\` nesting.
- **No PDB anywhere in the install** — searched recursively, none found.
  This is the single biggest negative finding relative to MKX, where a
  shipped `MK10.pdb` made static symbol-mining via `dbghelp` possible at
  all. **No such shortcut exists here.** A future Phase 2 (live memory
  reads) would need to find addresses the hard way — pattern scanning,
  manual reverse engineering, or cross-referencing community Cheat Engine
  tables — much closer to the Deadly Alliance/Deception GameCube projects'
  situation than MKX's.
- **No anti-cheat** (no EasyAntiCheat/BattlEye files found) — same
  low-risk situation as MKX for external memory reading.
- **Same NetherRealm UE3-derived engine lineage as MKX, strongly
  corroborated**: `DiscContentPC\Config\Coalesced.ini` (same filename
  convention), per-language `Coalesced.{eng,fra,int,por,spa}` variants,
  `DiscContentPC\Asset\` with 1,229 `.xxx`-renamed cooked assets using an
  `AVA_<CharacterName>` naming pattern (same renamed-cooked-package
  obfuscation convention MKX's `Asset\` folder uses). `PhysXLoader.dll`/
  `PhysXCooking.dll`/`PhysXCore.dll` (PhysX 3.x-era) and `binkw32.dll`
  (32-bit Bink Video) are also consistent with a UE3-era title. Direct
  `strings` scans of `MKKE.exe` for "Unreal"/"Scaleform" came back empty —
  same inconclusive (not negative) result MKX had, likely packed/compressed
  PE sections defeating a naive grep either way.
- **Extensive, mature, multi-year community precedent**: public Cheat
  Engine tables and commercial trainers specifically targeting `MKKE.exe`
  exist from FearLess Revolution (most recently updated August 2025),
  Cheat Happens, MrAntiFun, an "FTS" trainer, GameCopyWorld, and ggmania.
  This independently confirms `MKKE.exe` as the correct, stable process
  name and that external memory reading against it is thoroughly
  well-trodden — arguably even more so than MKX had, given the longer
  track record.
- **Not checked** (deliberately, per plan — live process/window inspection
  needs the game actually launched, held for a later phase with explicit
  go-ahead): window class, live behavior, whether Scaleform/GFx is
  definitively the UI middleware.

## Resume point

Repo just created. Next: fold in the Phase 0 findings once the file
investigation completes, then decide the concrete next technical step.
