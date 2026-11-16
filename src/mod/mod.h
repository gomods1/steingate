#pragma once
#include <windows.h>

namespace sga {

namespace Mod {
void Start(HMODULE self);  // worker thread entry: init, resolve, hook
void EarlyInit();          // log set-up; safe to call more than once
// Game thread, once per frame (from the keyboard poll).
void Tick();
bool OnKey(int dik, bool ctrl, bool shift);  // true = the mod claims the key
bool CapturesAllKeys();                       // a mod screen is open
void ClaimKey(int dik);
bool IsKeyClaimed(int dik);
void ReleaseKey(int dik);
}  // namespace Mod

}  // namespace sga
