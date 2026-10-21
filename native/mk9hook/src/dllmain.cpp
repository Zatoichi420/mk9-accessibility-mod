// MK9 accessibility hook.
//
// Loaded by the game as dinput8.dll (a proxy that forwards to the real one).
// It watches what the game's menu scripts write into the Scaleform menus and
// which item they highlight, and speaks it (announce.cpp). Nothing in the
// game is changed: every hook calls straight through.
//
// Log: mk9hook_log.txt next to MKKE.exe. Keys while playing: F9 repeats,
// F10 turns speech off or on, F11 writes a MARK line and beeps.
// See docs/NATIVE-HOOK-PLAN.md for how the addresses were found.

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include "MinHook.h"
#include "common.h"
#include "announce.h"
#include "input.h"

static bool g_isGame;  // loaded into MKKE.exe (not MKLauncher.exe)
static std::wstring g_gameDir;

const std::wstring& GameDir() { return g_gameDir; }

// ---------------------------------------------------------------- dinput8 proxy

typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, void*);
static DirectInput8Create_t g_realDirectInput8Create = nullptr;

static void LoadRealDinput8() {
    if (g_realDirectInput8Create) return;
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\dinput8.dll");
    HMODULE real = LoadLibraryW(path);
    if (real) g_realDirectInput8Create = (DirectInput8Create_t)GetProcAddress(real, "DirectInput8Create");
}

extern "C" HRESULT WINAPI Proxy_DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID* out, void* outer) {
    LoadRealDinput8();
    if (!g_realDirectInput8Create) return E_FAIL;
    HRESULT hr = g_realDirectInput8Create(inst, ver, riid, out, outer);
    if (SUCCEEDED(hr) && g_isGame && out) input::OnDirectInputCreated(*out);
    return hr;
}

// ---------------------------------------------------------------- log

static CRITICAL_SECTION g_logLock;
static FILE* g_log = nullptr;
static DWORD g_startTick = 0;

// Identical consecutive lines (loading-bar updates and the like) are
// collapsed into one "(repeated N times)" note.
static char g_lastLine[2600];
static unsigned g_repeats = 0;

void LogLine(const char* fmt, ...) {
    if (!g_log) return;
    char line[2600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_logLock);
    if (strcmp(line, g_lastLine) == 0) {
        g_repeats++;
    } else {
        DWORD t = GetTickCount() - g_startTick;
        if (g_repeats) fprintf(g_log, "           (repeated %u more times)\n", g_repeats);
        g_repeats = 0;
        fprintf(g_log, "%6lu.%03lu %s\n", t / 1000, t % 1000, line);
        fflush(g_log);
        strcpy_s(g_lastLine, line);
    }
    LeaveCriticalSection(&g_logLock);
}

// The helpers below read game memory inside __try blocks. MSVC forbids
// __try in functions that own C++ objects, so they work on char buffers.

// Copies a C string from game memory into out, escaping control characters.
static void CopyStr(const char* s, char* out, size_t cap) {
    size_t n = 0;
    if (!s) { strcpy_s(out, cap, "(null)"); return; }
    __try {
        for (size_t i = 0; s[i] && n + 3 < cap; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
            else out[n++] = (c < 32) ? '?' : (char)c;
        }
        out[n] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out[n] = 0;
        strcat_s(out, cap, "(bad ptr)");
    }
}

// Copies a wide string from game memory into out as UTF-8.
static void CopyWStr(const wchar_t* s, char* out, size_t cap) {
    wchar_t tmp[700];
    size_t n = 0;
    bool bad = false;
    if (!s) { strcpy_s(out, cap, "(null)"); return; }
    __try {
        for (; s[n] && n + 1 < _countof(tmp); n++) tmp[n] = (s[n] == L'\n') ? L' ' : s[n];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        bad = true;
    }
    tmp[n] = 0;
    if (WideCharToMultiByte(CP_UTF8, 0, tmp, -1, out, (int)cap, nullptr, nullptr) <= 0) out[0] = 0;
    if (bad) strcat_s(out, cap, "(bad ptr)");
}

