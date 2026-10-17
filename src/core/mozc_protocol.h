// The part of Mozc's session protocol FKeyIME uses (Option B, docs/spec.md section 9), as
// protobuf wire format written and read by hand. Field numbers are from protocol/commands.proto
// and protocol/candidate_window.proto at the pinned Mozc commit (third_party/mozc.commit). Only
// the fields listed here are read; everything else in an Output is skipped.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fkeyime::mozc {

// commands.proto KeyEvent::SpecialKey (subset).
enum class SpecialKey : std::uint32_t {
    kNone = 0,
    kSpace = 4,
    kEnter = 5,
    kLeft = 6,
    kRight = 7,
    kUp = 8,
    kDown = 9,
    kEscape = 10,
    kDelete = 11,
    kBackspace = 12,
    kHome = 16,
    kEnd = 17,
    kTab = 18,
    kF6 = 24,
    kF7 = 25,
    kF8 = 26,
    kF9 = 27,
    kF10 = 28,
    kPageUp = 31,
    kPageDown = 32,
};

// commands.proto KeyEvent::ModifierKey (subset).
inline constexpr std::uint32_t kModCtrl = 1;
inline constexpr std::uint32_t kModAlt = 2;
inline constexpr std::uint32_t kModShift = 4;

// commands.proto SessionCommand::CommandType (subset).
enum class Command : std::uint32_t {
    kRevert = 1,
    kSubmit = 2,
    kSelectCandidate = 3,
    kTurnOnIme = 22,
};

struct KeyEvent {
    std::uint32_t key_code = 0;  // printable ASCII; 0 = none
    SpecialKey special = SpecialKey::kNone;
    std::vector<std::uint32_t> modifiers;
    std::string key_string;  // UTF-8
    bool as_is = false;      // InputStyle AS_IS: insert key_string as it is
};

// Serialized commands::Input messages.
std::string CreateSession();
std::string DeleteSession(std::uint64_t session);
std::string SendKey(std::uint64_t session, const KeyEvent& key);
std::string SendCommand(std::uint64_t session, Command command, std::int32_t id = -1);

struct Segment {
    enum Annotation : std::uint32_t { kNone = 0, kUnderline = 1, kHighlight = 2 };
    std::uint32_t annotation = kNone;
    std::string value;  // UTF-8
};

struct Candidate {
    std::uint32_t index = 0;
    std::int32_t id = 0;
    std::string value;     // UTF-8
    std::string shortcut;  // the digit to press, if any
    std::string description;
};

struct Output {
    std::uint64_t id = 0;
    bool consumed = false;
    bool failed = false;  // error_code SESSION_FAILURE (unknown session, e.g. server restarted)
    bool has_result = false;
    std::string result;  // UTF-8, committed text
    bool has_preedit = false;
    std::vector<Segment> segments;
    std::uint32_t cursor = 0;
    bool has_candidates = false;
    bool has_focused = false;
    std::uint32_t focused_index = 0;
    std::uint32_t size = 0;  // all candidates, not just this page
    // candidate_window.proto Category: 0 conversion, 1 prediction, 2 suggestion (shown while
    // still composing), 3 transliteration, 4 usage.
    std::uint32_t category = 0;
    std::vector<Candidate> candidates;  // this page

    std::string PreeditText() const;
    // True when some segment is being converted, as opposed to plain composing (which may come
    // with suggestion candidates).
    bool Converting() const;
    // True when the candidates are a conversion's choices (shown in the candidate window).
    bool ShowsCandidates() const { return has_candidates && category != 2 && !candidates.empty(); }
};

// Parses a serialized commands::Output. False on malformed input.
bool ParseOutput(std::string_view bytes, Output* out);

// UTF-16 <-> UTF-8 for the TIP (TSF is UTF-16, protobuf strings are UTF-8).
std::string ToUtf8(std::u16string_view text);
std::u16string FromUtf8(std::string_view text);

}  // namespace fkeyime::mozc
