#pragma once

namespace sga {
// Patches the shared DirectInput8 device vtables (see dinput_hook.cpp).
bool InstallKeyboardHook();
}  // namespace sga
