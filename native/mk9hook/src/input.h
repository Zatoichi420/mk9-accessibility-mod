// Watches the game's input APIs, and in developer mode lets a script press
// keys and controller buttons inside the game (for automated menu tests).
//
// Developer mode is on only while mk9hook_dev.txt exists next to MKKE.exe.
// Commands are written to mk9hook_cmd.txt, one per line or comma-separated:
//   k:down  k:enter  k:esc  k:space   keyboard key (DirectInput)
//   p:a  p:b  p:start  p:down  p:up   controller 1 button (XInput)
//   w:500                             wait 500 ms
#pragma once

namespace input {

// Hooks XInputGetState and starts the command watcher.
void Install();

// Called by the dinput8 proxy with each IDirectInput8 the game creates.
void OnDirectInputCreated(void* directInput);

// True while the given virtual key is held on the real keyboard.
bool KeyDown(int vk);

}  // namespace input
