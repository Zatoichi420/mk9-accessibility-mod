#include "input.h"
#include "common.h"

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "MinHook.h"

namespace input {
namespace {

// ---------------------------------------------------------------- injected state

volatile uint8_t g_keys[256];      // DirectInput key codes (DIK_*) held by a script
volatile uint16_t g_padButtons;    // XInput button bits held by a script
volatile LONG g_padPacket = 1;
volatile bool g_injecting;         // a script has run since dev mode was seen

// ---------------------------------------------------------------- DirectInput

typedef HRESULT(__stdcall* CreateDevice_t)(void* self, REFGUID guid, void** device, void* outer);
typedef HRESULT(__stdcall* GetDeviceState_t)(void* self, DWORD size, void* data);
typedef HRESULT(__stdcall* GetDeviceData_t)(void* self, DWORD objSize, void* data, DWORD* count, DWORD flags);
CreateDevice_t o_CreateDevice;
GetDeviceState_t o_GetDeviceState;
GetDeviceData_t o_GetDeviceData;
LONG g_stateCalls, g_dataCalls;

struct DIDEVICEOBJECTDATA_ {
    DWORD dwOfs, dwData, dwTimeStamp, dwSequence;
    UINT_PTR uAppData;
};

HRESULT __stdcall h_GetDeviceState(void* self, DWORD size, void* data) {
    HRESULT hr = o_GetDeviceState(self, size, data);
    if (InterlockedIncrement(&g_stateCalls) == 1) LogLine("INPUT DirectInput GetDeviceState in use (size %lu)", size);
    if (size == 256 && g_injecting) {
        // A keyboard: merge in script-held keys. The device may be
        // unacquired while the game is in the background; report success.
        uint8_t* k = (uint8_t*)data;
        bool any = false;
        for (int i = 0; i < 256; i++) if (g_keys[i]) { k[i] |= 0x80; any = true; }
        if (any && FAILED(hr)) hr = S_OK;
    }
    return hr;
}

// Buffered keyboard events: queue a down and an up for each script press.
CRITICAL_SECTION g_bufLock;
std::vector<DIDEVICEOBJECTDATA_> g_bufEvents;
DWORD g_bufSeq = 0x40000000;

HRESULT __stdcall h_GetDeviceData(void* self, DWORD objSize, void* data, DWORD* count, DWORD flags) {
    // On entry *count is the caller's buffer capacity; on return, events written.
    DWORD capacity = count ? *count : 0;
    HRESULT hr = o_GetDeviceData(self, objSize, data, count, flags);
    if (InterlockedIncrement(&g_dataCalls) == 1) LogLine("INPUT DirectInput GetDeviceData in use (objSize %lu)", objSize);
    if (!g_injecting || !count || !data || objSize < sizeof(DWORD) * 4 || (flags & 1 /* DIGDD_PEEK */)) return hr;
    EnterCriticalSection(&g_bufLock);
    DWORD have = SUCCEEDED(hr) ? *count : 0;
    DWORD room = capacity > have ? capacity - have : 0;
    DWORD added = 0;
    while (!g_bufEvents.empty() && added < room) {
        uint8_t* slot = (uint8_t*)data + (have + added) * objSize;
        memcpy(slot, &g_bufEvents.front(), min((DWORD)sizeof(DIDEVICEOBJECTDATA_), objSize));
        g_bufEvents.erase(g_bufEvents.begin());
        added++;
    }
    LeaveCriticalSection(&g_bufLock);
    if (added) {
        *count = have + added;
        if (FAILED(hr)) hr = S_OK;
    }
    return hr;
}

HRESULT __stdcall h_CreateDevice(void* self, REFGUID guid, void** device, void* outer) {
    HRESULT hr = o_CreateDevice(self, guid, device, outer);
    if (SUCCEEDED(hr) && device && *device) {
        LogLine("INPUT DirectInput device created {%08lX-...}", guid.Data1);
        void** vtbl = *(void***)*device;
        if (!o_GetDeviceState) {
            MH_CreateHook(vtbl[9], (void*)h_GetDeviceState, (void**)&o_GetDeviceState);
            MH_CreateHook(vtbl[10], (void*)h_GetDeviceData, (void**)&o_GetDeviceData);
            MH_EnableHook(vtbl[9]);
            MH_EnableHook(vtbl[10]);
        }
    }
    return hr;
}

// ---------------------------------------------------------------- XInput

struct XINPUT_STATE_ {
    DWORD dwPacketNumber;
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
};
typedef DWORD(WINAPI* XInputGetState_t)(DWORD user, XINPUT_STATE_* state);
XInputGetState_t o_XInputGetState;
LONG g_padCalls;
WORD g_lastRealButtons;

DWORD WINAPI h_XInputGetState(DWORD user, XINPUT_STATE_* state) {
    DWORD r = o_XInputGetState(user, state);
    if (user == 0) {
        if (InterlockedIncrement(&g_padCalls) == 1) LogLine("INPUT XInput pad 1 polled: %s", r == 0 ? "connected" : "not connected");
        if (r == 0 && state->wButtons != g_lastRealButtons) {
            g_lastRealButtons = state->wButtons;
            if (g_lastRealButtons) LogLine("INPUT pad 1 buttons %04X", g_lastRealButtons);
        }
        if (g_injecting && (g_padButtons || r != 0)) {
            if (r != 0) memset(state, 0, sizeof(*state));
            state->wButtons |= g_padButtons;
            state->dwPacketNumber = (DWORD)InterlockedIncrement(&g_padPacket) + 0x10000;
            r = 0;
        }
    }
    return r;
}

// ---------------------------------------------------------------- scripts

struct Named {
    const char* name;
    int code;
};
const Named kKeys[] = {
    {"up", 0xC8}, {"down", 0xD0}, {"left", 0xCB}, {"right", 0xCD}, {"enter", 0x1C}, {"esc", 0x01},
    {"space", 0x39}, {"back", 0x0E}, {"a", 0x1E}, {"z", 0x2C}, {"g", 0x22}, {"j", 0x24}, {"x", 0x2D},
    {"y", 0x15}, {"tab", 0x0F}, {"pgup", 0xC9}, {"pgdn", 0xD1},
};
const Named kPad[] = {
    {"up", 0x0001}, {"down", 0x0002}, {"left", 0x0004}, {"right", 0x0008}, {"start", 0x0010},
    {"back", 0x0020}, {"lb", 0x0100}, {"rb", 0x0200}, {"a", 0x1000}, {"b", 0x2000}, {"x", 0x4000}, {"y", 0x8000},
};

int Lookup(const Named* table, size_t n, const std::string& name) {
    for (size_t i = 0; i < n; i++) if (name == table[i].name) return table[i].code;
    return -1;
}

void PushBuffered(int dik, bool down) {
    DIDEVICEOBJECTDATA_ e = {};
    e.dwOfs = (DWORD)dik;
    e.dwData = down ? 0x80 : 0;
    e.dwTimeStamp = GetTickCount();
    e.dwSequence = g_bufSeq++;
    EnterCriticalSection(&g_bufLock);
    g_bufEvents.push_back(e);
    LeaveCriticalSection(&g_bufLock);
}

void RunToken(const std::string& tok) {
    if (tok.size() < 3 || tok[1] != ':') return;
    std::string arg = tok.substr(2);
    if (tok[0] == 'w') {
        Sleep(atoi(arg.c_str()));
        return;
    }
    if (tok[0] == 'k') {
        int dik = Lookup(kKeys, _countof(kKeys), arg);
        if (dik < 0) { LogLine("SCRIPT unknown key %s", arg.c_str()); return; }
        LogLine("SCRIPT key %s", arg.c_str());
        g_keys[dik] = 1;
        PushBuffered(dik, true);
        Sleep(120);
        g_keys[dik] = 0;
        PushBuffered(dik, false);
        Sleep(350);
        return;
    }
    if (tok[0] == 'p') {
        int bit = Lookup(kPad, _countof(kPad), arg);
        if (bit < 0) { LogLine("SCRIPT unknown button %s", arg.c_str()); return; }
        LogLine("SCRIPT pad %s", arg.c_str());
        g_padButtons |= (uint16_t)bit;
        Sleep(120);
        g_padButtons &= (uint16_t)~bit;
        Sleep(350);
    }
}

DWORD WINAPI ScriptThread(LPVOID) {
    std::wstring devFlag = GameDir() + L"\\mk9hook_dev.txt";
    std::wstring cmdFile = GameDir() + L"\\mk9hook_cmd.txt";
    for (;;) {
        Sleep(100);
        if (GetFileAttributesW(devFlag.c_str()) == INVALID_FILE_ATTRIBUTES) {
            g_injecting = false;
            continue;
        }
        FILE* f = _wfsopen(cmdFile.c_str(), L"rb", _SH_DENYNO);
        if (!f) continue;
        std::string all;
        char buf[512];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) all.append(buf, n);
        fclose(f);
        DeleteFileW(cmdFile.c_str());
        g_injecting = true;
        std::string tok;
        for (char c : all + ",") {
            if (c == ',' || c == '\n' || c == '\r' || c == ' ') {
                if (!tok.empty()) RunToken(tok);
                tok.clear();
            } else {
                tok += (char)tolower((unsigned char)c);
            }
        }
        LogLine("SCRIPT done");
    }
}

}  // namespace

void Install() {
    InitializeCriticalSection(&g_bufLock);
    HMODULE xi = LoadLibraryW(L"xinput1_3.dll");
    void* target = xi ? (void*)GetProcAddress(xi, "XInputGetState") : nullptr;
    if (target) {
        MH_STATUS s = MH_CreateHook(target, (void*)h_XInputGetState, (void**)&o_XInputGetState);
        if (s == MH_OK) s = MH_EnableHook(target);
        LogLine("HOOK XInputGetState: %s", MH_StatusToString(s));
    }
    CreateThread(nullptr, 0, ScriptThread, nullptr, 0, nullptr);
}

void OnDirectInputCreated(void* directInput) {
    if (o_CreateDevice || !directInput) return;
    void** vtbl = *(void***)directInput;
    MH_STATUS s = MH_CreateHook(vtbl[3], (void*)h_CreateDevice, (void**)&o_CreateDevice);
    if (s == MH_OK) s = MH_EnableHook(vtbl[3]);
    LogLine("HOOK IDirectInput8::CreateDevice: %s", MH_StatusToString(s));
}

bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

}  // namespace input
