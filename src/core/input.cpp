#include "core/input.h"

#include <cstring>

namespace {
constexpr uint32_t kRepeatDelay = 280;
constexpr uint32_t kRepeatRate = 70;

bool repeats(Btn b) {
    switch (b) {
        case Btn::Up: case Btn::Down: case Btn::Left: case Btn::Right:
        case Btn::L1: case Btn::R1:
            return true;
        default:
            return false;
    }
}
}  // namespace

const char* btnName(Btn b) {
    static const char* names[kBtnCount] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1",
                                           "R1", "L2", "R2", "SELECT", "START", "MENU", "POWER",
                                           "VOL+", "VOL-"};
    int i = int(b);
    return (i >= 0 && i < kBtnCount) ? names[i] : "?";
}

// ---------------------------------------------------------------- key names
namespace {
bool g_keyboard = false;

struct KeyName {
    const char* pad;
    const char* key;
};
// Longer names first so "L1/R1" is matched before "L1".
// (0.27.0, owner: Esc = the game menu, Backspace = back, Tab = view the map;
// WASD moves the map camera, the arrow keys move the cursor and through lists)
const KeyName kKeyNames[] = {
    {"L1/R1", "Q/E"}, {"L2/R2", "Z/TAB"}, {"D-PAD", "ARROWS/WASD"}, {"SELECT", "R"},    {"START", "ENTER"},
    {"MENU", "ESC"},  {"L1", "Q"},        {"R1", "E"},         {"L2", "Z"},       {"R2", "TAB"},
    {"A", "SPACE"},   {"B", "BKSP"},      {"X", "X"},          {"Y", "V"},
};

bool g_deck = false;
const KeyName kDeckNames[] = {{"SELECT", "L3"}, {"START", "VIEW"}};

bool wordChar(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; }

bool endsWith(const std::string& s, const char* tail) {
    std::string t(tail);
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}
}  // namespace

void setKeyboardKeys(bool on) { g_keyboard = on; }
void setDeckKeys(bool on) { g_deck = on; }
bool keyboardKeys() { return g_keyboard; }

std::string keyLabel(const std::string& button) {
    if (g_deck && !g_keyboard) {
        for (const KeyName& k : kDeckNames)
            if (button == k.pad) return k.key;
        return button;
    }
    if (!g_keyboard) return button;
    for (const KeyName& k : kKeyNames)
        if (button == k.pad) return k.key;
    return button;
}

std::string keyText(const std::string& text) {
    if (g_deck && !g_keyboard) {  // only the two names that differ on the Deck
        std::string out = text;
        for (const KeyName& k : kDeckNames) {
            const std::string pad(k.pad);
            size_t at = 0;
            while ((at = out.find(pad, at)) != std::string::npos) {
                const bool before = at == 0 || !wordChar(out[at - 1]);
                const bool after = at + pad.size() >= out.size() || !wordChar(out[at + pad.size()]);
                if (before && after) {
                    out.replace(at, pad.size(), k.key);
                    at += std::strlen(k.key);
                } else {
                    at += pad.size();
                }
            }
        }
        return out;
    }
    if (!g_keyboard) return text;
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        bool done = false;
        if (i == 0 || !wordChar(text[i - 1])) {
            for (const KeyName& k : kKeyNames) {
                std::string pad(k.pad);
                if (text.compare(i, pad.size(), pad) != 0) continue;
                size_t end = i + pad.size();
                if (end < text.size() && wordChar(text[end])) continue;
                // One-letter buttons only where a sentence clearly means a button.
                if (pad.size() == 1) {
                    bool before = endsWith(out, "press ") || endsWith(out, "Press ") || endsWith(out, "for ") ||
                                  endsWith(out, "Hold ") || endsWith(out, "hold ");
                    bool after = text.compare(end, 3, " = ") == 0 || text.compare(end, 4, " to ") == 0 ||
                                 // 0.43.0 (owner: the Pass box said "A: Pass"): "A: ..." hints at a line start or after a gap
                                 (text.compare(end, 2, ": ") == 0 && (i == 0 || endsWith(out, "  ")));
                    if (!before && !after) continue;
                }
                out += k.key;
                i = end;
                done = true;
                break;
            }
        }
        if (!done) out += text[i++];
    }
    return out;
}

void Input::beginFrame() {
    for (int i = 0; i < kBtnCount; ++i) pressed_[i] = released_[i] = false;
    any_ = false;
    mouse_.moved = mouse_.leftPress = mouse_.click = mouse_.rightClick = false;
    mouse_.dragX = mouse_.dragY = mouse_.wheel = 0.f;
}

void Input::handle(Btn b, bool down, uint32_t now) {
    if (!valid(b)) return;
    int i = int(b);
    any_ = true;
    if (down && !held_[i]) {
        held_[i] = true;
        pressed_[i] = true;
        downAt_[i] = now;
        nextRepeat_[i] = now + kRepeatDelay;
    } else if (!down && held_[i]) {
        held_[i] = false;
        released_[i] = true;
    }
}

void Input::update(uint32_t now) {
    for (int i = 0; i < kBtnCount; ++i) {
        if (held_[i] && repeats(Btn(i)) && int32_t(now - nextRepeat_[i]) >= 0) {
            pressed_[i] = true;
            any_ = true;
            nextRepeat_[i] = now + kRepeatRate;
        }
    }
}
