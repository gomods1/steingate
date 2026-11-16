// Keyboard hook through DirectInput. Game.exe reads the keyboard only through
// DirectInput8 (no user32 key APIs are imported or called; checked with
// tools/re/probe_input.py). All DirectInput devices of one interface flavour
// share a vtable inside dinput8.dll, so the mod creates its own keyboard
// device, patches GetDeviceState/GetDeviceData in that shared vtable (A and W
// flavours) and releases its device. This gives a per-frame callback on the
// game thread and lets the mod hide its keys from the game.
#define DIRECTINPUT_VERSION 0x0800
#include "dinput_hook.h"

#include <windows.h>
#include <dinput.h>

#include <cstring>
#include <set>

#include "input.h"
#include "log.h"

namespace sga {

namespace {

using GetDeviceStateProc = HRESULT(STDMETHODCALLTYPE*)(void* self, DWORD size, void* data);
using GetDeviceDataProc = HRESULT(STDMETHODCALLTYPE*)(void* self, DWORD od, DIDEVICEOBJECTDATA* data,
                                                      DWORD* inOut, DWORD flags);

struct Flavour {
    GetDeviceStateProc origState = nullptr;
    GetDeviceDataProc origData = nullptr;
};
Flavour g_a, g_w;
std::set<void*> g_keyboards;  // devices seen reading a 256-byte key state
bool g_loggedState = false, g_loggedData = false;

void PatchSlot(void** slot, void* fn) {
    DWORD old;
    VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    *slot = fn;
    VirtualProtect(slot, sizeof(void*), old, &old);
}

HRESULT OnState(Flavour& f, void* self, DWORD size, void* data) {
    HRESULT hr = f.origState(self, size, data);
    if (size == 256 && SUCCEEDED(hr) && data) {
        g_keyboards.insert(self);
        if (!g_loggedState) {
            g_loggedState = true;
            Log("keyboard read through GetDeviceState");
        }
        Input::OnKeyboardState((uint8_t*)data, size);
    }
    return hr;
}

HRESULT OnData(Flavour& f, void* self, DWORD od, DIDEVICEOBJECTDATA* data, DWORD* inOut, DWORD flags) {
    HRESULT hr = f.origData(self, od, data, inOut, flags);
    if (!g_keyboards.count(self) || FAILED(hr) || !data || !inOut || (flags & DIGDD_PEEK)) return hr;
    if (!g_loggedData) {
        g_loggedData = true;
        Log("keyboard read through GetDeviceData");
    }
    DWORD w = 0;
    for (DWORD r = 0; r < *inOut; ++r) {
        auto* e = (DIDEVICEOBJECTDATA*)((uint8_t*)data + r * od);
        if (Input::FilterEvent(e->dwOfs, (e->dwData & 0x80) != 0)) {
            if (w != r) memcpy((uint8_t*)data + w * od, e, od);
            ++w;
        }
    }
    *inOut = w;
    return hr;
}

HRESULT STDMETHODCALLTYPE StateA(void* s, DWORD n, void* d) { return OnState(g_a, s, n, d); }
HRESULT STDMETHODCALLTYPE StateW(void* s, DWORD n, void* d) { return OnState(g_w, s, n, d); }
HRESULT STDMETHODCALLTYPE DataA(void* s, DWORD od, DIDEVICEOBJECTDATA* d, DWORD* n, DWORD f) {
    return OnData(g_a, s, od, d, n, f);
}
HRESULT STDMETHODCALLTYPE DataW(void* s, DWORD od, DIDEVICEOBJECTDATA* d, DWORD* n, DWORD f) {
    return OnData(g_w, s, od, d, n, f);
}

bool HookFlavour(const IID& iid, Flavour& f, void* stateFn, void* dataFn) {
    using Create = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    HMODULE di = LoadLibraryW(L"dinput8.dll");
    auto create = di ? (Create)GetProcAddress(di, "DirectInput8Create") : nullptr;
    if (!create) return false;
    IDirectInput8W* dinput = nullptr;  // A and W share the vtable layout
    if (FAILED(create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, iid, (void**)&dinput, nullptr)))
        return false;
    IDirectInputDevice8W* dev = nullptr;
    HRESULT hr = dinput->CreateDevice(GUID_SysKeyboard, &dev, nullptr);
    if (FAILED(hr) || !dev) {
        dinput->Release();
        return false;
    }
    void** vt = *(void***)dev;
    if (vt[9] == (void*)&StateA || vt[9] == (void*)&StateW) {  // A and W share one vtable
        dev->Release();
        dinput->Release();
        return true;
    }
    f.origState = (GetDeviceStateProc)vt[9];
    f.origData = (GetDeviceDataProc)vt[10];
    PatchSlot(&vt[9], stateFn);
    PatchSlot(&vt[10], dataFn);
    dev->Release();
    dinput->Release();
    return true;
}

}  // namespace

bool InstallKeyboardHook() {
    bool a = HookFlavour(IID_IDirectInput8A, g_a, (void*)&StateA, (void*)&DataA);
    bool w = HookFlavour(IID_IDirectInput8W, g_w, (void*)&StateW, (void*)&DataW);
    Log("DirectInput keyboard hook: A %s, W %s", a ? "ok" : "FAILED", w ? "ok" : "FAILED");
    return a || w;
}

}  // namespace sga
