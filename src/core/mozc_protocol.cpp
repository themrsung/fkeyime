#include "mozc_protocol.h"

namespace fkeyime::mozc {
namespace {

// --- writing ---------------------------------------------------------------------------------

enum WireType : std::uint32_t { kVarint = 0, kFixed64 = 1, kLength = 2, kStartGroup = 3,
                                kEndGroup = 4, kFixed32 = 5 };

void PutVarint(std::string& out, std::uint64_t v) {
    while (v >= 0x80) {
        out += static_cast<char>((v & 0x7F) | 0x80);
        v >>= 7;
    }
    out += static_cast<char>(v);
}

void PutTag(std::string& out, std::uint32_t field, WireType type) {
    PutVarint(out, (static_cast<std::uint64_t>(field) << 3) | type);
}

void PutUint(std::string& out, std::uint32_t field, std::uint64_t v) {
    PutTag(out, field, kVarint);
    PutVarint(out, v);
}

void PutBytes(std::string& out, std::uint32_t field, std::string_view bytes) {
    PutTag(out, field, kLength);
    PutVarint(out, bytes.size());
    out.append(bytes);
}

// commands.proto Input: type = 1, id = 2, key = 3, command = 4.
std::string Input(std::uint32_t type, std::uint64_t session, bool with_session) {
    std::string out;
    PutUint(out, 1, type);
    if (with_session) PutUint(out, 2, session);
    return out;
}

// --- reading ---------------------------------------------------------------------------------

class Reader {
public:
    explicit Reader(std::string_view bytes) : p_(bytes) {}

    bool Done() const { return p_.empty(); }

    bool Varint(std::uint64_t* v) {
        *v = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            if (p_.empty()) return false;
            const auto b = static_cast<std::uint8_t>(p_.front());
            p_.remove_prefix(1);
            *v |= static_cast<std::uint64_t>(b & 0x7F) << shift;
            if (!(b & 0x80)) return true;
        }
        return false;
    }

    bool Tag(std::uint32_t* field, std::uint32_t* type) {
        std::uint64_t tag;
        if (!Varint(&tag)) return false;
        *field = static_cast<std::uint32_t>(tag >> 3);
        *type = static_cast<std::uint32_t>(tag & 7);
        return *field != 0;
    }

    bool Bytes(std::string_view* bytes) {
        std::uint64_t n;
        if (!Varint(&n) || n > p_.size()) return false;
        *bytes = p_.substr(0, static_cast<size_t>(n));
        p_.remove_prefix(static_cast<size_t>(n));
        return true;
    }

