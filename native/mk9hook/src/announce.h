// Decides what to speak from the menu text and highlight calls the hooks see.
#pragma once

namespace announce {

// Reads mk9hook.ini and starts speech.
void Start();

// A text value was written to the menu at path (UTF-8, path as the game gave it).
void OnText(const char* path, const char* text);

// The game called method path (for example "_root.MainNav.Nav.gotoAndPlay")
// with firstArg as its first argument ("" if none; numbers as digits).
void OnCall(const char* path, const char* firstArg);

// F9 and F10.
void Repeat();
void Toggle();

}  // namespace announce
