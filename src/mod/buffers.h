#pragma once
#include <deque>
#include <string>
#include <vector>

namespace sga {

// Review buffers (rpg.md 11.2): named lists, index 0 newest, cursor -1 until
// the player moves. Ctrl+Up/Down walk a buffer, Ctrl+Left/Right switch.
class Buffer {
public:
    Buffer(std::string name, size_t cap) : name_(std::move(name)), cap_(cap) {}
    void Add(const std::string& line);
    void Set(const std::vector<std::string>& lines);  // replaces content (focus-fed)
    void Clear();
    bool Empty() const { return items_.empty(); }
    const std::string& Name() const { return name_; }
    // Older = towards the end of the list (Ctrl+Up), newer = towards index 0.
    std::string Older();
    std::string Newer();
    void ResetCursor() { cursor_ = -1; }
    size_t Size() const { return items_.size(); }
    const std::string& At(size_t i) const { return items_[i]; }

private:
    std::string name_;
    size_t cap_;
    std::deque<std::string> items_;
    int cursor_ = -1;
};

namespace Buffers {
enum Id { Dialogue, Scene, Events, Details, Count };
Buffer& Get(Id id);
void Select(Id id);  // makes it current with cursor reset
// Key handlers; each returns the line to speak.
std::string Prev();
std::string Next();
std::string SwitchLeft();
std::string SwitchRight();
}  // namespace Buffers

}  // namespace sga
