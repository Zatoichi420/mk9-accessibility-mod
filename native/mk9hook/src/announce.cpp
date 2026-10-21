#include "announce.h"
#include "common.h"
#include "speech.h"

#include <ctype.h>
#include <stdlib.h>
#include <algorithm>
#include <string.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace announce {
namespace {

struct MenuName {
    const char* desc;
    const char* name;
};
const MenuName kMenuNames[] = {
#include "menu_names.inc"
};

// Animation labels the game plays on the item that just got the highlight.
const char* const kFocusLabels[] = {
    "DownHighlight", "UpHighlight", "StaticHighlight", "GainFocus", "HighlightOn", "focusOn", "over",
};

struct Entry {
    std::string text;  // cleaned
    unsigned long seq;
};

std::mutex g_lock;
std::unordered_map<std::string, Entry> g_text;  // normalised path -> latest text
unsigned long g_seq;
bool g_muted;    // mk9hook.ini mute=1: log what would be said, say nothing
bool g_enabled = true;
std::string g_lastObj, g_lastSaid, g_lastExtra;
DWORD g_lastTick;
DWORD g_lastTitleTick;     // a screen title was just spoken; let it finish
std::unordered_map<std::string, std::string> g_lastInRoot;  // "MainNav" -> last highlighted item
std::unordered_map<std::string, int> g_slider;              // item -> slider position

bool EndsWith(const std::string& s, const char* suffix) {
    size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool Contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// "_root.ps.heads.head1.this._parent.Highlight1" -> "ps.heads.Highlight1"
std::string Normalize(const char* path) {
    std::vector<std::string> segs;
    std::string seg;
    for (const char* p = path;; p++) {
        if (*p == '.' || *p == 0) {
            if (seg == "_root") segs.clear();
            else if (seg == "_parent") { if (!segs.empty()) segs.pop_back(); }
            else if (!seg.empty() && seg != "this") segs.push_back(seg);
            seg.clear();
            if (*p == 0) break;
        } else {
            seg += *p;
        }
    }
    std::string out;
    for (auto& s : segs) {
        if (!out.empty()) out += '.';
        out += s;
    }
    return out;
}

struct Token {
    const char* tag;
    const char* words;
};
// Button and move symbols the game writes inside square brackets.
const Token kTokens[] = {
    {"A", "A button"}, {"B", "B button"}, {"X", "X button"}, {"Y", "Y button"},
    {"Start", "Start"}, {"START", "Start"}, {"Select", "Back button"}, {"SELECT", "Back button"}, {"BACK", "Back button"},
    {"LSH", "left bumper"}, {"RSH", "right bumper"}, {"LT", "left trigger"}, {"RT", "right trigger"},
    {"LTRIG", "left trigger"}, {"RTRIG", "right trigger"},
    {"U", "up"}, {"D", "down"}, {"L", "left"}, {"R", "right"},
    {"Up", "up"}, {"Down", "down"}, {"Left", "left"}, {"Right", "right"},
    {"Away", "away"}, {"Toward", "toward"}, {"Towards", "toward"},
    {"FP", "front punch"}, {"BP", "back punch"}, {"FK", "front kick"}, {"BK", "back kick"},
    {"BL", "block"}, {"TG", "tag"}, {"TAG", "tag"}, {"SS", "switch stance"}, {"H", "hand to hand"}, {"W", "weapon"},
    {"^", "jump"}, {",", ","}, {"+", "plus"}, {"-", "minus"},
    {"THROW", "throw"}, {"BLOCK", "block"}, {"Throw", "throw"}, {"Block", "block"},
    {"Key_Ent", "Enter"}, {"Key_Bksp", "Backspace"}, {"Key_Esc", "Escape"}, {"Key_Space", "Space"},
    {"Key_Tab", "Tab"}, {"Key_ArUp", "up arrow"}, {"Key_ArDwn", "down arrow"}, {"Key_ArLft", "left arrow"},
    {"Key_ArRht", "right arrow"}, {"Key_PgUp", "Page Up"}, {"Key_PgDn", "Page Down"},
    {"Key_LShft", "left Shift"}, {"Key_RShft", "right Shift"}, {"Key_LCtrl", "left Control"}, {"Key_RCtrl", "right Control"},
};

std::string Clean(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); i++) {
        char c = in[i];
        if (c == '<') {  // HTML tag
            size_t end = in.find('>', i);
            if (end != std::string::npos) { i = end; out += ' '; continue; }
        }
        if (c == '[') {
            size_t end = in.find(']', i + 1);
            if (end != std::string::npos && end - i < 16) {
                std::string tag = in.substr(i + 1, end - i - 1);
                const char* words = nullptr;
                for (auto& t : kTokens) if (tag == t.tag) { words = t.words; break; }
                std::string spoken = words ? words : (tag.compare(0, 4, "Key_") == 0 ? tag.substr(4) : tag);
                if (!out.empty() && out.back() != ' ') out += ' ';
                out += spoken;
                out += ' ';
                i = end;
                continue;
            }
        }
        out += (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
    }
    // Collapse spaces and trim.
    std::string tidy;
    for (char c : out) {
        if (c == ' ' && (tidy.empty() || tidy.back() == ' ')) continue;
        tidy += c;
    }
    while (!tidy.empty() && tidy.back() == ' ') tidy.pop_back();
    return tidy;
}

void Say(const std::string& text, bool interrupt) {
    if (text.empty() || !g_enabled) return;
    // The game often sets the same text twice in a frame (text and shadow).
    static std::string lastText;
    static DWORD lastTick;
    DWORD now = GetTickCount();
    if (text == lastText && now - lastTick < 300) return;
    lastText = text;
    lastTick = now;
    LogLine("SAY%s %s", interrupt ? "!" : " ", text.c_str());
    if (!g_muted) speech::Say(text, interrupt);
}

// On-screen key hints ("Select", "[A]") are not part of a menu item.
bool IsHint(const std::string& path) {
    std::string l = Lower(path);
    return Contains(l, "button") || Contains(l, ".nav.") || Contains(l, "navgroup") || Contains(l, "navigation") ||
           Contains(l, "prompt") || EndsWith(l, ".nav") || Contains(l, "nav_accept") || Contains(l, "nav_back");
}

bool IsUnder(const std::string& path, const std::string& obj) {
    return path == obj || (path.size() > obj.size() && path.compare(0, obj.size(), obj) == 0 && path[obj.size()] == '.');
}

// How early a text inside an item should be spoken: its name first, then
// other labels, then values.
int Rank(const std::string& path) {
    std::string l = Lower(path);
    if (EndsWith(l, ".label.varlabel") || Contains(l, "menuop") || Contains(l, "mainnav_text") ||
        Contains(l, ".name.") || EndsWith(l, ".name")) return 0;
    if (Contains(l, "label") || Contains(l, "title") || Contains(l, "name")) return 1;
    return 2;
}

std::string TextAt(const std::string& path) {
    auto it = g_text.find(path);
    return it == g_text.end() ? std::string() : it->second.text;
}

// The move list is a table of text_<row>_<column> fields; "<table>.@row<n>"
// stands for row n: "Uppercut, down plus back punch".
const char kMoveTable[] = "instance1.moves";

// All current text inside a menu item, name first, joined by commas.
std::string ItemText(const std::string& obj) {
    size_t at = obj.find(".@row");
    if (at != std::string::npos) {
        std::string base = obj.substr(0, at) + ".text_" + obj.substr(at + 5) + "_";
        std::string name = TextAt(base + "0"), command = TextAt(base + "1");
        return command.empty() ? name : name + ", " + command;
    }
    if (obj == "navAnim.@krypt") {
        // Krypt: the item in front of you; koin0 is the one nearest the camera.
        std::string label = TextAt("navAnim.purchaseInfo.label"), cost = TextAt("koin0.price.amount");
        if (label.empty()) return std::string();
        return cost.empty() ? label : label + ", cost " + cost + " koins";
    }
    if (EndsWith(obj, ".challengeInfo")) {
        // Challenge Tower card. Its "COMPLETED" and "SKIP COST" labels are
        // always set whether shown or not, so only the facts are read.
        std::string number = TextAt(obj + ".numberTextClip.numberText");
        std::string title = TextAt(obj + ".titleTextClipTop.titleText");
        std::string coins = TextAt(obj + ".reward.group.rewardCoins.text");
        if (title.empty()) return std::string();
        std::string out = number.empty() ? title : "Challenge " + number + ", " + title;
        if (!coins.empty()) out += ", reward " + coins + " koins";
        return out;
    }
    struct Part { int rank; std::string path, text; };
    std::vector<Part> parts;
    for (auto& kv : g_text) {
        if (!IsUnder(kv.first, obj) || kv.second.text.empty() || IsHint(kv.first)) continue;
        parts.push_back({Rank(kv.first), kv.first, kv.second.text});
    }
    std::sort(parts.begin(), parts.end(), [](const Part& a, const Part& b) {
        return a.rank != b.rank ? a.rank < b.rank : a.path < b.path;
    });
    std::string out;
    std::vector<std::string> said;
    for (auto& p : parts) {
        if (std::find(said.begin(), said.end(), p.text) != said.end()) continue;  // text and its drop shadow
        said.push_back(p.text);
        if (!out.empty()) out += ", ";
        out += p.text;
    }
    auto s = g_slider.find(obj);
    if (!out.empty() && s != g_slider.end()) out += ", " + std::to_string(s->second);
    return out;
}

std::string RootOf(const std::string& path) {
    size_t dot = path.find('.');
    return dot == std::string::npos ? path : path.substr(0, dot);
}

const char* MenuNameFor(const std::string& desc) {
    std::string d = Lower(desc);
    for (auto& m : kMenuNames) if (Lower(m.desc) == d) return m.name;
    return nullptr;
}

// Highlights are spoken ~60 ms after the game makes them. Screens often
// highlight an item in the same frame as (or just before) they fill in its
// text and the screen title, and the layers of one item are highlighted one
// after another; waiting for the frame to finish gets all of that right.
std::string g_queuedObj;
bool g_queuedForce;
DWORD g_queuedTick;
DWORD g_settleMs = 60;
DWORD g_lastHighlightCall;
const DWORD kSettleMs = 60, kBurstSettleMs = 350, kGiveUpMs = 1500;

void SpeakItem(const std::string& obj, bool force);

// force: speak even if it was the last thing said (returning to a menu).
void Highlight(const std::string& obj, bool force = false) {
    DWORD now = GetTickCount();
    DWORD gap = now - g_lastHighlightCall;
    g_lastHighlightCall = now;
    // Layers of one item arrive within a frame or two. Highlights a few
    // frames apart, faster than anyone presses, are the game scrolling by
    // itself (Challenge Tower opening); wait for it to stop.
    if (gap > 40 && gap < 300) g_settleMs = kBurstSettleMs;
    else if (gap >= 300) g_settleMs = kSettleMs;
    g_queuedObj = obj;
    g_queuedForce = force;
    g_queuedTick = now;
}

DWORD WINAPI SettleThread(LPVOID) {
    for (;;) {
        Sleep(15);
        std::lock_guard<std::mutex> g(g_lock);
        if (g_queuedObj.empty()) continue;
        DWORD age = GetTickCount() - g_queuedTick;
        if (age < g_settleMs) continue;
        std::string obj = g_queuedObj;
        if (ItemText(obj).empty() && !(obj.compare(0, 8, "MainNav.") == 0 && Contains(obj, ".navText"))) {
            if (age > kGiveUpMs) g_queuedObj.clear();  // nothing readable in it
            continue;
        }
        g_queuedObj.clear();
        SpeakItem(obj, g_queuedForce);
    }
}

void SpeakItem(const std::string& obj, bool force) {
    std::string text = ItemText(obj), extra;
    if (obj.compare(0, 8, "MainNav.") == 0 && Contains(obj, ".navText")) {
        // Main menu: the item's description is set just before its highlight.
        // Its name text isn't always refilled when a menu comes back (after
        // the credits, the old names linger), so the name the description
        // maps to wins.
        auto it = g_text.find("MainNav.descAnim.desc.text");
        if (it != g_text.end() && !it->second.text.empty()) {
            const char* name = MenuNameFor(it->second.text);
            if (name) text = name;
            else if (text.empty()) text = it->second.text;
            if (text != it->second.text) extra = it->second.text;
        }
    }
    DWORD now = GetTickCount();
    if (text.empty()) return;
    g_lastInRoot[RootOf(obj)] = obj;
    // The game plays the highlight on several layers of one item (text,
    // shadow, container) in the same frame; speak it once.
    if (!force && text == g_lastSaid && now - g_lastTick < 400) return;
    if (!force && obj == g_lastObj && text == g_lastSaid && now - g_lastTick < 800) return;
    g_lastObj = obj;
    g_lastSaid = text;
    g_lastExtra = extra;
    g_lastTick = now;
    Say(text, now - g_lastTitleTick > 600);  // don't cut off a screen title
    if (!extra.empty()) Say(extra, false);
}

void SayTitle(const std::string& text) {
    Say(text, true);
    g_lastTitleTick = GetTickCount();
}

// Pause-menu style FocusGain(n): find the item container numbered n.
void HighlightIndex(const std::string& n) {
    const std::string names[] = {"mc_option_" + n, "option" + n, "option_" + n, "track" + n, "item" + n};
    for (auto& kv : g_text) {
        const std::string& p = kv.first;
        for (auto& name : names) {
            size_t at = p.find("." + name + ".");
            if (at == std::string::npos) continue;
            Highlight(p.substr(0, at + 1 + name.size()));
            return;
        }
    }
}

bool IsFocusLabel(const char* label) {
    for (auto* l : kFocusLabels) if (_stricmp(label, l) == 0) return true;
    return false;
}

}  // namespace

