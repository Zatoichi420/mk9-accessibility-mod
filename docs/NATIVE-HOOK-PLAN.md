# MK9 native screen-reader hook: findings and plan

Written 2026-10-20 after taking apart Mortal Kombat Komplete Edition
(Steam, `DiscContentPC\MKKE.exe`). The research files are in `F:\MK9-RE`.
They are not in this repo, because they are the game's own content.

## What the game is made of

- `MKKE.exe` is a 32-bit Unreal Engine 3 program, 11 MB, built 2014-08-01
  (PE timestamp 1406923673). It isn't packed or encrypted. Its symbol file
  (`MK9PCGame-Retail.pdb`) wasn't shipped, so function names must be found
  from the error strings they use.
- **Every menu is a Scaleform GFx 3.x Flash movie** (ActionScript 2). There
  are 143 of them, one per screen, inside the `Asset\ui_*.xxx` packages.
  Some packages are LZO-compressed. `tools/re/unpack.py` and
  `tools/re/extract_gfx.py` get them out, and JPEXS FFDec (`F:\Tools\ffdec`)
  decompiles them.
- **The movies hold almost no logic.** They contain text fields named
  `varText`, `varLabel`, `var_valueTxt` and so on, and animations with frame
  labels such as `HighlightOn`, `HighlightOff`, `Focus` and `Selected`.
- **The menus are driven by NetherRealm's MKScript** (`MainMenu.mko`,
  `PlayerSelect.mko`, `pause_menu.mko` and others, inside
  `Asset\Startup.xxx`). The scripts fill the movies through Scaleform paths,
  for example:
  - `audio.slider%d.optionLabel.varLabel`
  - `SocialMessaging.option%d.optionText.var_valueTxt`
  - `MainNav.descAnim.desc.text`
  - `screen.nameAnim.name.varText`
  - `ps.AltCostumeSide%d.optionLabel.varLabel`
  - `arenaAnim.arena.nameGroup.arena_name`

  They move the highlight with calls like `option%d.gotoAndPlay("HighlightOn")`.
- **All menu text is in `Localization\Coalesced.eng`**: zlib-compressed,
  then every byte inverted. Decoded copy: `F:\MK9-RE\Coalesced_eng.txt`. It
  covers MainMenu, PlayerSelect (character and costume names), Pause,
  Options, Movelist, MsgBox, Krypt and the rest, so text the game shows can
  be checked against it.

## Where to hook

Everything the scripts put on screen passes through two Scaleform functions.
Hooking those catches every menu without knowing MKScript's bytecode.

| What | Address (MKKE.exe, base 0x400000) | Found by |
|---|---|---|
| GFxMovieRoot vtable | 0xD92BA8 | holds the SetVariable wrapper |
| `SetVariable` public wrapper (vtable slot 16) | 0xAC7810 | "NULL pathToVar passed to SetVariable" |
| `SetVariable` internal | 0xA90F10 | "SetVariable failed: can't resolve the path" |
| `Invoke` (GFxValue args) | 0xA91150 | "Invoked method '%s' is not a function" |
| `Invoke` (format string args) | 0xA913A0 | same string, second user |
| AS `gotoAndPlay` native | 0xAA3D40 | "GFx_SpriteGotoAndPlay needs one arg" |
| Game helper: `%s.row%d.col%d.varText` | 0x4D7530 | format string |
| Game helper: `%s.summary%d.FocusGain` | 0x4D73D0 | format string |
| Game helper: `UpdateHighlight` | 0x4D7BC0 | string |

Hooks should find these with byte-pattern scans, not fixed addresses, and
switch themselves off safely if a pattern isn't found.

## How the mod would work

1. **Loader.** Put a proxy `xinput1_3.dll` (or `dinput8.dll`) in
   `DiscContentPC`. The game imports both, and Windows loads them from the
   game folder first. Nothing in the game's own files changes, and deleting
   the DLL uninstalls it. It's built as 32-bit C++ with MSVC (2022 Build
   Tools and VS 18 are installed) and MinHook.
2. **Hooks** on SetVariable and both Invokes record, for each movie:
   - path → text (labels, values and descriptions);
   - path → highlight state (`gotoAndPlay` with `HighlightOn`, `Focus` or
     `Selected`).
