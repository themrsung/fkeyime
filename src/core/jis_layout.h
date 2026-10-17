// FKeyIME's own key tables, by PS/2 set-1 scan code (what Windows reports for the HHKB Studio
// and every other keyboard), so the text never depends on the Windows keyboard layout
// (docs/spec.md, decision 8).
#pragma once

#include <cstdint>

namespace fkeyime::jis {

// Scan codes FKeyIME cares about (JIS 106/109 positions).
namespace scan {
inline constexpr std::uint8_t kBackspace = 0x0E;
inline constexpr std::uint8_t kTab = 0x0F;
inline constexpr std::uint8_t kEnter = 0x1C;
inline constexpr std::uint8_t kSpace = 0x39;
inline constexpr std::uint8_t kEscape = 0x01;
inline constexpr std::uint8_t kYen = 0x7D;  // ¥, left of Backspace
inline constexpr std::uint8_t kRo = 0x73;   // ろ, right of /
}  // namespace scan

// English (and the symbols in Korean): the JIS keycap legends, macOS style. ¥ types ¥ (U+00A5),
// ろ types \ and _. Letters are lower case; `caps` makes them upper case (Shift reverses it).
// Returns 0 for keys that aren't in the table (Enter, Space, arrows, ...).
char16_t Ascii(std::uint8_t scan, bool shift, bool caps = false);

// Korean 2-Set (두벌식): the compatibility jamo (U+3131..) on a letter key, or 0. Shift gives
// ㅃㅉㄸㄲㅆㅒㅖ; other shifted letters give the plain jamo.
char16_t TwoSetJamo(std::uint8_t scan, bool shift);

// Japanese JIS kana input (かな入力): the hiragana or mark on a key, or 0. Includes ゛ (U+309B)
// and ゜ (U+309C), which the kana composer combines with the previous kana.
char16_t Kana(std::uint8_t scan, bool shift);

}  // namespace fkeyime::jis
