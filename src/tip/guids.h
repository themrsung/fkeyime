// FKeyIME identifiers. These GUIDs are permanent: they end up in users' registries and
// language lists (InputMethodTips "<LANGID>:{CLSID}{PROFILE}"). Never regenerate them.
#pragma once

#include <guiddef.h>

namespace fkeyime {

// The TIP's COM class.
// {66749D2A-1770-43D3-A269-A051B5BC1AAA}
inline constexpr GUID kClsidTextService = {
    0x66749d2a, 0x1770, 0x43d3, {0xa2, 0x69, 0xa0, 0x51, 0xb5, 0xbc, 0x1a, 0xaa}};

// Language profiles under that CLSID (docs/spec.md, "Architecture").
// FKeyIME EN, en-US 0x0409. {8F61A2C0-75BC-4042-B739-CC7D63B542E5}
inline constexpr GUID kProfileEnglish = {
    0x8f61a2c0, 0x75bc, 0x4042, {0xb7, 0x39, 0xcc, 0x7d, 0x63, 0xb5, 0x42, 0xe5}};
// FKeyIME KO, ko-KR 0x0412. {28C201BF-390E-482F-8259-CD26E05844A9}
inline constexpr GUID kProfileKorean = {
    0x28c201bf, 0x390e, 0x482f, {0x82, 0x59, 0xcd, 0x26, 0xe0, 0x58, 0x44, 0xa9}};
// FKeyIME JA, ja-JP 0x0411. {1AC13F3E-07AC-4B03-A606-967BD48BFD99}
inline constexpr GUID kProfileJapanese = {
    0x1ac13f3e, 0x07ac, 0x4b03, {0xa6, 0x06, 0x96, 0x7b, 0xd4, 0x8b, 0xfd, 0x99}};

// Language bar / tray mode indicator button. {2F6B62C8-63E3-4086-85D3-1840F0ABD0E8}
inline constexpr GUID kLangBarItemMode = {
    0x2f6b62c8, 0x63e3, 0x4086, {0x85, 0xd3, 0x18, 0x40, 0xf0, 0xab, 0xd0, 0xe8}};

// Display attribute of the text being composed (underline). {CDB4ADB8-14FB-46A2-9AC9-EEE321451FD4}
inline constexpr GUID kDisplayAttributeInput = {
    0xcdb4adb8, 0x14fb, 0x46a2, {0x9a, 0xc9, 0xee, 0xe3, 0x21, 0x45, 0x1f, 0xd4}};

inline constexpr unsigned short kLangIdEnglish = 0x0409;
inline constexpr unsigned short kLangIdKorean = 0x0412;
inline constexpr unsigned short kLangIdJapanese = 0x0411;

// All three profiles substitute the Japanese 106/109 layout so symbols match the JIS keycaps
// in every mode. [verify] in M0 that hklSubstitute does this for en-US and ko-KR.
inline constexpr unsigned long kHklJapanese = 0x04110411;

}  // namespace fkeyime