void Start() {
    std::wstring ini = GameDir() + L"\\mk9hook.ini";
    g_muted = GetPrivateProfileIntW(L"speech", L"mute", 0, ini.c_str()) != 0;
    speech::Start(GameDir());
    CreateThread(nullptr, 0, SettleThread, nullptr, 0, nullptr);
    LogLine("Announcer ready%s", g_muted ? " (muted by mk9hook.ini)" : "");
}

void OnText(const char* rawPath, const char* rawText) {
    std::lock_guard<std::mutex> g(g_lock);
    std::string path = Normalize(rawPath);
    std::string text = Clean(rawText);
    Entry& e = g_text[path];
    bool changed = e.text != text;
    e.text = text;
    e.seq = ++g_seq;
    std::string lower = Lower(path);
    static bool fighterSelectOpened;  // names are cleared when the screen opens
    if (EndsWith(path, "vs_names.name_p1.name.text") && text.empty()) fighterSelectOpened = true;
    if (text.empty()) return;
    if (fighterSelectOpened && EndsWith(path, "vs_names.name_p1.name.text")) {
        fighterSelectOpened = false;
        SayTitle("Choose your fighter");
        Say(text, false);
        return;
    }
    // Message boxes and screen titles are announced every time the game
    // shows them, even with the same text as last time.
    bool isTitle = (EndsWith(lower, "titletxt") && Contains(lower, "header")) || EndsWith(lower, "title_text") ||
                   EndsWith(lower, "screentitletext") || EndsWith(lower, "description_title.vartext") ||
                   EndsWith(lower, "screentitle.vartext");
    bool isMsgBox = Contains(path, "msgBoxAnim.");
    bool isMoveListOpen = path == "instance1.prompt.txt_optionLeft";
    bool isLoading = EndsWith(path, "LoadText.var_loadTxt");
    if (!changed && !isTitle && !isMsgBox && !isMoveListOpen && !isLoading) return;

    if (!g_queuedObj.empty() && IsUnder(path, g_queuedObj)) {
        // Text for an item that's about to be spoken; SettleThread reads it.
    } else if (EndsWith(path, "name_p1.name.text") || EndsWith(path, "name_p2.name.text") ||
               EndsWith(path, "arena_name") || EndsWith(path, "num_reels")) {
        Say(text, true);
    } else if (EndsWith(path, "PlayerNumTrigger.Text.var_valueTxt")) {
        Say(text, false);
    } else if (EndsWith(path, "MainNav.descAnim.desc.text") && text == "Change the Number of Players.") {
        // The players switch under the main menu has no highlight animation.
        auto it = g_text.find("TriggerMotion.PlayerNumTrigger.Text.var_valueTxt");
        g_lastObj = "TriggerMotion.PlayerNumTrigger";
        g_lastSaid = (it != g_text.end() ? it->second.text : std::string("Players")) + ", number of players";
        g_lastExtra = "Left or right to change.";
        g_lastTick = GetTickCount();
        Say(g_lastSaid, true);
        Say(g_lastExtra, false);
    } else if (EndsWith(path, "msgBoxAnim.mainText.message")) {
        Say(text, true);
    } else if (EndsWith(path, "msgBoxAnim.navigationGroup.txt_optionLeftButton") ||
               EndsWith(path, "msgBoxAnim.navigationGroup.txt_optionRightButton")) {
        // The choice's label was set just before its button: "Yes, A button".
        std::string labelPath = path.substr(0, path.size() - 6);
        auto it = g_text.find(labelPath);
        Say((it != g_text.end() ? it->second.text + ", " : std::string()) + text, false);
    } else if (isMsgBox) {
        // Other message box parts (choice labels) are spoken with their buttons.
    } else if (isMoveListOpen) {
        SayTitle("Move list. " + text + ", A button.");
    } else if (isTitle) {
        // The player-select screen loads the team screen behind it, whose
        // title is wrong for one-player modes; the fighter names say enough.
        if (text != "CHOOSE YOUR TEAM") SayTitle(text);
    } else if (EndsWith(path, "press_start.start_string")) {
        Say(text == "PRESS SPACE" ? "Press Space, or Start on a controller" : text, false);
    } else if (EndsWith(path, "controllerPrompt0.promptLabel.text") && Contains(text, "Accept Button")) {
        Say(text + ". That's A on a controller, or G on the keyboard.", false);
    } else if (EndsWith(path, "promptLabel.text")) {
        Say(text, false);
    } else if (EndsWith(path, "LoadText.var_loadTxt")) {
        Say("Loading", false);
    } else if (path == "locationAnim.location.locationLabel") {
        SayTitle(text);  // Krypt area
    } else if (path == "navAnim.purchaseInfo.label") {
        Highlight("navAnim.@krypt");  // its cost is set right after
    } else if (!g_lastObj.empty() && IsUnder(path, g_lastObj) && !IsHint(path) &&
               (Contains(lower, "value") || Contains(lower, "optiontext") || Contains(lower, "optionlabel"))) {
        // A setting's value changed inside the highlighted item (left/right).
        Say(text, true);
    }
}

