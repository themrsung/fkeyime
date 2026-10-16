// FKeyIDE mode logic: what F17-F20 do, independent of TSF.
//
// Pure C++ with no Windows headers, so it can be unit-tested on its own. The TIP feeds key
// presses in and applies the returned effects (activate a profile, set Caps Lock, ...).
// Behavior spec: docs/spec.md.
#pragma once

#include <cstdint>
#include <vector>

namespace fkeyide {

enum class Mode : std::uint8_t { kEnglish, kKorean, kJapanese };

enum class FKey : std::uint8_t { kF17, kF18, kF19, kF20 };

enum class JapaneseInput : std::uint8_t { kKana, kRomaji };

// Global state, shared by every process's TIP instance (see docs/spec.md, "Shared state").
struct State {
    Mode mode = Mode::kEnglish;
    JapaneseInput japanese_input = JapaneseInput::kKana;  // only meaningful in kJapanese
    bool caps_lock = false;                               // transient, English only
};

// Owner decisions that are still open (build plan, section 6). Defaults are the proposals.
struct Policy {
    // F20 while already in Japanese resets romaji back to kana (open question 1).
    bool f20_resets_romaji = true;
};

struct Effect {
    enum class Kind : std::uint8_t {
        kActivateProfile,   // mode: switch to (or re-assert) this language profile
        kSetJapaneseInput,  // japanese_input: kana or romaji preedit
        kSetCapsLock,       // caps_lock: real system Caps Lock on/off
        kConvertHanja,      // Korean: Hanja for the composing syllable, or the one before the caret
    };
    Kind kind;
    Mode mode = Mode::kEnglish;
    JapaneseInput japanese_input = JapaneseInput::kKana;
    bool caps_lock = false;

    friend bool operator==(const Effect&, const Effect&) = default;
};

struct Result {
    bool eaten = false;  // false: the key is not ours (e.g. Shift+F18); let the app have it
    State next;
    std::vector<Effect> effects;
};

// `has_modifiers` is true when Shift, Ctrl, Alt or Win is held. Only bare F-keys act
// (open question 5: modified F-keys stay unused).
Result OnFKey(const State& current, FKey key, bool has_modifiers, const Policy& policy = {});

}  // namespace fkeyide
