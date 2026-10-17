// Converts hiragana with FKeyIME's Mozc server through the same client the TIP uses.
//   fkeyime_mozc_cli.exe <server.exe> <hiragana> [keys]
// keys: s = Space (convert / next candidate), e = Enter (commit), x = Escape, b = Backspace,
//       l / r = Left / Right (segment), L / R = Shift+Left / Right (resize), 1-9 = candidate.
// Default keys: "se". Prints the preedit, the candidates and the result after each key.
#include <windows.h>

#include <cstdio>
#include <string>

#include "mozc_client.h"
#include "mozc_protocol.h"

namespace {

std::wstring Show(const std::string& utf8) {
    const std::u16string s = fkeyime::mozc::FromUtf8(utf8);
    std::wstring out;
    for (char16_t c : s) {
        if (c >= 0x20 && c < 0x7f) {
            out += static_cast<wchar_t>(c);
        } else {
            wchar_t b[8];
            swprintf_s(b, L"\\u%04X", c);
            out += b;
        }
    }
    return out;
}

void Print(const wchar_t* step, const fkeyime::mozc::Output& o) {
    std::wprintf(L"%s: consumed=%d", step, o.consumed);
    if (o.has_result) std::wprintf(L" result=[%s]", Show(o.result).c_str());
    if (o.has_preedit) {
        std::wprintf(L" preedit=");
        for (const auto& s : o.segments) std::wprintf(L"[%s:%u]", Show(s.value).c_str(), s.annotation);
    }
    std::wprintf(L"\n");
    if (o.has_candidates) {
        std::wprintf(L"  candidates %u total, focused %d:", o.size,
                     o.has_focused ? static_cast<int>(o.focused_index) : -1);
        for (const auto& c : o.candidates) {
            std::wprintf(L" %s%s=%s", c.shortcut.empty() ? L"" : Show(c.shortcut).c_str(),
                         c.shortcut.empty() ? L"" : L".", Show(c.value).c_str());
        }
        std::wprintf(L"\n");
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    using namespace fkeyime::mozc;
    if (argc < 3) {
        std::wprintf(L"usage: fkeyime_mozc_cli <server.exe> <hiragana> [keys]\n");
        return 1;
    }
    std::wprintf(L"pipe %s\n", fkeyime::MozcClient::PipeName().c_str());
    fkeyime::MozcClient client(argv[1]);
    const std::wstring hiragana = argv[2];
    const std::wstring keys = argc > 3 ? argv[3] : L"se";

    KeyEvent text;
    text.key_string = ToUtf8(std::u16string(hiragana.begin(), hiragana.end()));
    text.as_is = true;
    Output out;
    if (!client.SendKey(text, &out)) {
        std::wprintf(L"server unreachable\n");
        return 2;
    }
    Print(L"text", out);
    for (wchar_t k : keys) {
        KeyEvent key;
        switch (k) {
            case L's': key.special = SpecialKey::kSpace; break;
            case L'e': key.special = SpecialKey::kEnter; break;
            case L'x': key.special = SpecialKey::kEscape; break;
            case L'b': key.special = SpecialKey::kBackspace; break;
            case L'l': key.special = SpecialKey::kLeft; break;
            case L'r': key.special = SpecialKey::kRight; break;
            case L'L': key.special = SpecialKey::kLeft; key.modifiers = {kModShift}; break;
            case L'R': key.special = SpecialKey::kRight; key.modifiers = {kModShift}; break;
            default: key.key_code = static_cast<std::uint32_t>(k); break;
        }
        if (!client.SendKey(key, &out)) {
            std::wprintf(L"server unreachable\n");
            return 2;
        }
        const wchar_t step[2] = {k, 0};
        Print(step, out);
    }
    client.Close();
    return 0;
}
