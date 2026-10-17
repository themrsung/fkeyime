#include "mode_machine.h"

#include <algorithm>

#include "check.h"

using namespace fkeyime;

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
    Policy keep{.japanese_key_resets_input = false};
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
    for (FKey k : {FKey::kF16, FKey::kF17, FKey::kF18, FKey::kF19, FKey::kF20}) {
        auto r = OnFKey(ko, k, true);
        CHECK(!r.eaten);
        CHECK(r.effects.empty());
        CHECK(r.next.mode == Mode::kKorean);
    }
}

TEST_CASE("F16 goes to Japanese with romaji input from any mode") {
    for (Mode from : {Mode::kEnglish, Mode::kKorean, Mode::kJapanese}) {
        auto r = OnFKey(State{.mode = from}, FKey::kF16, false);
        CHECK(r.eaten);
        CHECK(r.next.mode == Mode::kJapanese);
        CHECK(r.next.japanese_input == JapaneseInput::kRomaji);
        CHECK(Has(r, ActivateEffect(Mode::kJapanese)));
        CHECK(Has(r, Effect{.kind = Effect::Kind::kSetJapaneseInput,
                            .japanese_input = JapaneseInput::kRomaji}));
    }
}

TEST_CASE("F17 after F16 is transient kana, and F16 / F20 re-select their input") {
    auto romaji = OnFKey(State{}, FKey::kF16, false);
    auto kana = OnFKey(romaji.next, FKey::kF17, false);
    CHECK(kana.next.japanese_input == JapaneseInput::kKana);
    CHECK(OnFKey(kana.next, FKey::kF16, false).next.japanese_input == JapaneseInput::kRomaji);
    CHECK(OnFKey(romaji.next, FKey::kF20, false).next.japanese_input == JapaneseInput::kKana);
}

TEST_CASE("F16 in Japanese keeps the current input when the policy says so") {
    Policy keep{.japanese_key_resets_input = false};
    State ja_kana{.mode = Mode::kJapanese};
    CHECK(OnFKey(ja_kana, FKey::kF16, false, keep).next.japanese_input == JapaneseInput::kKana);
}

TEST_CASE("leaving Japanese forgets the transient input; F16 comes back in romaji") {
    auto kana = OnFKey(OnFKey(State{}, FKey::kF16, false).next, FKey::kF17, false);
    auto ko = OnFKey(kana.next, FKey::kF18, false);
    CHECK(OnFKey(ko.next, FKey::kF16, false).next.japanese_input == JapaneseInput::kRomaji);
    CHECK(OnFKey(ko.next, FKey::kF20, false).next.japanese_input == JapaneseInput::kKana);
}

TEST_CASE("cycle key: English -> Korean -> Japanese -> English") {
    CHECK(CycleKey(Mode::kEnglish, JapaneseInput::kKana) == FKey::kF18);
    CHECK(CycleKey(Mode::kKorean, JapaneseInput::kKana) == FKey::kF20);
    CHECK(CycleKey(Mode::kKorean, JapaneseInput::kRomaji) == FKey::kF16);
    CHECK(CycleKey(Mode::kJapanese, JapaneseInput::kKana) == FKey::kF19);
    CHECK(CycleKey(Mode::kJapanese, JapaneseInput::kRomaji) == FKey::kF19);

    // Three presses come back to the start, with Japanese in the chosen input.
    State s;
    for (int i = 0; i < 2; ++i) s = OnFKey(s, CycleKey(s.mode, JapaneseInput::kRomaji), false).next;
    CHECK(s.mode == Mode::kJapanese);
    CHECK(s.japanese_input == JapaneseInput::kRomaji);
    s = OnFKey(s, CycleKey(s.mode, JapaneseInput::kRomaji), false).next;
    CHECK(s.mode == Mode::kEnglish);
}

TEST_CASE("Mac 英数 / かな keys stand for F19 / F20, by scan code only") {
    FKey key = FKey::kF17;
    // Korean and Japanese layouts: one extended key-down.
    CHECK(MacLanguageKey(0xF1, true, false, &key));
    CHECK(key == FKey::kF19);
    CHECK(MacLanguageKey(0xF2, true, false, &key));
    CHECK(key == FKey::kF20);
    // US layout: key-ups with scan 0x71 / 0x72.
    CHECK(MacLanguageKey(0x71, false, true, &key));
    CHECK(key == FKey::kF19);
    CHECK(MacLanguageKey(0x72, false, true, &key));
    CHECK(key == FKey::kF20);
    CHECK(!MacLanguageKey(0xF1, false, false, &key));  // not the LANG2 code
    CHECK(!MacLanguageKey(0x72, false, false, &key));  // a 0x72 key-down isn't a LANG key
    CHECK(!MacLanguageKey(0xF2, true, true, &key));
    CHECK(!MacLanguageKey(0x29, false, false, &key));  // the ` key (VK_KANJI with Alt, JIS)
    CHECK(!MacLanguageKey(0x5C, true, false, &key));   // Right Win (right Command)
}
