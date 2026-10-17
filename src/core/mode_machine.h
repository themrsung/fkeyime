// FKeyIME mode logic: what F17-F20 do, independent of TSF.
//
// Pure C++ with no Windows headers, so it can be unit-tested on its own. The TIP feeds key
// presses in and applies the returned effects (activate a profile, set Caps Lock, ...).
// Behavior spec: docs/spec.md.
#pragma once

#include <cstdint>
#include <vector>

namespace fkeyime {

enum class Mode : std::uint8_t { kEnglish, kKorean, kJapanese };

// F16 was added after F17-F20, so it comes last.
enum class FKey : std::uint8_t { kF17, kF18, kF19, kF20, kF16 };

enum class JapaneseInput : std::uint8_t { kKana, kRomaji };

// Global state, shared by every process's TIP instance (see docs/spec.md, "Shared state").
struct State {
    Mode mode = Mode::kEnglish;
    JapaneseInput japanese_input = JapaneseInput::kKana;  // only meaningful in kJapanese
    bool caps_lock = false;                               // transient, English only
};

// Owner decisions that are still open (build plan, section 6). Defaults are the proposals.
struct Policy {
    // A Japanese key pressed while already in Japanese re-applies its input: F20 resets to kana,
    // F16 to romaji (open question 1). When false, it keeps the current input.
    bool japanese_key_resets_input = true;
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

// The cycle key (options CycleKeys, an escape hatch for keyboards that can't send F16-F20): the
// F-key it stands for in `current`. English -> Korean -> Japanese -> English; Japanese is entered
// with F20 (kana) or F16 (romaji), per `japanese`.
FKey CycleKey(Mode current, JapaneseInput japanese);

// The Mac JIS language keys (option MacLangKeys, offered for the HHKB Studio's keymap Profile 1):
// 英数 (HID LANG2) acts as F19 (English) and かな (LANG1) as F20 (Japanese, kana), as on macOS.
// How Windows delivers them depends on the thread's layout (measured, HHKB Studio Profile 1):
// - Korean and Japanese layouts: one key-down, scan E0 F1 / E0 F2, no key-up.
// - US layout (FKeyIME EN): no key-down; two key-ups per press, scan 0x71 / 0x72.
// Matched by scan code because the virtual key changes with the layout (VK_HANJA, VK_KANJI,
// 0xE9, 0xFF). A Korean keyboard's 한자 and 한/영 keys send the same codes, so the option is
// opt-in. Returns false for any other key.
bool MacLanguageKey(std::uint8_t scan, bool extended, bool key_up, FKey* key);

}  // namespace fkeyime
