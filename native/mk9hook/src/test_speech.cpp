// Manual check for speech.cpp: test_speech.exe "text to say"
#include <windows.h>
#include <stdio.h>
#include "speech.h"

int wmain(int argc, wchar_t** argv) {
    wchar_t dir[MAX_PATH];
    GetModuleFileNameW(nullptr, dir, MAX_PATH);
    *wcsrchr(dir, L'\\') = 0;
    speech::Start(dir);
    if (argc > 1) {
        char text[1000];
        WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, text, sizeof(text), nullptr, nullptr);
        speech::Say(text, true);
        Sleep(3000);
    } else {
        Sleep(500);
    }
    printf("backend: %s\n", speech::Backend());
    return 0;
}
