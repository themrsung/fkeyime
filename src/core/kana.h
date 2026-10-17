// Japanese kana input composer (hiragana; no kanji conversion yet).
//
// Collects the kana typed with the JIS kana layout (jis::Kana) as the preedit. ゛ and ゜ combine
// with the kana before them (か゛ → が, は゜ → ぱ). Like the macOS Japanese IME, F6 / F7 / F8
// force the whole preedit to hiragana / full-width katakana / half-width katakana; the next kana
// typed commits that and starts a new preedit, and Escape undoes the forced form.
#pragma once

#include <string>
#include <string_view>

namespace fkeyime {

enum class KanaForm { kHiragana, kKatakana, kHalfwidthKatakana };

// Conversions used by the composer, exposed for tests.
std::u16string ToKatakana(std::u16string_view hiragana);
std::u16string ToHalfwidthKatakana(std::u16string_view hiragana);

class KanaComposer {
public:
    // Adds a kana or mark. Returns text committed by it (a forced form), often empty.
    std::u16string Input(char16_t kana);

    // F6 / F7 / F8. No effect when nothing is being composed.
    void Force(KanaForm form);

    // Removes the last kana. False when nothing is being composed.
    bool Backspace();

    // Undoes a forced form, or else cancels the preedit. False when nothing is being composed.
    bool Escape();

    // Ends the composition and returns its text in the current form.
    std::u16string Flush();

    // Replaces the preedit with `hiragana`, unforced (after a kanji conversion was cancelled).
    void Reset(std::u16string hiragana);

    std::u16string Preedit() const;
    const std::u16string& Hiragana() const { return raw_; }  // ignoring a forced form
    bool Empty() const { return raw_.empty(); }

private:
    std::u16string raw_;  // always hiragana
    KanaForm form_ = KanaForm::kHiragana;
    bool forced_ = false;
};

}  // namespace fkeyime