// ---------------------------------------------------------------- GFxValue (Scaleform GFx 3.x, 32-bit)

struct GFxValue {
    void* pObjectInterface;
    uint32_t Type;  // low 4 bits: kind; 0x40 = managed (string held by reference)
    union {
        double NValue;
        bool BValue;
        const char* pString;
        const char** pStringManaged;
        const wchar_t* pStringW;
        const wchar_t** pStringWManaged;
    };
};
static_assert(sizeof(GFxValue) == 16, "GFxValue layout");

static void DescribeValue(const GFxValue* v, char* out, size_t cap) {
    char text[2200];
    if (!v) { strcpy_s(out, cap, "(no value)"); return; }
    __try {
        uint32_t raw = v->Type;
        bool managed = (raw & 0x40) != 0;
        switch (raw & 0x0F) {
        case 0: strcpy_s(out, cap, "undefined"); break;
        case 1: strcpy_s(out, cap, "null"); break;
        case 2: strcpy_s(out, cap, v->BValue ? "true" : "false"); break;
        case 3: sprintf_s(out, cap, "%g", v->NValue); break;
        case 4: CopyStr(managed ? *v->pStringManaged : v->pString, text, sizeof(text)); sprintf_s(out, cap, "\"%s\"", text); break;
        case 5: CopyWStr(managed ? *v->pStringWManaged : v->pStringW, text, sizeof(text)); sprintf_s(out, cap, "L\"%s\"", text); break;
        default: sprintf_s(out, cap, "(type 0x%X)", raw); break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(out, cap, "(bad value)");
    }
}

// Copies a string GFxValue as UTF-8. Returns false for other value types.
static bool ValueText(const GFxValue* v, char* out, size_t cap) {
    out[0] = 0;
    if (!v) return false;
    __try {
        uint32_t raw = v->Type;
        bool managed = (raw & 0x40) != 0;
        if ((raw & 0x0F) == 4) { CopyStr(managed ? *v->pStringManaged : v->pString, out, cap); return true; }
        if ((raw & 0x0F) == 5) { CopyWStr(managed ? *v->pStringWManaged : v->pStringW, out, cap); return true; }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return false;
}

// ---------------------------------------------------------------- hooks
//
// MSVC can't declare __thiscall free functions, so each hook is __fastcall
// with an unused EDX argument; ECX carries `this` either way.

// GFxMovieRoot::SetVariable(const char* path, const GFxValue& value, SetVarType)
typedef bool(__fastcall* SetVariable_t)(void* self, void* edx, const char* path, const GFxValue* value, int setType);
static SetVariable_t o_SetVariable;

// Only log a path when its value changes, so per-frame updates don't flood the log.
static std::unordered_map<std::string, std::string> g_lastValue;

static bool __fastcall h_SetVariable(void* self, void* edx, const char* path, const GFxValue* value, int setType) {
    char pbuf[300], vbuf[2300];
    CopyStr(path, pbuf, sizeof(pbuf));
    DescribeValue(value, vbuf, sizeof(vbuf));
    std::string p = pbuf, v = vbuf;
    bool changed;
    EnterCriticalSection(&g_logLock);
    std::string key = std::to_string((uintptr_t)self) + "|" + p;
    auto it = g_lastValue.find(key);
    changed = (it == g_lastValue.end() || it->second != v);
    if (changed) g_lastValue[key] = v;
    LeaveCriticalSection(&g_logLock);
    if (changed) LogLine("SET  movie=%p %s = %s", self, p.c_str(), v.c_str());
    if (ValueText(value, vbuf, sizeof(vbuf))) announce::OnText(pbuf, vbuf, self);
    return o_SetVariable(self, edx, path, value, setType);
}

// Decodes printf-style arguments for a GFx Invoke format string such as "%s"
// or "%d %ls". args points at the caller's va_list. All reads are guarded.
// first receives the first argument alone, unquoted.
static void FormatArgs(const char* fmt, const char* args, char* out, size_t cap, char* first, size_t firstCap) {
    char one[2200];
    out[0] = 0;
    first[0] = 0;
    __try {
        for (const char* f = fmt; f && *f; f++) {
            if (*f != '%') continue;
            f++;
            bool wide = false;
            while (*f == 'l' || *f == 'h') { wide |= (*f == 'l'); f++; }
            if (!*f) break;
            if (*f == '%') continue;
            bool isFirst = out[0] == 0;
            if (!isFirst) strcat_s(out, cap, ", ");
            switch (*f) {
            case 's':
                if (wide) CopyWStr(*(const wchar_t**)args, one, sizeof(one));
                else CopyStr(*(const char**)args, one, sizeof(one));
                strcat_s(out, cap, "\"");
                strcat_s(out, cap, one);
                strcat_s(out, cap, "\"");
                args += 4;
                break;
            case 'f': case 'g': case 'e':
                sprintf_s(one, "%g", *(const double*)args);
                strcat_s(out, cap, one);
                args += 8;
                break;
            default:  // d, i, u, x, b, c and anything else 4 bytes wide
                sprintf_s(one, "%d", *(const int*)args);
                strcat_s(out, cap, one);
                args += 4;
                break;
            }
            if (isFirst) strncpy_s(first, firstCap, one, _TRUNCATE);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcat_s(out, cap, "(bad args)");
    }
}

// Sprite Invoke(const char* method, GFxValue* result, const char* argFmt, va_list args)
// Used when the game's helper couldn't resolve the path first; method then
// holds the full path, for example "_root.gotoAndPlay".
typedef bool(__fastcall* InvokeVa_t)(void* self, void* edx, const char* method, GFxValue* result, const char* fmt, const char* args);
static InvokeVa_t o_InvokeVa;

static bool __fastcall h_InvokeVa(void* self, void* edx, const char* method, GFxValue* result, const char* fmt, const char* args) {
    char mbuf[300], fbuf[60], abuf[2400], first[200];
    CopyStr(method, mbuf, sizeof(mbuf));
    CopyStr(fmt, fbuf, sizeof(fbuf));
    FormatArgs(fmt, args, abuf, sizeof(abuf), first, sizeof(first));
    LogLine("CALL %s (%s)  fmt=%s", mbuf, abuf, fbuf);
    announce::OnCall(mbuf, first);
    return o_InvokeVa(self, edx, method, result, fmt, args);
}

// GFxMovieRoot invoke on an already-resolved character:
// (const char* path, character*, GFxValue* result, const char* argFmt, va_list args)
typedef bool(__fastcall* InvokeOn_t)(void* self, void* edx, const char* path, void* chr, GFxValue* result, const char* fmt, const char* args);
static InvokeOn_t o_InvokeOn;

static bool __fastcall h_InvokeOn(void* self, void* edx, const char* path, void* chr, GFxValue* result, const char* fmt, const char* args) {
    char pbuf[300], fbuf[60], abuf[2400], first[200];
    CopyStr(path, pbuf, sizeof(pbuf));
    CopyStr(fmt, fbuf, sizeof(fbuf));
    FormatArgs(fmt, args, abuf, sizeof(abuf), first, sizeof(first));
    LogLine("CALL movie=%p %s (%s)  fmt=%s", self, pbuf, abuf, fbuf);
    announce::OnCall(pbuf, first);
    return o_InvokeOn(self, edx, path, chr, result, fmt, args);
}

// Third Invoke form; its other two arguments aren't decoded yet.
typedef bool(__fastcall* InvokeOther_t)(void* self, void* edx, const char* method, void* a2, void* a3);
static InvokeOther_t o_InvokeOther;

static bool __fastcall h_InvokeOther(void* self, void* edx, const char* method, void* a2, void* a3) {
    char mbuf[300];
    CopyStr(method, mbuf, sizeof(mbuf));
    LogLine("CAL3 %s a2=%p a3=%p", mbuf, a2, a3);
    return o_InvokeOther(self, edx, method, a2, a3);
}

// GFxMovieRoot::GetVariable(GFxValue* out, const char* path). The main menu
// fetches text boxes this way, then sets their text through SetText below,
// so remember which object came from which path.
typedef bool(__fastcall* GetVariable_t)(void* self, void* edx, GFxValue* out, const char* path);
static GetVariable_t o_GetVariable;
static CRITICAL_SECTION g_pathLock;
struct ObjectInfo {
    std::string path;
    void* movie;
};
static std::unordered_map<void*, ObjectInfo> g_objectPaths;

static void* ObjectData(const GFxValue* v) {
    __try {
        return ((v->Type & 0x0F) >= 6) ? (void*)v->pString : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static bool __fastcall h_GetVariable(void* self, void* edx, GFxValue* out, const char* path) {
    bool ok = o_GetVariable(self, edx, out, path);
    if (ok) {
        void* data = ObjectData(out);
        if (data) {
            char pbuf[300];
            CopyStr(path, pbuf, sizeof(pbuf));
            EnterCriticalSection(&g_pathLock);
            if (g_objectPaths.size() > 20000) g_objectPaths.clear();
            g_objectPaths[data] = {pbuf, self};
            LeaveCriticalSection(&g_pathLock);
        }
    }
    return ok;
}

// GFxValue::ObjectInterface::SetText(void* object, const wchar_t* text, bool isHtml)
typedef bool(__fastcall* SetText_t)(void* self, void* edx, void* object, const wchar_t* text, bool html);
static SetText_t o_SetText;

static bool __fastcall h_SetText(void* self, void* edx, void* object, const wchar_t* text, bool html) {
    char tbuf[2200];
    CopyWStr(text, tbuf, sizeof(tbuf));
    std::string path;
    void* movie = nullptr;
    EnterCriticalSection(&g_pathLock);
    auto it = g_objectPaths.find(object);
    if (it != g_objectPaths.end()) {
        path = it->second.path;
        movie = it->second.movie;
    }
    LeaveCriticalSection(&g_pathLock);
    if (path.empty()) {
        char p[32];
        sprintf_s(p, "(object %p)", object);
        LogLine("TEXT %s = \"%s\"%s", p, tbuf, html ? "  (html)" : "");
    } else {
        LogLine("TEXT %s = \"%s\"%s", path.c_str(), tbuf, html ? "  (html)" : "");
        announce::OnText(path.c_str(), tbuf, movie);
    }
    return o_SetText(self, edx, object, text, html);
}

// GFxValue::ObjectInterface::Invoke(void* object, GFxValue* result, const char* name,
//                                   const GFxValue* args, UPInt numArgs, bool isDisplayObject)
// Menus that keep a handle to each item call methods on it directly; the
// pause menu's options get FocusGain() this way.
typedef bool(__fastcall* ObjInvoke_t)(void* self, void* edx, void* object, GFxValue* result, const char* name,
                                      const GFxValue* args, unsigned numArgs, bool isDisplayObject);
static ObjInvoke_t o_ObjInvoke;

static bool __fastcall h_ObjInvoke(void* self, void* edx, void* object, GFxValue* result, const char* name,
                                   const GFxValue* args, unsigned numArgs, bool isDisplayObject) {
    char nbuf[120], argText[1200] = "", first[200] = "", one[500];
    CopyStr(name, nbuf, sizeof(nbuf));
    for (unsigned i = 0; i < numArgs && i < 6; i++) {
        if (ValueText(&args[i], one, sizeof(one))) {
            if (i == 0) strcpy_s(first, one);
        } else {
            DescribeValue(&args[i], one, sizeof(one));
            if (i == 0) strcpy_s(first, one);
        }
        if (i) strcat_s(argText, ", ");
        strcat_s(argText, one);
    }
    std::string path;
    EnterCriticalSection(&g_pathLock);
    auto it = g_objectPaths.find(object);
    if (it != g_objectPaths.end()) path = it->second.path;
    LeaveCriticalSection(&g_pathLock);
    if (path.empty()) {
        LogLine("OBJ  (object %p).%s (%s)", object, nbuf, argText);
    } else {
        LogLine("OBJ  %s.%s (%s)", path.c_str(), nbuf, argText);
        announce::OnCall((path + "." + nbuf).c_str(), first);
    }
    return o_ObjInvoke(self, edx, object, result, name, args, numArgs, isDisplayObject);
}

// ---------------------------------------------------------------- install

struct Target {
    const char* name;
    uintptr_t va;            // address in the game exe (base 0x400000)
    uint8_t sig[12];
    void* detour;
    void** original;
};

// Mortal Kombat Komplete Edition, MKKE.exe built 2014-08-01.
static Target g_mk9Targets[] = {
    {"SetVariable", 0xAC7810, {0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x30, 0x00, 0x75, 0x09}, (void*)h_SetVariable, (void**)&o_SetVariable},
    {"InvokeVa",    0xAA1EA0, {0x53, 0x56, 0x8B, 0xF1, 0x85, 0xF6, 0x74, 0x08, 0xFF, 0x46, 0x04, 0x8D}, (void*)h_InvokeVa, (void**)&o_InvokeVa},
    {"InvokeOn",    0xABF7C0, {0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF9, 0x83, 0x7F, 0x30, 0x00, 0x0F, 0x84}, (void*)h_InvokeOn, (void**)&o_InvokeOn},
    {"InvokeOther", 0xAA3800, {0x53, 0x56, 0x8B, 0xF1, 0x85, 0xF6, 0x74, 0x08, 0xFF, 0x46, 0x04, 0x8D}, (void*)h_InvokeOther, (void**)&o_InvokeOther},
    {"GetVariable", 0xAB7B90, {0x83, 0xEC, 0x18, 0x53, 0x57, 0x8B, 0xF9, 0x83, 0x7F, 0x30, 0x00, 0x0F}, (void*)h_GetVariable, (void**)&o_GetVariable},
    {"SetText",     0xA646A0, {0x83, 0xEC, 0x20, 0x55, 0x8B, 0x6C, 0x24, 0x28, 0x56, 0x57, 0x8B, 0xF9}, (void*)h_SetText, (void**)&o_SetText},
    {"ObjInvoke",   0xA64510, {0x83, 0xEC, 0x34, 0x80, 0x7C, 0x24, 0x4C, 0x00, 0x57, 0x8B, 0xF9, 0x8B}, (void*)h_ObjInvoke, (void**)&o_ObjInvoke},
};

// Mortal Kombat Arcade Kollection, MKHDGame.exe built 2012-02-15. Same
// Scaleform GFx 3 code as MK9; only GFxMovieRoot's field offsets differ.
static Target g_arcadeTargets[] = {
    {"SetVariable", 0x1499270, {0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x68, 0x00, 0x75, 0x09}, (void*)h_SetVariable, (void**)&o_SetVariable},
    {"InvokeVa",    0x1415EB0, {0x53, 0x56, 0x8B, 0xF1, 0x85, 0xF6, 0x74, 0x08, 0xFF, 0x46, 0x04, 0x8D}, (void*)h_InvokeVa, (void**)&o_InvokeVa},
    {"InvokeOn",    0x1491230, {0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF9, 0x83, 0x7F, 0x68, 0x00, 0x0F, 0x84}, (void*)h_InvokeOn, (void**)&o_InvokeOn},
    {"InvokeOther", 0x1418090, {0x53, 0x56, 0x8B, 0xF1, 0x85, 0xF6, 0x74, 0x08, 0xFF, 0x46, 0x04, 0x8D}, (void*)h_InvokeOther, (void**)&o_InvokeOther},
    {"GetVariable", 0x1486320, {0x83, 0xEC, 0x18, 0x53, 0x57, 0x8B, 0xF9, 0x83, 0x7F, 0x68, 0x00, 0x0F}, (void*)h_GetVariable, (void**)&o_GetVariable},
    {"SetText",     0x13B24D0, {0x83, 0xEC, 0x20, 0x55, 0x8B, 0x6C, 0x24, 0x28, 0x56, 0x57, 0x8B, 0xF9}, (void*)h_SetText, (void**)&o_SetText},
    {"ObjInvoke",   0x13B2190, {0x83, 0xEC, 0x34, 0x80, 0x7C, 0x24, 0x4C, 0x00, 0x57, 0x8B, 0xF9, 0x8B}, (void*)h_ObjInvoke, (void**)&o_ObjInvoke},
};

struct Game {
    const wchar_t* exe;
    DWORD timestamp;  // PE header TimeDateStamp of the supported build
    Target* targets;
    size_t count;
};
static const Game kGames[] = {
    {L"MKKE.exe", 1406923673, g_mk9Targets, _countof(g_mk9Targets)},
    {L"MKHDGame.exe", 1329338420, g_arcadeTargets, _countof(g_arcadeTargets)},
};
static const Game* g_game;

static const Game* FindGame(const wchar_t* exeName) {
    for (auto& g : kGames) if (_wcsicmp(exeName, g.exe) == 0) return &g;
    return nullptr;
}

static bool IsExpectedBuild(HMODULE exe) {
    auto dos = (IMAGE_DOS_HEADER*)exe;
    auto nt = (IMAGE_NT_HEADERS32*)((uint8_t*)exe + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == g_game->timestamp;
}

static bool GameIsForeground() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static DWORD WINAPI HotkeyThread(LPVOID) {
    const int keys[] = {VK_F9, VK_F10, VK_F11};
    bool down[3] = {};
    for (;;) {
        for (int i = 0; i < 3; i++) {
            bool now = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
            if (now && !down[i] && GameIsForeground()) {
                if (keys[i] == VK_F9) announce::Repeat();
                else if (keys[i] == VK_F10) announce::Toggle();
                else {
                    LogLine("==== MARK (F11) ====");
                    Beep(880, 120);
                }
            }
            down[i] = now;
        }
        Sleep(30);
    }
}

static DWORD WINAPI InstallThread(LPVOID) {
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!IsExpectedBuild(exe)) {
        LogLine("Not the expected %ls build; no hooks installed.", g_game->exe);
        return 0;
    }
    uintptr_t base = (uintptr_t)exe;
    announce::Start();
    input::Install();
    for (size_t i = 0; i < g_game->count; i++) {
        Target& t = g_game->targets[i];
        uint8_t* addr = (uint8_t*)(base + (t.va - 0x400000));
        if (memcmp(addr, t.sig, sizeof(t.sig)) != 0) {
            LogLine("SKIP %s: bytes don't match", t.name);
            continue;
        }
        MH_STATUS s = MH_CreateHook(addr, t.detour, t.original);
        if (s == MH_OK) s = MH_EnableHook(addr);
        LogLine("HOOK %s at %p: %s", t.name, addr, MH_StatusToString(s));
    }
    CreateThread(nullptr, 0, HotkeyThread, nullptr, 0, nullptr);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        const wchar_t* exeName = wcsrchr(exePath, L'\\');
        exeName = exeName ? exeName + 1 : exePath;
        g_game = FindGame(exeName);
        if (!g_game) return TRUE;  // e.g. MKLauncher.exe: proxy only

        InitializeCriticalSection(&g_logLock);
        InitializeCriticalSection(&g_pathLock);
        g_startTick = GetTickCount();
        g_gameDir = exePath;
        g_gameDir.resize(g_gameDir.rfind(L'\\'));
        g_log = _wfsopen((g_gameDir + L"\\mk9hook_log.txt").c_str(), L"a", _SH_DENYWR);
        LogLine("---- mk9hook started in %ls ----", g_game->exe);
        // MinHook must be ready before the game calls DirectInput8Create,
        // which can happen before InstallThread runs. MH_Initialize only
        // creates a private heap, so it is safe under the loader lock.
        if (MH_Initialize() != MH_OK) {
            LogLine("MinHook failed to start.");
            return TRUE;
        }
        g_isGame = true;
        CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
