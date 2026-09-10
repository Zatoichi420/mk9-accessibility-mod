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

## Phased plan

### Phase 0 — Research & feasibility (mostly done via static analysis above)

Remaining: launch the game and confirm `MKKE.exe` is genuinely the live
process (not `MKLauncher.exe`), check its window class for engine
confirmation, and note whether it reaches an interactive menu on its own or
needs the same kind of input MKX's title screen turned out to need. Hold
for explicit go-ahead before launching, matching how MKX's Phase 0 handled
this the same way.

### Phase 1 — Guaranteed-feasible baseline: OCR + reference library

**Done (2026-09-10), scaffolded and syntax-checked, not yet live-tested.**
Ported `mkx-accessibility-mod`'s `ocr_reader/` (`main.py`,
`screen_library.py`, `nvda_controller_client/`) near-verbatim, retargeted
at `MKKE.exe`. Also brought over `install.bat`/`uninstall.bat` (the
Startup-folder-shortcut one-click setup, not Task Scheduler) and
`requirements.txt`. Verified `py_compile` clean; all dependencies
(pywin32/pillow/numpy/winsdk) already installed from the sister projects.

Game-specific differences from the MKX original:
- `PROCESS_NAME = "MKKE.exe"` — per Phase 0 static analysis + community
  Cheat Engine precedent, **not yet confirmed via an actual live launch**
  on this machine, unlike MKX's equivalent line (which was launch-tested
  before being written that confidently).
- `known_screens/` starts empty — nothing captured/verified yet.
- Highlight-color thresholds (`BRIGHTNESS_THRESHOLD`/
  `BLUE_MINUS_RED_THRESHOLD`) are the **same placeholder values that have
  now passed unchanged through three different games' codebases**
  (Legacy Kollection → MKX → here) — flagged `NEEDS_CALIBRATION` in the
  code. MK9's actual UI colors have never been sampled.
- No Phase 2 memory-reading shortcut exists yet (no shipped PDB, unlike
  MKX) — this OCR baseline is the plan, not a stopgap for something
  faster.

### Phase 2 — Live memory reads (harder here than MKX — no PDB)

Without a shipped PDB, this phase needs one of:
- Cross-referencing the existing community Cheat Engine tables for `MKKE.exe`
  (confirmed to exist, multiple sources) for known-good addresses, then
  pattern-scanning outward from those for menu/cursor state specifically
  (the public tables focus on gameplay values like health/timer, not menu
  navigation — menu offsets would still need original work).
- Manual reverse engineering via a disassembler (Ghidra), the same
  fallback Legacy Kollection's `proxy_dll/` attempt represents elsewhere in
  this project family — accepting it's slower without symbol names to
  anchor on.
- Remember `MKKE.exe` is **32-bit** — pointer size, calling convention, and
  any ported tooling (e.g. MKX's `tools/dump_pdb_symbols.py`-style scripts)
  need real adjustment, not a blind copy.

This phase is explicitly lower-confidence than MKX's equivalent going in —
say so plainly if asked for a timeline, rather than implying PDB-free
reverse engineering is as fast as PDB-assisted was.

### Phase 3 — Calibration and hardening

Build out `known_screens/` for main menu, mode select, character select,
options, with the same per-screen review-before-going-live discipline as
the sister projects.

## Out of scope for v1

Online play/matchmaking, live round-by-round fight narration. Mirrors the
precedent set across the whole project family.

## Resume point

Phase 0 (static analysis) and Phase 1 (OCR/NVDA reader baseline) are both
done. **Nothing has been launched or tested live yet** — `PROCESS_NAME`
is an educated, well-corroborated guess (`MKKE.exe`), not a confirmed
fact, and the highlight-color thresholds are known-wrong placeholders.
Next concrete step: launch the game (needs explicit go-ahead, matching
MKX's own Phase 0 discipline) to confirm the live process name, see
whether it reaches an interactive menu on its own or needs the kind of
input MKX's title screen turned out to need, and get real screenshots to
seed `known_screens/` and calibrate highlight detection. Remember: any
reader instance needs to be started by the user directly, not launched by
Claude, for its hotkeys to work (see [[environment_hotkey_limitation]]).
