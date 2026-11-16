#include "input.h"

#include <windows.h>

#include <cstring>

#include "log.h"
#include "mod.h"

namespace sga {
namespace Input {

namespace {
uint8_t g_prev[256];
uint8_t g_cur[256];
bool g_mask[256];

// Ctrl is the game's "force skip" key and also the mod's review-buffer
// modifier. Ctrl is held back from the game for kCtrlHoldMs; if a mod combo
// is pressed meanwhile it stays hidden, otherwise it is passed through.
const DWORD kCtrlHoldMs = 300;
DWORD g_ctrlStart = 0;
bool g_ctrlCombo = false;
bool g_ctrlPassed = false;

bool Down(const uint8_t* s, int k) { return (s[k] & 0x80) != 0; }
bool Pressed(int k) { return Down(g_cur, k) && !Down(g_prev, k); }
}  // namespace

bool IsDown(int dik) { return Down(g_cur, dik); }

bool ForceSkipHeld() {
    return g_ctrlPassed && (Down(g_cur, KEY_LCTRL) || Down(g_cur, KEY_RCTRL));
}

void OnKeyboardState(uint8_t* state, uint32_t size) {
    if (size < 256) return;
    memcpy(g_cur, state, 256);
    DWORD now = GetTickCount();
    bool ctrl = Down(g_cur, KEY_LCTRL) || Down(g_cur, KEY_RCTRL);
    bool ctrlWas = Down(g_prev, KEY_LCTRL) || Down(g_prev, KEY_RCTRL);
    bool shift = Down(g_cur, KEY_LSHIFT) || Down(g_cur, KEY_RSHIFT);
    if (ctrl && !ctrlWas) {
        g_ctrlStart = now;
        g_ctrlCombo = false;
        g_ctrlPassed = false;
    }
    if (ctrl && !g_ctrlCombo && !g_ctrlPassed && now - g_ctrlStart > kCtrlHoldMs) g_ctrlPassed = true;
    bool modCtrl = ctrl && !g_ctrlPassed;

    memset(g_mask, 0, sizeof g_mask);
    bool captureAll = Mod::CapturesAllKeys();
    for (int k = 1; k < 256; ++k) {
        if (k == KEY_LCTRL || k == KEY_RCTRL) continue;  // Shift reaches OnKey: it hides the text window
        if (Pressed(k)) {
            bool claimed = false;
            __try {
                claimed = Mod::OnKey(k, modCtrl, shift);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("exception in OnKey %d", k);
            }
            if (claimed) {
                if (modCtrl) g_ctrlCombo = true;
                Mod::ClaimKey(k);
            }
        }
    }
    for (int k = 1; k < 256; ++k)
        if (Mod::IsKeyClaimed(k) && Down(g_cur, k)) g_mask[k] = true;
    if (captureAll) memset(g_mask, 1, sizeof g_mask);
    if (modCtrl || (ctrl && g_ctrlCombo) || captureAll) {
        g_mask[KEY_LCTRL] = g_mask[KEY_RCTRL] = true;
    }
    memcpy(g_prev, g_cur, 256);
    for (int k = 0; k < 256; ++k)
        if (g_mask[k]) state[k] = 0;
    // Released keys stop being claimed.
    for (int k = 1; k < 256; ++k)
        if (!Down(g_cur, k)) Mod::ReleaseKey(k);

    __try {
        Mod::Tick();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("exception in Tick");
    }
}

bool FilterEvent(uint32_t dik, bool down) {
    (void)down;
    return dik >= 256 || !g_mask[dik];
}

}  // namespace Input
}  // namespace sga