    // Skips one value of `type`. For a start group, skips to its end group (of `field`).
    bool Skip(std::uint32_t field, std::uint32_t type) {
        std::uint64_t v;
        std::string_view s;
        switch (type) {
            case kVarint: return Varint(&v);
            case kFixed64: return Advance(8);
            case kLength: return Bytes(&s);
            case kFixed32: return Advance(4);
            case kStartGroup: {
                std::uint32_t f, t;
                while (Tag(&f, &t)) {
                    if (t == kEndGroup) return f == field;
                    if (!Skip(f, t)) return false;
                }
                return false;
            }
        }
        return false;
    }

private:
    bool Advance(size_t n) {
        if (n > p_.size()) return false;
        p_.remove_prefix(n);
        return true;
    }
    std::string_view p_;
};

// Reads the fields of a group until its end tag. `on_field` handles the fields it knows and
// returns false for the others, which are skipped.
template <typename Fn>
bool ReadGroup(Reader& r, std::uint32_t group_field, Fn on_field) {
    std::uint32_t f, t;
    while (r.Tag(&f, &t)) {
        if (t == kEndGroup) return f == group_field;
        bool handled = false;
        if (!on_field(r, f, t, &handled)) return false;
        if (!handled && !r.Skip(f, t)) return false;
    }
    return false;
}

template <typename Fn>
bool ReadMessage(std::string_view bytes, Fn on_field) {
    Reader r(bytes);
    std::uint32_t f, t;
    while (!r.Done()) {
        if (!r.Tag(&f, &t)) return false;
        bool handled = false;
        if (!on_field(r, f, t, &handled)) return false;
        if (!handled && !r.Skip(f, t)) return false;
    }
    return true;
}

bool ReadString(Reader& r, std::uint32_t type, std::string* out) {
    std::string_view s;
    if (type != kLength || !r.Bytes(&s)) return false;
    out->assign(s);
    return true;
}

template <typename T>
bool ReadUint(Reader& r, std::uint32_t type, T* out) {
    std::uint64_t v;
    if (type != kVarint || !r.Varint(&v)) return false;
    *out = static_cast<T>(v);
    return true;
}

// candidate_window.proto Annotation: shortcut = 4, description = 3.
bool ParseAnnotation(std::string_view bytes, Candidate* c) {
    return ReadMessage(bytes, [&](Reader& r, std::uint32_t f, std::uint32_t t, bool* handled) {
        if (f == 4) return *handled = true, ReadString(r, t, &c->shortcut);
        if (f == 3) return *handled = true, ReadString(r, t, &c->description);
        return true;
    });
}

// candidate_window.proto CandidateWindow: focused_index = 1, size = 2, group Candidate = 3
// {index = 4, value = 5, id = 9, annotation = 7}, position = 6, category = 11.
bool ParseCandidateWindow(std::string_view bytes, Output* out) {
    out->has_candidates = true;
    return ReadMessage(bytes, [&](Reader& r, std::uint32_t f, std::uint32_t t, bool* handled) {
        if (f == 1) {
            out->has_focused = true;
            return *handled = true, ReadUint(r, t, &out->focused_index);
        }
        if (f == 2) return *handled = true, ReadUint(r, t, &out->size);
        if (f == 11) return *handled = true, ReadUint(r, t, &out->category);
        if (f == 3 && t == kStartGroup) {
            *handled = true;
            Candidate c;
            const bool ok = ReadGroup(r, 3, [&](Reader& g, std::uint32_t gf, std::uint32_t gt,
                                                bool* gh) {
                if (gf == 4) return *gh = true, ReadUint(g, gt, &c.index);
                if (gf == 5) return *gh = true, ReadString(g, gt, &c.value);
                if (gf == 9) return *gh = true, ReadUint(g, gt, &c.id);
                if (gf == 7) {
                    std::string_view a;
                    *gh = true;
                    return gt == kLength && g.Bytes(&a) && ParseAnnotation(a, &c);
                }
                return true;
            });
            out->candidates.push_back(std::move(c));
            return ok;
        }
        return true;
    });
}

// commands.proto Preedit: cursor = 1, group Segment = 2 {annotation = 3, value = 4}.
bool ParsePreedit(std::string_view bytes, Output* out) {
    out->has_preedit = true;
    return ReadMessage(bytes, [&](Reader& r, std::uint32_t f, std::uint32_t t, bool* handled) {
        if (f == 1) return *handled = true, ReadUint(r, t, &out->cursor);
        if (f == 2 && t == kStartGroup) {
            *handled = true;
            Segment s;
            const bool ok = ReadGroup(r, 2, [&](Reader& g, std::uint32_t gf, std::uint32_t gt,
                                                bool* gh) {
                if (gf == 3) return *gh = true, ReadUint(g, gt, &s.annotation);
                if (gf == 4) return *gh = true, ReadString(g, gt, &s.value);
                return true;
            });
            out->segments.push_back(std::move(s));
            return ok;
        }
        return true;
    });
}

}  // namespace

std::string CreateSession() { return Input(1, 0, false); }

std::string DeleteSession(std::uint64_t session) { return Input(2, session, true); }