3. **Screen model.** When a movie opens (`ui_s_mainmenu`, `ui_i_pause_menu`,
   `ui_ps_ladder` and so on), say its title. When the highlight moves to item
   N, say item N's label, its value if it has one (sliders, on/off), then the
   description (`MainNav.descAnim.desc.text`) after a short pause.
4. **Speech** through Tolk, 32-bit, which supports NVDA, JAWS and SAPI. The
   existing reader's behaviour stays the same: F9 repeats, F10 toggles.
5. **Text clean-up.** Replace the button tokens used in `Coalesced`
   (`[FP]`, `[BK]`, `[Up]` and so on) with words, such as "front punch".
   This makes the move list readable.
6. **Coexisting with the OCR reader.** The DLL creates a named mutex. The
   OCR reader checks for it and stays quiet for MK9 while the hook is
   active, but can still be used for anything the hook misses.

## Phases

0. **Done:** unpack, decompile, string tables, hook addresses (this file).
1. **Logging spike.** The proxy DLL only logs (movie, path, value) and
   highlight calls to a file. You walk the main menu, Options, character
   select and the pause menu for about 10 minutes, and the log shows exactly
   which paths matter. No speech yet.
2. **Speech for the main menu, sub-menus, Options and the pause menu.**
3. **Character select:** character name under each player's cursor, costume
   choice, random select, Player 1 or 2, team select, arena select and
   ladder difficulty.
4. **Message boxes, move list, Challenge Tower, Krypt, results screens.**
5. **Installer:** the existing Setup.exe finds the Steam game folder, copies
   the DLL in, and removes it on uninstall.

## Risks

- **Text encoding.** GFx 3 values may be UTF-8 or wide strings, so both
  must be handled.
- **Fights.** Health and round info aren't menus, so they're out of scope
  for now.
- **Online play.** MK9 PC has no anti-cheat, but the DLL should do nothing
  except read and speak. It must not change what the game sends.
- **Game updates.** The Steam build hasn't changed since 2014, but the
  pattern scans protect against a different exe anyway.

## Phase 1 status (2026-10-20)

Built (`native/mk9hook`, `build.bat`) and installed as
`DiscContentPC\dinput8.dll`. Checked on the real game at the legal and
title screens: all hooks install, the game runs normally, and text and
calls are logged to `DiscContentPC\mk9hook_log.txt`. F11 writes a MARK line.

Corrections to the table above, from that run:
- 0xAA1EA0 is Invoke(method, result, argFmt, va_list), printf style, not
  an array of GFxValues. Example: `_root.gotoAndPlay("legal")`, format `%ls`.
- 0xABF7C0 is the same call on a path the game has already looked up:
  (path, character, result, argFmt, va_list).
- SetVariable strings arrive as wide strings (GFxValue type 5).
- The 0xAA3800 form isn't decoded yet; only its method name is logged.

To uninstall, delete `dinput8.dll` from `DiscContentPC`.

## Phase 2 status (2026-10-20 evening): speaking

The DLL now speaks (`announce.cpp`, `speech.cpp`) through NVDA, then JAWS,
then the Windows voice. Install it by copying `out\dinput8.dll` and
`out\nvdaControllerClient.dll` into `DiscContentPC`. F9 repeats the last
item, F10 turns speech off or on, F11 marks the log.

More hooks were needed than the table above lists:
- `GetVariable` (0xAB7B90) and `GFxValue::ObjectInterface::SetText`
  (0xA646A0). Menus fetch a text box once, then set its text directly; the
  main menu's item names arrive this way. GetVariable gives each object
  its path.
- `ObjectInterface::Invoke` (0xA64510). The pause menu calls
  `mc_option_N.FocusGain()` on objects it holds.

How each screen is read, all checked against the real game with scripted
controller input:

