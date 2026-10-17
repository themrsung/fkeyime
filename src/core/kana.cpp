#include "kana.h"

#include <string_view>

namespace fkeyime {
namespace {

constexpr char16_t kDakuten = u'゛';
constexpr char16_t kHandakuten = u'゜';

// Kana that take ゛ (the voiced kana is the next code point), and that take ゜ (two after).
constexpr std::u16string_view kDakutenBase = u"かきくけこさしすせそたちつてとはひふへほ";
constexpr std::u16string_view kHandakutenBase = u"はひふへほ";

// Hiragana (and marks) with a half-width katakana form, and that form, position by position.
constexpr std::u16string_view kHalfFrom =
    u"あぁいぃうぅえぇおぉかきくけこさしすせそたちつってとなにぬねのはひふへほまみむめも"
    u"やゃゆゅよょらりるれろわゎをんー、。「」・゛゜";
constexpr std::u16string_view kHalfTo =
    u"ｱｧｲｨｳｩｴｪｵｫｶｷｸｹｺｻｼｽｾｿﾀﾁﾂｯﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓ"
    u"ﾔｬﾕｭﾖｮﾗﾘﾙﾚﾛﾜﾜｦﾝｰ､｡｢｣･ﾞﾟ";
static_assert(kHalfFrom.size() == kHalfTo.size());

bool In(std::u16string_view set, char16_t c) { return set.find(c) != std::u16string_view::npos; }

char16_t Half(char16_t c) {
    const size_t i = kHalfFrom.find(c);
    return i == std::u16string_view::npos ? 0 : kHalfTo[i];
}

}  // namespace

std::u16string ToKatakana(std::u16string_view hiragana) {
    std::u16string out(hiragana);
    for (char16_t& c : out) {
        if ((c >= u'ぁ' && c <= u'ゖ') || c == u'ゝ' || c == u'ゞ') c = static_cast<char16_t>(c + 0x60);
    }
    return out;
}

std::u16string ToHalfwidthKatakana(std::u16string_view hiragana) {
    std::u16string out;
    for (char16_t c : hiragana) {
        if (char16_t h = Half(c)) {
            out += h;
        } else if (c == u'ゔ') {
            out += u"ｳﾞ";
        } else if (In(kDakutenBase, static_cast<char16_t>(c - 1))) {
            out += Half(static_cast<char16_t>(c - 1));
            out += u'ﾞ';
        } else if (In(kHandakutenBase, static_cast<char16_t>(c - 2))) {
            out += Half(static_cast<char16_t>(c - 2));
            out += u'ﾟ';
        } else {
            out += c;  // no half-width form: keep it
        }
    }
    return out;
}

std::u16string KanaComposer::Preedit() const {
    switch (form_) {
        case KanaForm::kKatakana: return ToKatakana(raw_);
        case KanaForm::kHalfwidthKatakana: return ToHalfwidthKatakana(raw_);
        case KanaForm::kHiragana: break;
    }
    return raw_;
}

std::u16string KanaComposer::Flush() {
    std::u16string text = Preedit();
    raw_.clear();
    form_ = KanaForm::kHiragana;
    forced_ = false;
    return text;
}

void KanaComposer::Reset(std::u16string hiragana) {
    raw_ = std::move(hiragana);
    form_ = KanaForm::kHiragana;
    forced_ = false;
}

std::u16string KanaComposer::Input(char16_t kana) {
    std::u16string out;
    if (forced_) out = Flush();  // typing after F6-F8 accepts the forced form

    if (!raw_.empty() && (kana == kDakuten || kana == kHandakuten)) {
        char16_t& last = raw_.back();
        if (kana == kDakuten && In(kDakutenBase, last)) {
            last = static_cast<char16_t>(last + 1);
            return out;
        }
        if (kana == kDakuten && last == u'う') {
            last = u'ゔ';
            return out;
        }
        if (kana == kDakuten && last == u'ゝ') {
            last = u'ゞ';
            return out;
        }
        if (kana == kHandakuten && In(kHandakutenBase, last)) {
            last = static_cast<char16_t>(last + 2);
            return out;
        }
    }
    raw_ += kana;
    return out;
}

void KanaComposer::Force(KanaForm form) {
    if (raw_.empty()) return;
    form_ = form;
    forced_ = true;
}

bool KanaComposer::Backspace() {
    if (raw_.empty()) return false;
    raw_.pop_back();
    if (raw_.empty()) Flush();
    return true;
}

bool KanaComposer::Escape() {
    if (raw_.empty()) return false;
    if (forced_ && form_ != KanaForm::kHiragana) {
        form_ = KanaForm::kHiragana;
        forced_ = false;
    } else {
        raw_.clear();
        Flush();
    }
    return true;
}

}  // namespace fkeyime
