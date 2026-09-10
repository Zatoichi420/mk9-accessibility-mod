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

## Phase 0 findings

*Pending — static-analysis investigation of the actual install in
progress.*

## Resume point

Repo just created. Next: fold in the Phase 0 findings once the file
investigation completes, then decide the concrete next technical step.