std::string SendKey(std::uint64_t session, const KeyEvent& key) {
    // KeyEvent: key_code = 1, special_key = 3, modifier_keys = 4, key_string = 5, input_style = 6.
    std::string k;
    if (key.key_code) PutUint(k, 1, key.key_code);
    if (key.special != SpecialKey::kNone) PutUint(k, 3, static_cast<std::uint32_t>(key.special));
    for (std::uint32_t m : key.modifiers) PutUint(k, 4, m);
    if (!key.key_string.empty()) PutBytes(k, 5, key.key_string);
    if (key.as_is) PutUint(k, 6, 1);
    std::string out = Input(3, session, true);
    PutBytes(out, 3, k);
    return out;
}

std::string SendCommand(std::uint64_t session, Command command, std::int32_t id) {
    // SessionCommand: type = 1, id = 2.
    std::string c;
    PutUint(c, 1, static_cast<std::uint32_t>(command));
    if (id >= 0) PutUint(c, 2, static_cast<std::uint32_t>(id));
    // Turning the IME on also selects hiragana (composition_mode = 3).
    if (command == Command::kTurnOnIme) PutUint(c, 3, 1);
    std::string out = Input(5, session, true);
    PutBytes(out, 4, c);
    return out;
}

bool ParseOutput(std::string_view bytes, Output* out) {
    *out = Output{};
    // Output: id = 1, consumed = 3, result = 4 {value = 2}, preedit = 5, candidate_window = 6,
    // error_code = 11.
    return ReadMessage(bytes, [&](Reader& r, std::uint32_t f, std::uint32_t t, bool* handled) {
        std::string_view sub;
        switch (f) {
            case 1: return *handled = true, ReadUint(r, t, &out->id);
            case 3: return *handled = true, ReadUint(r, t, &out->consumed);
            case 11: {
                std::uint32_t code = 0;
                *handled = true;
                if (!ReadUint(r, t, &code)) return false;
                out->failed = code != 0;
                return true;
            }
            case 4:
                *handled = true;
                if (t != kLength || !r.Bytes(&sub)) return false;
                out->has_result = true;
                return ReadMessage(sub, [&](Reader& rr, std::uint32_t ff, std::uint32_t tt,
                                            bool* hh) {
                    if (ff == 2) return *hh = true, ReadString(rr, tt, &out->result);
                    return true;
                });
            case 5:
                *handled = true;
                return t == kLength && r.Bytes(&sub) && ParsePreedit(sub, out);
            case 6:
                *handled = true;
                return t == kLength && r.Bytes(&sub) && ParseCandidateWindow(sub, out);
        }
        return true;
    });
}

std::string Output::PreeditText() const {
    std::string text;
    for (const Segment& s : segments) text += s.value;
    return text;
}

bool Output::Converting() const {
    for (const Segment& s : segments) {
        if (s.annotation == Segment::kHighlight) return true;
    }
    return false;
}

std::string ToUtf8(std::u16string_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        std::uint32_t c = text[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size()) {
            c = 0x10000 + ((c - 0xD800) << 10) + (text[++i] - 0xDC00);
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::u16string FromUtf8(std::string_view text) {
    std::u16string out;
    for (size_t i = 0; i < text.size();) {
        const auto b = static_cast<std::uint8_t>(text[i]);
        std::uint32_t c;
        size_t n;
        if (b < 0x80) { c = b; n = 1; }
        else if ((b >> 5) == 6) { c = b & 0x1F; n = 2; }
        else if ((b >> 4) == 14) { c = b & 0x0F; n = 3; }
        else { c = b & 0x07; n = 4; }
        if (i + n > text.size()) break;
        for (size_t k = 1; k < n; ++k) c = (c << 6) | (static_cast<std::uint8_t>(text[i + k]) & 0x3F);
        i += n;
        if (c >= 0x10000) {
            c -= 0x10000;
            out += static_cast<char16_t>(0xD800 + (c >> 10));
            out += static_cast<char16_t>(0xDC00 + (c & 0x3FF));
        } else {
            out += static_cast<char16_t>(c);
        }
    }
    return out;
}

}  // namespace fkeyime::mozc
