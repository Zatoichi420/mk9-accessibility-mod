// Shared helpers for the MK9 hook's source files.
#pragma once
#include <windows.h>
#include <string>

// Appends a timestamped line to mk9hook_log.txt (printf-style).
void LogLine(const char* fmt, ...);

// Folder holding MKKE.exe and this DLL, without a trailing backslash.
const std::wstring& GameDir();
