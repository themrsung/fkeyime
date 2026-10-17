#include "jis_layout.h"

namespace fkeyime::jis {
namespace {

struct Key {
    std::uint8_t scan;
    char16_t normal;
    char16_t shifted;
};

// JIS keycaps. Shift+0 types nothing on JIS.
constexpr Key kAscii[] = {
    {0x02, u'1', u'!'}, {0x03, u'2', u'"'}, {0x04, u'3', u'#'}, {0x05, u'4', u'$'},
    {0x06, u'5', u'%'}, {0x07, u'6', u'&'}, {0x08, u'7', u'\''}, {0x09, u'8', u'('},
    {0x0A, u'9', u')'}, {0x0B, u'0', 0},    {0x0C, u'-', u'='}, {0x0D, u'^', u'~'},
    {scan::kYen, u'¥', u'|'},
    {0x1A, u'@', u'`'}, {0x1B, u'[', u'{'},
    {0x27, u';', u'+'}, {0x28, u':', u'*'}, {0x2B, u']', u'}'},
    {0x33, u',', u'<'}, {0x34, u'.', u'>'}, {0x35, u'/', u'?'}, {scan::kRo, u'\\', u'_'},
};

// Letter scan codes in QWERTY order of the key rows.
constexpr Key kLetters[] = {
    {0x10, u'q', 0}, {0x11, u'w', 0}, {0x12, u'e', 0}, {0x13, u'r', 0}, {0x14, u't', 0},
    {0x15, u'y', 0}, {0x16, u'u', 0}, {0x17, u'i', 0}, {0x18, u'o', 0}, {0x19, u'p', 0},
    {0x1E, u'a', 0}, {0x1F, u's', 0}, {0x20, u'd', 0}, {0x21, u'f', 0}, {0x22, u'g', 0},
    {0x23, u'h', 0}, {0x24, u'j', 0}, {0x25, u'k', 0}, {0x26, u'l', 0},
    {0x2C, u'z', 0}, {0x2D, u'x', 0}, {0x2E, u'c', 0}, {0x2F, u'v', 0}, {0x30, u'b', 0},
    {0x31, u'n', 0}, {0x32, u'm', 0},
};

// 2-Set: jamo per letter, and the shifted (tensed) jamo where there is one.
constexpr Key kTwoSet[] = {
    {0x10, u'ㅂ', u'ㅃ'}, {0x11, u'ㅈ', u'ㅉ'}, {0x12, u'ㄷ', u'ㄸ'}, {0x13, u'ㄱ', u'ㄲ'},
    {0x14, u'ㅅ', u'ㅆ'}, {0x15, u'ㅛ', 0},     {0x16, u'ㅕ', 0},     {0x17, u'ㅑ', 0},
    {0x18, u'ㅐ', u'ㅒ'}, {0x19, u'ㅔ', u'ㅖ'},
    {0x1E, u'ㅁ', 0},     {0x1F, u'ㄴ', 0},     {0x20, u'ㅇ', 0},     {0x21, u'ㄹ', 0},
    {0x22, u'ㅎ', 0},     {0x23, u'ㅗ', 0},     {0x24, u'ㅓ', 0},     {0x25, u'ㅏ', 0},
    {0x26, u'ㅣ', 0},
    {0x2C, u'ㅋ', 0},     {0x2D, u'ㅌ', 0},     {0x2E, u'ㅊ', 0},     {0x2F, u'ㅍ', 0},
    {0x30, u'ㅠ', 0},     {0x31, u'ㅜ', 0},     {0x32, u'ㅡ', 0},
};

// JIS kana layout (かな入力). Shifted entries are the small kana and the punctuation; 0 means
// "same as unshifted".
constexpr Key kKana[] = {
    {0x02, u'ぬ', 0},     {0x03, u'ふ', 0},     {0x04, u'あ', u'ぁ'}, {0x05, u'う', u'ぅ'},
    {0x06, u'え', u'ぇ'}, {0x07, u'お', u'ぉ'}, {0x08, u'や', u'ゃ'}, {0x09, u'ゆ', u'ゅ'},
    {0x0A, u'よ', u'ょ'}, {0x0B, u'わ', u'を'}, {0x0C, u'ほ', 0},     {0x0D, u'へ', 0},
    {scan::kYen, u'ー', 0},
    {0x10, u'た', 0},     {0x11, u'て', 0},     {0x12, u'い', u'ぃ'}, {0x13, u'す', 0},
    {0x14, u'か', 0},     {0x15, u'ん', 0},     {0x16, u'な', 0},     {0x17, u'に', 0},
    {0x18, u'ら', 0},     {0x19, u'せ', 0},     {0x1A, u'゛', 0},     {0x1B, u'゜', u'「'},
    {0x1E, u'ち', 0},     {0x1F, u'と', 0},     {0x20, u'し', 0},     {0x21, u'は', 0},
    {0x22, u'き', 0},     {0x23, u'く', 0},     {0x24, u'ま', 0},     {0x25, u'の', 0},
    {0x26, u'り', 0},     {0x27, u'れ', 0},     {0x28, u'け', 0},     {0x2B, u'む', u'」'},
    {0x2C, u'つ', u'っ'}, {0x2D, u'さ', 0},     {0x2E, u'そ', 0},     {0x2F, u'ひ', 0},
    {0x30, u'こ', 0},     {0x31, u'み', 0},     {0x32, u'も', 0},     {0x33, u'ね', u'、'},
    {0x34, u'る', u'。'}, {0x35, u'め', u'・'}, {scan::kRo, u'ろ', 0},
};

template <size_t N>
const Key* Find(const Key (&table)[N], std::uint8_t scan) {
    for (const Key& k : table) {
        if (k.scan == scan) return &k;
    }
    return nullptr;
}

}  // namespace

char16_t Ascii(std::uint8_t scan, bool shift, bool caps) {
    if (const Key* k = Find(kLetters, scan)) {
        return shift != caps ? static_cast<char16_t>(k->normal - u'a' + u'A') : k->normal;
    }
    if (const Key* k = Find(kAscii, scan)) return shift ? k->shifted : k->normal;
    return 0;
}

char16_t TwoSetJamo(std::uint8_t scan, bool shift) {
    const Key* k = Find(kTwoSet, scan);
    if (!k) return 0;
    return shift && k->shifted ? k->shifted : k->normal;
}

char16_t Kana(std::uint8_t scan, bool shift) {
    const Key* k = Find(kKana, scan);
    if (!k) return 0;
    return shift && k->shifted ? k->shifted : k->normal;
}

}  // namespace fkeyime::jis
