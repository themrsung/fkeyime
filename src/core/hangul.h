// Korean 2-Set (두벌식) syllable composer.
//
// Fed one compatibility jamo per key (jis::TwoSetJamo). Keeps the syllable being typed as the
// preedit and returns the text that leaves it (committed). Backspace removes the last jamo, so
// a compound vowel or double final splits back (ㅘ → ㅗ, ㄳ → ㄱ), like the Microsoft Korean IME.
#pragma once

#include <string>
#include <vector>

namespace fkeyime {

class HangulComposer {
public:
    // Adds a jamo. Returns text committed by it (the previous syllable), often empty.
    std::u16string Feed(char16_t jamo);

    // Removes the last jamo of the current syllable. False when nothing is being composed.
    bool Backspace();

    // Ends the composition and returns its text.
    std::u16string Flush();

    std::u16string Preedit() const;
    bool Empty() const { return history_.empty(); }

private:
    struct Syllable {
        int cho = -1;   // initial consonant index (0..18), -1 none
        int jung = -1;  // vowel index (0..20), -1 none
        int jong = 0;   // final index (1..27), 0 none
    };
    static std::u16string Render(const Syllable& s);
    std::u16string Commit();
    void Start(const Syllable& s);

    // The current syllable after each of its jamo; back() is what's shown.
    std::vector<Syllable> history_;
};

}  // namespace fkeyime
