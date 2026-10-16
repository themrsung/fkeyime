#include "mode_machine.h"

#include <algorithm>

#include "check.h"

using namespace fkeyide;

namespace {

bool Has(const Result& r, const Effect& e) {
    return std::find(r.effects.begin(), r.effects.end(), e) != r.effects.end();
}

const Effect kCapsOff{.kind = Effect::Kind::kSetCapsLock, .caps_lock = false};
const Effect kKana{.kind = Effect::Kind::kSetJapaneseInput,
                   .japanese_input = JapaneseInput::kKana};

Effect ActivateEffect(Mode m) { return {.kind = Effect::Kind::kActivateProfile, .mode = m}; }

}  // namespace

TEST_CASE("mode keys switch from any mode") {
    for (Mode from : {Mode::kEnglish, Mode::kKorean, Mode::kJapanese}) {
        State s{.mode = from};
        CHECK(OnFKey(s, FKey::kF18, false).next.mode == Mode::kKorean);
        CHECK(OnFKey(s, FKey::kF19, false).next.mode == Mode::kEnglish);
        CHECK(OnFKey(s, FKey::kF20, false).next.mode == Mode::kJapanese);
    }
}

TEST_CASE("one-way: same mode key re-applies instead of toggling or doing nothing") {
    State ko{.mode = Mode::kKorean};
    auto r = OnFKey(ko, FKey::kF18, false);
    CHECK(r.eaten);
    CHECK(r.next.mode == Mode::kKorean);
    CHECK(Has(r, ActivateEffect(Mode::kKorean)));
}

TEST_CASE("English F17 toggles transient Caps Lock") {
    State en{.mode = Mode::kEnglish};
    auto on = OnFKey(en, FKey::kF17, false);
    CHECK(on.next.caps_lock);
    auto off = OnFKey(on.next, FKey::kF17, false);
    CHECK(!off.next.caps_lock);
}

TEST_CASE("Caps Lock is turned off by every mode key, including F19 in English") {
    State en_caps{.mode = Mode::kEnglish, .caps_lock = true};
    for (FKey k : {FKey::kF18, FKey::kF19, FKey::kF20}) {
        auto r = OnFKey(en_caps, k, false);
        CHECK(!r.next.caps_lock);
        CHECK(Has(r, kCapsOff));
    }
}

TEST_CASE("Japanese always starts in kana, romaji is forgotten on leaving") {
    State ja_romaji{.mode = Mode::kJapanese, .japanese_input = JapaneseInput::kRomaji};
    auto left = OnFKey(ja_romaji, FKey::kF19, false);
    auto back = OnFKey(left.next, FKey::kF20, false);
    CHECK(back.next.japanese_input == JapaneseInput::kKana);
    CHECK(Has(back, kKana));
}

TEST_CASE("Japanese F17 toggles kana and romaji") {
    State ja{.mode = Mode::kJapanese};
    auto r = OnFKey(ja, FKey::kF17, false);
    CHECK(r.next.japanese_input == JapaneseInput::kRomaji);
    CHECK(OnFKey(r.next, FKey::kF17, false).next.japanese_input == JapaneseInput::kKana);
}

TEST_CASE("F20 in Japanese resets romaji only when the policy says so") {
    State ja_romaji{.mode = Mode::kJapanese, .japanese_input = JapaneseInput::kRomaji};
    CHECK(OnFKey(ja_romaji, FKey::kF20, false).next.japanese_input == JapaneseInput::kKana);
    Policy keep{.f20_resets_romaji = false};
    CHECK(OnFKey(ja_romaji, FKey::kF20, false, keep).next.japanese_input ==
          JapaneseInput::kRomaji);
}

TEST_CASE("Korean F17 asks for Hanja and keeps the mode") {
    State ko{.mode = Mode::kKorean};
    auto r = OnFKey(ko, FKey::kF17, false);
    CHECK(r.next.mode == Mode::kKorean);
    CHECK(Has(r, Effect{.kind = Effect::Kind::kConvertHanja}));
}

TEST_CASE("modified F-keys are not eaten and change nothing") {
    State ko{.mode = Mode::kKorean};
    for (FKey k : {FKey::kF17, FKey::kF18, FKey::kF19, FKey::kF20}) {
        auto r = OnFKey(ko, k, true);
        CHECK(!r.eaten);
        CHECK(r.effects.empty());
        CHECK(r.next.mode == Mode::kKorean);
    }
}

TEST_CASE("han/yeong key is passed through unless the policy turns it on") {
    for (Mode from : {Mode::kEnglish, Mode::kKorean, Mode::kJapanese}) {
        auto r = OnFKey(State{.mode = from}, FKey::kHanYeong, false);
        CHECK(!r.eaten);
        CHECK(r.next.mode == from);
        CHECK(r.effects.empty());
    }
}

TEST_CASE("han/yeong key toggles Korean <-> English, and goes to Korean from Japanese") {
    const Policy on{.han_yeong_toggles = true};
    auto from_ko = OnFKey(State{.mode = Mode::kKorean}, FKey::kHanYeong, false, on);
    CHECK(from_ko.eaten);
    CHECK(from_ko.next.mode == Mode::kEnglish);
    CHECK(Has(from_ko, ActivateEffect(Mode::kEnglish)));

    auto from_en = OnFKey(State{.mode = Mode::kEnglish, .caps_lock = true}, FKey::kHanYeong,
                          false, on);
    CHECK(from_en.next.mode == Mode::kKorean);
    CHECK(!from_en.next.caps_lock);
    CHECK(Has(from_en, kCapsOff));

    auto from_ja = OnFKey(State{.mode = Mode::kJapanese, .japanese_input = JapaneseInput::kRomaji},
                          FKey::kHanYeong, false, on);
    CHECK(from_ja.next.mode == Mode::kKorean);
    CHECK(from_ja.next.japanese_input == JapaneseInput::kKana);  // romaji forgotten
}

TEST_CASE("modified han/yeong key is not eaten even when the policy is on") {
    const Policy on{.han_yeong_toggles = true};
    auto r = OnFKey(State{.mode = Mode::kKorean}, FKey::kHanYeong, true, on);
    CHECK(!r.eaten);
    CHECK(r.next.mode == Mode::kKorean);
}
