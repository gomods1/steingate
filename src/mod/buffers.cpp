#include "buffers.h"

#include "loc.h"

namespace sga {

void Buffer::Add(const std::string& line) {
    if (line.empty()) return;
    items_.push_front(line);
    if (items_.size() > cap_) items_.pop_back();
    if (cursor_ >= 0) ++cursor_;  // keep the cursor on the same item
    if (cursor_ >= (int)items_.size()) cursor_ = (int)items_.size() - 1;
}

void Buffer::Set(const std::vector<std::string>& lines) {
    items_.assign(lines.begin(), lines.end());
    cursor_ = -1;
}

void Buffer::Clear() {
    items_.clear();
    cursor_ = -1;
}

// The Details buffer is read top-down (first line first), the history
// buffers bottom-up (newest first); both use Older() for Ctrl+Up.
std::string Buffer::Older() {
    if (items_.empty()) return Loc::T("buffer_empty");
    if (cursor_ + 1 >= (int)items_.size()) return Loc::T("buffer_top") + " " + items_[cursor_ < 0 ? 0 : cursor_];
    ++cursor_;
    return items_[cursor_];
}

std::string Buffer::Newer() {
    if (items_.empty()) return Loc::T("buffer_empty");
    if (cursor_ <= 0) {
        cursor_ = 0;
        return Loc::T("buffer_bottom") + " " + items_[0];
    }
    --cursor_;
    return items_[cursor_];
}

namespace Buffers {
namespace {
Buffer g_buffers[Count] = {
    Buffer("dialogue", 500), Buffer("scene", 100), Buffer("events", 200), Buffer("details", 200),
};
int g_current = Dialogue;
const char* kNameKeys[Count] = {"buf_dialogue", "buf_scene", "buf_events", "buf_details"};

std::string Describe(int id) {
    Buffer& b = g_buffers[id];
    return Loc::Format("buffer_switch", {{"name", Loc::T(kNameKeys[id])}, {"count", std::to_string(b.Size())}});
}
}  // namespace

Buffer& Get(Id id) { return g_buffers[id]; }

void Select(Id id) {
    g_current = id;
    g_buffers[id].ResetCursor();
}

std::string Prev() { return g_buffers[g_current].Older(); }
std::string Next() { return g_buffers[g_current].Newer(); }

std::string SwitchLeft() {
    for (int i = 1; i < Count; ++i) {
        int id = (g_current - i + Count) % Count;
        if (!g_buffers[id].Empty()) {
            Select((Id)id);
            return Describe(id);
        }
    }
    return Describe(g_current);
}

std::string SwitchRight() {
    for (int i = 1; i < Count; ++i) {
        int id = (g_current + i) % Count;
        if (!g_buffers[id].Empty()) {
            Select((Id)id);
            return Describe(id);
        }
    }
    return Describe(g_current);
}
}  // namespace Buffers

}  // namespace sga