| Screen | Highlight signal | What is said |
|---|---|---|
| Main menu and its sub-menus (Fight, Training, Options, Extras) | `navTextN.gotoAndPlay("DownHighlight"/"UpHighlight"/"StaticHighlight")` | item name, then its description |
| Players switch under the main menu | description "Change the Number of Players." | "1 PLAYER, number of players" |
| Audio, Gameplay, Player Options settings | `sliderN`/`optionN.gotoAndPlay("HighlightOn")` | label, value; slider position as a number |
| Message boxes | `msgBoxAnim.mainText.message` set | message, then "Yes, A button. No, B button." |
| Character select | `vs_names.name_p1.name.text` | "Choose your fighter", then each name |
| Difficulty pop-up | `optionN.gotoAndPlay("BuildOn")` | "Difficulty", then each level |
| Pause menu | `mc_option_N.FocusGain()` | "PAUSE", then each option |
| Move list | `_root.UpdateHighlight(2 × row)` | "Uppercut, down plus back punch" |
| Challenge Tower | `challengeInfo.gotoAndPlay("focusOn")` | "Challenge 2, Training: Defense, reward 100 koins" |
| Krypt | `locationLabel`, `purchaseInfo.label` | area name; item ahead and its cost |
| Video, Keyboard settings | `optionN`/`btnN.gotoAndPlay("HighlightOn")` | "FRONT PUNCH, T, numpad 7" |
| Practice pause menu, Practice/AI Options | `mc_option_N.FocusGain()`, `optionN` HighlightOn | "PRACTICE OPTIONS", "DAMAGE INFO, ON" |
| Continue screen | `mc_option_N.FocusGain()` | "CONTINUE?", "RETRY" |
| Player 2 pick, costumes, tag partner | `name_p2`, `AltCostumeSideN.nameGroup.text` | "Player 2, choose your fighter"; "LIU KANG and SCORPION" |
| Tutorial and challenges | `practiceHUD.messageBox`, `moveNameAnim.move_name`, `*TextAnim.Body.Text`, `TaskSuccess` | lesson cards, the move to do, "Done!", dialogue "Jax: …" |

Screens reuse the same names (the pause menu and the continue screen both
have `mc_option_1`), so each stored text remembers its movie and an item is
read only from the movie that wrote to it last.

## Arcade Kollection (MKHDGame.exe), tried 2026-10-20

The same Scaleform GFx 3 code, so the DLL has a second address table for it
(`g_arcadeTargets`) and all hooks attach without Games for Windows Live
objecting. But its menus are built from Scaleform CLIK components, which
move the highlight inside ActionScript without calling the game, and almost
nothing reached the hooks. The scripted test input didn't move its menus
either. The DLL was removed from its folder again; reading its menus needs
another hook point (CLIK focus or the text renderer) and a session with
someone at the keyboard.

Highlights are spoken ~60 ms after the game makes them, so the frame can
finish filling in names and titles. A run of highlights closer together
than anyone presses (Challenge Tower scrolling itself) waits 350 ms and
speaks only where it stops.

Developer testing: with `mk9hook_dev.txt` in `DiscContentPC`, commands in
`mk9hook_cmd.txt` (`p:down`, `k:enter`, `w:500`; see `input.h`) press
buttons inside the game. `mk9hook.ini` with `[speech] mute=1` logs `SAY`
lines without speaking.

Not covered yet: the Kredits scroll, Krypt movement between graves (the
D-pad didn't move it in testing), Challenge Tower Up/Down (same), online
screens, Test Your Luck/Might, and fights themselves.

## Backlog for a future session (noted 2026-10-21)

MK9 native hook, not done yet:
- Kredits scroll: say "Kredits, A to exit" at least.
- Krypt: moving between graves and buying (the scripted D-pad didn't move;
  needs a real controller or the stick), and unlock messages after a buy.
- Challenge Tower: Up/Down and the bumpers' fast scroll (same: scripted
  D-pad didn't register), and the Details pop-up.
- Test Your Luck (reels), Test Your Might, Sight and Strike screens.
- Tutorial: the button command for each move (drawn as icons). Could be
  looked up from the move list text.
- Story Mode chapter screens and its pause menu (expected to work like the
  fight pause menu, untested).
- Nekropolis (bios, kontent viewer), Kombat Kodes on the VS screen, ladder
  end stats, online and lobby screens.
- Two-player versus: both players moving on character select at once.
- Installer: have Setup.exe copy `dinput8.dll` and the NVDA client into
  the game folder and remove them on uninstall; make the OCR reader stay
  quiet for MK9 when the hook is loaded (named mutex).
- The user's live test with speech on (only the muted log and a separate
  NVDA speech test have been verified).

Arcade Kollection (MKHDGame.exe): hooks attach, but its CLIK menus move the
highlight inside ActionScript. Needs a hook on CLIK focus changes or text
rendering, and someone at the keyboard (injected input didn't work).
