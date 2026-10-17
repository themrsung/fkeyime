#include "mode_machine.h"

namespace fkeyime {
namespace {

Effect Activate(Mode mode) { return {.kind = Effect::Kind::kActivateProfile, .mode = mode}; }

Effect Caps(bool on) { return {.kind = Effect::Kind::kSetCapsLock, .caps_lock = on}; }

Effect JaInput(JapaneseInput input) {
    return {.kind = Effect::Kind::kSetJapaneseInput, .japanese_input = input};
}

// One-way mode keys. Never early-return on "already in this mode": always re-assert the
// profile and the reset rules, so a stale state can't leave the key dead.
// `japanese_input` is the input a Japanese key selects: kana for F20, romaji for F16.
Result SwitchTo(const State& current, Mode target, const Policy& policy,
                JapaneseInput japanese_input = JapaneseInput::kKana) {
    Result r{.eaten = true, .next = current};
    r.next.mode = target;
    r.next.caps_lock = false;

    r.effects.push_back(Activate(target));
    r.effects.push_back(Caps(false));

    if (target == Mode::kJapanese) {
        const bool reset = current.mode != Mode::kJapanese || policy.japanese_key_resets_input;
        if (reset) r.next.japanese_input = japanese_input;
        r.effects.push_back(JaInput(r.next.japanese_input));
    } else {
        // Romaji is forgotten when you leave Japanese.
        r.next.japanese_input = JapaneseInput::kKana;
    }
    return r;
}

Result ContextKey(const State& current) {
    Result r{.eaten = true, .next = current};
    switch (current.mode) {
        case Mode::kKorean:
            r.effects.push_back({.kind = Effect::Kind::kConvertHanja});
            break;
        case Mode::kJapanese:
            r.next.japanese_input = current.japanese_input == JapaneseInput::kKana
                                        ? JapaneseInput::kRomaji
                                        : JapaneseInput::kKana;
            r.effects.push_back(JaInput(r.next.japanese_input));
            break;
        case Mode::kEnglish:
            r.next.caps_lock = !current.caps_lock;
            r.effects.push_back(Caps(r.next.caps_lock));
            break;
    }
    return r;
}

}  // namespace

Result OnFKey(const State& current, FKey key, bool has_modifiers, const Policy& policy) {
    if (has_modifiers) return {.eaten = false, .next = current};
    switch (key) {
        case FKey::kF18: return SwitchTo(current, Mode::kKorean, policy);
        case FKey::kF19: return SwitchTo(current, Mode::kEnglish, policy);
        case FKey::kF20: return SwitchTo(current, Mode::kJapanese, policy, JapaneseInput::kKana);
        case FKey::kF16:
            return SwitchTo(current, Mode::kJapanese, policy, JapaneseInput::kRomaji);
        case FKey::kF17: return ContextKey(current);
    }
    return {.eaten = false, .next = current};
}

bool MacLanguageKey(std::uint8_t scan, bool extended, bool key_up, FKey* key) {
    if (key_up ? extended : !extended) return false;
    switch (key_up ? scan | 0x80 : scan) {
        case 0xF1: *key = FKey::kF19; return true;  // 英数 (LANG2) -> English
        case 0xF2: *key = FKey::kF20; return true;  // かな (LANG1) -> Japanese, kana
    }
    return false;
}

FKey CycleKey(Mode current, JapaneseInput japanese) {
    switch (current) {
        case Mode::kEnglish: return FKey::kF18;
        case Mode::kKorean: return japanese == JapaneseInput::kRomaji ? FKey::kF16 : FKey::kF20;
        case Mode::kJapanese: return FKey::kF19;
    }
    return FKey::kF19;
}

}  // namespace fkeyime
