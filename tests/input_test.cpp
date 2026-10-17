// JIS key tables, the Hangul 2-Set composer and the kana composer.
#include <string>

#include "check.h"
#include "hangul.h"
#include "jis_layout.h"
#include "kana.h"

using namespace fkeyime;

namespace {

// Feeds jamo one by one and returns everything committed plus the final preedit.
std::u16string TypeHangul(std::u16string_view jamo) {
    HangulComposer h;
    std::u16string out;
    for (char16_t c : jamo) out += h.Feed(c);
    return out + h.Flush();
}

}  // namespace

TEST_CASE("jis: English gives the JIS keycap symbols, macOS style") {
    CHECK(jis::Ascii(0x03, true) == u'"');            // Shift+2
    CHECK(jis::Ascii(0x08, true) == u'\'');           // Shift+7
    CHECK(jis::Ascii(jis::scan::kRo, false) == u'\\');
    CHECK(jis::Ascii(jis::scan::kRo, true) == u'_');
    CHECK(jis::Ascii(jis::scan::kYen, false) == u'¥');
    CHECK(jis::Ascii(jis::scan::kYen, true) == u'|');
    CHECK(jis::Ascii(0x1A, false) == u'@');
    CHECK(jis::Ascii(0x1A, true) == u'`');
    CHECK(jis::Ascii(0x28, false) == u':');
    CHECK(jis::Ascii(0x28, true) == u'*');
    CHECK(jis::Ascii(0x0D, true) == u'~');
    CHECK(jis::Ascii(0x0B, true) == 0);               // Shift+0: nothing on JIS
    CHECK(jis::Ascii(jis::scan::kSpace, false) == 0);
}

TEST_CASE("jis: letters follow Shift and Caps Lock") {
    CHECK(jis::Ascii(0x1E, false) == u'a');
    CHECK(jis::Ascii(0x1E, true) == u'A');
    CHECK(jis::Ascii(0x1E, false, true) == u'A');
    CHECK(jis::Ascii(0x1E, true, true) == u'a');
    CHECK(jis::Ascii(0x03, false, true) == u'2');     // Caps Lock doesn't touch digits
}

TEST_CASE("jis: 2-Set jamo and their shifted forms") {
    CHECK(jis::TwoSetJamo(0x10, false) == u'ㅂ');
    CHECK(jis::TwoSetJamo(0x10, true) == u'ㅃ');
    CHECK(jis::TwoSetJamo(0x18, true) == u'ㅒ');
    CHECK(jis::TwoSetJamo(0x25, true) == u'ㅏ');      // no shifted form
    CHECK(jis::TwoSetJamo(0x03, false) == 0);         // digits aren't jamo
}

TEST_CASE("jis: kana layout") {
    CHECK(jis::Kana(0x04, false) == u'あ');
    CHECK(jis::Kana(0x04, true) == u'ぁ');
    CHECK(jis::Kana(0x0B, true) == u'を');
    CHECK(jis::Kana(0x2C, true) == u'っ');
    CHECK(jis::Kana(0x1A, false) == u'゛');
    CHECK(jis::Kana(0x1B, true) == u'「');
    CHECK(jis::Kana(jis::scan::kYen, false) == u'ー');
    CHECK(jis::Kana(jis::scan::kRo, false) == u'ろ');
    CHECK(jis::Kana(0x34, true) == u'。');
}

TEST_CASE("hangul: words") {
    CHECK(TypeHangul(u"ㅇㅏㄴㄴㅕㅇㅎㅏㅅㅔㅇㅛ") == u"안녕하세요");
    CHECK(TypeHangul(u"ㅎㅏㄴㄱㅡㄹ") == u"한글");
    CHECK(TypeHangul(u"ㅂㅜㅔㄹㄱ") == u"뷁");
    CHECK(TypeHangul(u"ㅃㅏㄹㄹㅣ") == u"빨리");
}

TEST_CASE("hangul: a final moves to the next syllable before a vowel") {
    CHECK(TypeHangul(u"ㄱㅏㄱㅏ") == u"가가");
    CHECK(TypeHangul(u"ㄷㅏㄹㄱㅏ") == u"달가");  // 닭 + ㅏ: the double final splits
    CHECK(TypeHangul(u"ㅇㅓㅂㅅㅇㅓ") == u"없어");
}

