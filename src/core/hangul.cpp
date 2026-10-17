#include "hangul.h"

#include <iterator>
#include <string_view>

namespace fkeyime {
namespace {

// Compatibility jamo in Unicode syllable order.
constexpr std::u16string_view kCho = u"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
constexpr std::u16string_view kJung = u"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
// Index 0 is "no final".
constexpr std::u16string_view kJong = u" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ";

struct Pair {
    char16_t first, second, combined;
};
constexpr Pair kCompoundVowels[] = {
    {u'ㅗ', u'ㅏ', u'ㅘ'}, {u'ㅗ', u'ㅐ', u'ㅙ'}, {u'ㅗ', u'ㅣ', u'ㅚ'}, {u'ㅜ', u'ㅓ', u'ㅝ'},
    {u'ㅜ', u'ㅔ', u'ㅞ'}, {u'ㅜ', u'ㅣ', u'ㅟ'}, {u'ㅡ', u'ㅣ', u'ㅢ'},
};
constexpr Pair kDoubleFinals[] = {
    {u'ㄱ', u'ㅅ', u'ㄳ'}, {u'ㄴ', u'ㅈ', u'ㄵ'}, {u'ㄴ', u'ㅎ', u'ㄶ'}, {u'ㄹ', u'ㄱ', u'ㄺ'},
    {u'ㄹ', u'ㅁ', u'ㄻ'}, {u'ㄹ', u'ㅂ', u'ㄼ'}, {u'ㄹ', u'ㅅ', u'ㄽ'}, {u'ㄹ', u'ㅌ', u'ㄾ'},
    {u'ㄹ', u'ㅍ', u'ㄿ'}, {u'ㄹ', u'ㅎ', u'ㅀ'}, {u'ㅂ', u'ㅅ', u'ㅄ'},
};

int IndexIn(std::u16string_view set, char16_t c) {
    const size_t i = set.find(c);
    return i == std::u16string_view::npos ? -1 : static_cast<int>(i);
}

char16_t Combine(const Pair* begin, const Pair* end, char16_t a, char16_t b) {
    for (const Pair* p = begin; p != end; ++p) {
        if (p->first == a && p->second == b) return p->combined;
    }
    return 0;
}

const Pair* SplitFinal(char16_t combined) {
    for (const Pair& p : kDoubleFinals) {
        if (p.combined == combined) return &p;
    }
    return nullptr;
}

}  // namespace

std::u16string HangulComposer::Render(const Syllable& s) {
    if (s.cho >= 0 && s.jung >= 0) {
        return std::u16string(1, static_cast<char16_t>(0xAC00 + (s.cho * 21 + s.jung) * 28 + s.jong));
    }
    if (s.cho >= 0) return std::u16string(1, kCho[s.cho]);
    if (s.jung >= 0) return std::u16string(1, kJung[s.jung]);
    return {};
}

std::u16string HangulComposer::Preedit() const {
    return history_.empty() ? std::u16string() : Render(history_.back());
}

std::u16string HangulComposer::Commit() {
    std::u16string text = Preedit();
    history_.clear();
    return text;
}

std::u16string HangulComposer::Flush() { return Commit(); }

void HangulComposer::Start(const Syllable& s) { history_.push_back(s); }

bool HangulComposer::Backspace() {
    if (history_.empty()) return false;
    history_.pop_back();
    return true;
}

std::u16string HangulComposer::Feed(char16_t jamo) {
    const Syllable cur = history_.empty() ? Syllable{} : history_.back();
    const int cho = IndexIn(kCho, jamo);
    const int jung = IndexIn(kJung, jamo);

    if (cho >= 0) {  // consonant
        if (cur.cho < 0 && cur.jung < 0) {
            Start({cho});
            return {};
        }
        if (cur.cho >= 0 && cur.jung >= 0) {
            if (cur.jong == 0) {
                const int jong = IndexIn(kJong, jamo);
                if (jong > 0) {  // ㄸ ㅃ ㅉ can't be finals
                    history_.push_back({cur.cho, cur.jung, jong});
                    return {};
                }
            } else if (char16_t both = Combine(std::begin(kDoubleFinals), std::end(kDoubleFinals),
                                               kJong[cur.jong], jamo)) {
                history_.push_back({cur.cho, cur.jung, IndexIn(kJong, both)});
                return {};
            }
        }
        // Consonant after a lone consonant or vowel, or one that can't extend the final.
        std::u16string out = Commit();
        Start({cho});
        return out;
    }

    if (jung < 0) return {};  // not a jamo

    // vowel
    if (cur.cho < 0 && cur.jung < 0) {
        Start({-1, jung});
        return {};
    }
    if (cur.jung < 0) {  // initial consonant only
        history_.push_back({cur.cho, jung});
        return {};
    }
    if (cur.jong == 0) {
        if (char16_t both = Combine(std::begin(kCompoundVowels), std::end(kCompoundVowels),
                                    kJung[cur.jung], jamo)) {
            history_.push_back({cur.cho, IndexIn(kJung, both)});
            return {};
        }
        std::u16string out = Commit();
        Start({-1, jung});
        return out;
    }
    // The final moves to the next syllable (a double final gives up its second part): 각+ㅏ → 가가.
    Syllable kept = cur;
    char16_t moving = kJong[cur.jong];
    if (const Pair* pair = SplitFinal(moving)) {
        kept.jong = IndexIn(kJong, pair->first);
        moving = pair->second;
    } else {
        kept.jong = 0;
    }
    std::u16string out = Render(kept);
    history_.clear();
    const int next_cho = IndexIn(kCho, moving);
    Start({next_cho});
    history_.push_back({next_cho, jung});
    return out;
}

}  // namespace fkeyime
