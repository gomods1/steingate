#pragma once
#include <string>
#include <vector>

namespace sga {

// Which game screen is in front, from the script instructions that run every
// frame while a menu is open (TitleMenuMain, SystemMenuMain, SaveMenuMain...),
// and what is focused on it.
namespace Screens {
void InstallHooks();
// Game thread, once per frame. Announces screen changes and focus moves.
void Tick();
bool InMenu();               // a game menu (not dialogue) is in front
std::string CurrentName();   // spoken screen name, "" in dialogue
std::string CurrentFocus();  // spoken focused item, "" if unknown
std::vector<std::string> ScreenText();  // all text drawn on the current screen, as lines
}  // namespace Screens

}  // namespace sga
