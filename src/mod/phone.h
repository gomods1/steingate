#pragma once
#include <string>
#include <vector>

namespace sga {

// Okabe's phone (Phone Trigger): calls, mails and reply links. The phone UI
// is native; its text comes from the phone draw functions (TextCap) and its
// menu state from ScrWork 6800-6830 (CoZ LanguageBarrier handlePhoneMenuInput:
// 6802 phone screen, 6812 top-menu option).
namespace Phone {
void Tick(bool skipping);           // game thread, once per frame; quiet while skipping
bool Active();                      // the phone shows something
std::vector<std::string> Lines();   // everything the phone shows now
std::vector<std::string> BoardLines();  // in-scene document text (board, PC screen)
}  // namespace Phone

}  // namespace sga
