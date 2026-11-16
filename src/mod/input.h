#pragma once
#include <cstdint>

namespace sga {

// Keyboard handling through the game's own DirectInput keyboard device: the
// mod sees each frame's key state and hides the keys it claims from the game.
namespace Input {
// Called from the DirectInput8 proxy after the game's GetDeviceState on the
// keyboard device (once per game frame). May modify state (masking).
void OnKeyboardState(uint8_t* state, uint32_t size);
// Buffered input: returns false when this DIK event must be hidden.
bool FilterEvent(uint32_t dik, bool down);
bool IsDown(int dik);
// Ctrl is held and has been passed to the game: it is force-skipping.
bool ForceSkipHeld();
}  // namespace Input

// DIK scancodes used by the mod.
enum Dik : int {
    KEY_ESC = 0x01, KEY_BACK = 0x0E, KEY_TAB = 0x0F, KEY_RET = 0x1C, KEY_LCTRL = 0x1D,
    KEY_LSHIFT = 0x2A, KEY_RSHIFT = 0x36, KEY_RCTRL = 0x9D, KEY_SPACE = 0x39,
    KEY_UP = 0xC8, KEY_DOWN = 0xD0, KEY_LEFT = 0xCB, KEY_RIGHT = 0xCD,
    KEY_HOME = 0xC7, KEY_END = 0xCF, KEY_PRIOR = 0xC9, KEY_NEXT = 0xD1,
    KEY_A = 0x1E, KEY_D = 0x20, KEY_H = 0x23, KEY_R = 0x13, KEY_T = 0x14, KEY_O = 0x18,
    KEY_S = 0x1F, KEY_W = 0x11, KEY_Q = 0x10, KEY_F1 = 0x3B, KEY_NUMPADENTER = 0x9C,
};

}  // namespace sga