TEST_CASE("hangul: compound vowels, lone jamo, and tense consonants") {
    CHECK(TypeHangul(u"ㅇㅗㅏ") == u"와");
    CHECK(TypeHangul(u"ㅡㅣ") == u"ㅢ");
    CHECK(TypeHangul(u"ㅏㅏ") == u"ㅏㅏ");
    CHECK(TypeHangul(u"ㄱㄱ") == u"ㄱㄱ");
    CHECK(TypeHangul(u"ㄱㅏㄸ") == u"가ㄸ");      // ㄸ can't be a final
    CHECK(TypeHangul(u"ㄱㅏㄸㅏ") == u"가따");
}

TEST_CASE("hangul: backspace removes one jamo at a time") {
    HangulComposer h;
    for (char16_t c : std::u16string_view(u"ㄱㅗㅏㄹㄱ")) h.Feed(c);
    CHECK(h.Preedit() == u"괅");
    CHECK(h.Backspace());
    CHECK(h.Preedit() == u"괄");
    CHECK(h.Backspace());
    CHECK(h.Preedit() == u"과");
    CHECK(h.Backspace());
    CHECK(h.Preedit() == u"고");
    CHECK(h.Backspace());
    CHECK(h.Preedit() == u"ㄱ");
    CHECK(h.Backspace());
    CHECK(h.Empty());
    CHECK(!h.Backspace());
}

TEST_CASE("hangul: after a final moved, backspace stays in the new syllable") {
    HangulComposer h;
    std::u16string out;
    for (char16_t c : std::u16string_view(u"ㄱㅏㄱㅏ")) out += h.Feed(c);
    CHECK(out == u"가");
    CHECK(h.Preedit() == u"가");
    CHECK(h.Backspace());
    CHECK(h.Preedit() == u"ㄱ");
}

TEST_CASE("kana: dakuten and handakuten combine with the kana before") {
    KanaComposer k;
    for (char16_t c : std::u16string_view(u"か゛は゜う゛")) k.Input(c);
    CHECK(k.Preedit() == u"がぱゔ");
    KanaComposer lone;
    lone.Input(u'゛');
    lone.Input(u'あ');
    lone.Input(u'゜');
    CHECK(lone.Preedit() == u"゛あ゜");
}

TEST_CASE("kana: F7 katakana, F8 half-width, F6 back to hiragana") {
    KanaComposer k;
    for (char16_t c : std::u16string_view(u"か゛っこう")) k.Input(c);
    k.Force(KanaForm::kKatakana);
    CHECK(k.Preedit() == u"ガッコウ");
    k.Force(KanaForm::kHalfwidthKatakana);
    CHECK(k.Preedit() == u"ｶﾞｯｺｳ");
    k.Force(KanaForm::kHiragana);
    CHECK(k.Preedit() == u"がっこう");
}

TEST_CASE("kana: typing after a forced form commits it") {
    KanaComposer k;
    k.Input(u'あ');
    k.Input(u'い');
    k.Force(KanaForm::kKatakana);
    CHECK(k.Input(u'う') == u"アイ");
    CHECK(k.Preedit() == u"う");
}

TEST_CASE("kana: escape undoes the forced form, then cancels") {
    KanaComposer k;
    k.Input(u'あ');
    k.Force(KanaForm::kHalfwidthKatakana);
    CHECK(k.Escape());
    CHECK(k.Preedit() == u"あ");
    CHECK(k.Escape());
    CHECK(k.Empty());
    CHECK(!k.Escape());
}

TEST_CASE("kana: backspace and empty forcing") {
    KanaComposer k;
    k.Force(KanaForm::kKatakana);  // nothing to force
    CHECK(k.Empty());
    k.Input(u'あ');
    k.Input(u'い');
    CHECK(k.Backspace());
    CHECK(k.Preedit() == u"あ");
    CHECK(k.Backspace());
    CHECK(k.Empty());
    CHECK(!k.Backspace());
}

TEST_CASE("kana: half-width conversion of marks and punctuation") {
    CHECK(ToHalfwidthKatakana(u"「ぱー」、。・") == u"｢ﾊﾟｰ｣､｡･");
    CHECK(ToHalfwidthKatakana(u"ぢゔ") == u"ﾁﾞｳﾞ");
    CHECK(ToKatakana(u"ゝゞー") == u"ヽヾー");
}