void OnCall(const char* rawPath, const char* firstArg) {
    std::lock_guard<std::mutex> g(g_lock);
    std::string path = Normalize(rawPath);
    size_t dot = path.rfind('.');
    std::string method = dot == std::string::npos ? path : path.substr(dot + 1);
    std::string obj = dot == std::string::npos ? "" : path.substr(0, dot);
    bool isGoto = method == "gotoAndPlay" || method == "gotoAndStop";
    std::string last = obj.substr(obj.rfind('.') + 1);
    if (isGoto && IsFocusLabel(firstArg)) {
        Highlight(obj);
    } else if (method == "gotoAndPlay" && _stricmp(firstArg, "BuildOn") == 0 &&
               last.size() > 6 && last.compare(0, 6, "option") == 0 && isdigit((unsigned char)last[6])) {
        // Pop-up lists (difficulty) mark their choice by building it on.
        Highlight(obj);
    } else if (method == "gotoAndPlay" && _stricmp(firstArg, "Activate") == 0) {
        auto it = g_text.find(obj + ".title.varText");  // a pop-up opened
        if (it != g_text.end() && !it->second.text.empty()) SayTitle(it->second.text);
    } else if (method == "FocusGain" && isdigit((unsigned char)firstArg[0])) {
        HighlightIndex(firstArg);
    } else if (method == "FocusGain") {
        Highlight(obj);  // pause menu: mc_option_N.FocusGain()
    } else if (method == "UpdateHighlight" && obj.empty() && isdigit((unsigned char)firstArg[0]) &&
               g_text.count(std::string(kMoveTable) + ".text_0_0")) {
        // Move list: the highlight index counts two per visible row.
        Highlight(std::string(kMoveTable) + ".@row" + std::to_string(atoi(firstArg) / 2));
    } else if (method == "gotoAndStop" && EndsWith(obj, ".slider") && isdigit((unsigned char)firstArg[0])) {
        // Volume-style sliders show their value as an animation frame.
        std::string item = obj.substr(0, obj.size() - 7);
        int value = atoi(firstArg);
        auto it = g_slider.find(item);
        bool changed = it != g_slider.end() && it->second != value;
        g_slider[item] = value;
        if (changed && item == g_lastObj) Say(std::to_string(value), true);
    } else if (method == "gotoAndPlay" && _stricmp(firstArg, "buildOn") == 0 && obj.find('.') == std::string::npos) {
        // A whole menu came back (for example after Back): say where we are,
        // unless the menu is highlighting an item itself right now.
        auto it = g_lastInRoot.find(obj);
        if (it != g_lastInRoot.end() && g_queuedObj.empty() && GetTickCount() - g_lastTick > 1000) Highlight(it->second, true);
    } else if (method == "gotoAndPlay" && _stricmp(firstArg, "BuildOn") == 0 && obj == "ps.nav") {
        // Back on the fighter grid after Player Options.
        std::string name = TextAt("ps.vs_names.name_p1.name.text");
        if (!name.empty()) Say(name, true);
    }
}

void Repeat() {
    std::lock_guard<std::mutex> g(g_lock);
    bool was = g_enabled;
    g_enabled = true;
    Say(g_lastSaid.empty() ? "Nothing to repeat" : g_lastSaid, true);
    if (!g_lastExtra.empty()) Say(g_lastExtra, false);
    g_enabled = was;
}

void Toggle() {
    std::lock_guard<std::mutex> g(g_lock);
    if (g_enabled) {
        Say("Reader off", true);
        g_enabled = false;
    } else {
        g_enabled = true;
        Say("Reader on", true);
    }
}

}  // namespace announce
