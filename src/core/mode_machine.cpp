#include "mode_machine.h"

namespace fkeyide {
namespace {

Effect Activate(Mode mode) { return {.kind = Effect::Kind::kActivateProfile, .mode = mode}; }

Effect Caps(bool on) { return {.kind = Effect::Kind::kSetCapsLock, .caps_lock = on}; }

Effect JaInput(JapaneseInput input) {
    return {.kind = Effect::Kind::kSetJapaneseInput, .japanese_input = input};
}

// One-way mode keys. Never early-return on "already in this mode": always re-assert the
// profile and the reset rules, so a stale state can't leave the key dead.
Result SwitchTo(const State& current, Mode target, const Policy& policy) {
    Result r{.eaten = true, .next = current};
    r.next.mode = target;
    r.next.caps_lock = false;

    r.effects.push_back(Activate(target));
    r.effects.push_back(Caps(false));

    if (target == Mode::kJapanese) {
        const bool reset = current.mode != Mode::kJapanese || policy.f20_resets_romaji;
        if (reset) r.next.japanese_input = JapaneseInput::kKana;
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
    if (key == FKey::kHanYeong && !policy.han_yeong_toggles) {
        return {.eaten = false, .next = current};
    }
    switch (key) {
        case FKey::kF18: return SwitchTo(current, Mode::kKorean, policy);
        case FKey::kF19: return SwitchTo(current, Mode::kEnglish, policy);
        case FKey::kF20: return SwitchTo(current, Mode::kJapanese, policy);
        case FKey::kF17: return ContextKey(current);
        case FKey::kHanYeong:
            // Same reset rules as the mode keys; only the target depends on the current mode.
            return SwitchTo(current, current.mode == Mode::kKorean ? Mode::kEnglish : Mode::kKorean,
                            policy);
    }
    return {.eaten = false, .next = current};
}

}  // namespace fkeyide
