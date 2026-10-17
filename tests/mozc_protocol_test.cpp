#include "mozc_protocol.h"

#include <string>

#include "check.h"

using namespace fkeyime::mozc;

namespace {

std::string Bytes(std::initializer_list<int> b) {
    std::string s;
    for (int c : b) s += static_cast<char>(c);
    return s;
}

// A varint-prefixed field with a string payload.
std::string Field(int tag, const std::string& payload) {
    std::string s;
    s += static_cast<char>(tag);
    s += static_cast<char>(payload.size());
    return s + payload;
}

}  // namespace

TEST_CASE("mozc: requests are encoded as commands.proto Input") {
    CHECK(CreateSession() == Bytes({0x08, 0x01}));
    CHECK(DeleteSession(5) == Bytes({0x08, 0x02, 0x10, 0x05}));
    KeyEvent space;
    space.special = SpecialKey::kSpace;
    // type=SEND_KEY, id=300 (varint ac 02), key={special_key=4}
    CHECK(SendKey(300, space) == Bytes({0x08, 0x03, 0x10, 0xAC, 0x02, 0x1A, 0x02, 0x18, 0x04}));
    KeyEvent text;
    text.key_string = "\xE3\x81\x82";  // あ
    text.as_is = true;
    text.modifiers = {kModShift};
    CHECK(SendKey(1, text) == Bytes({0x08, 0x03, 0x10, 0x01, 0x1A, 0x09, 0x20, 0x04, 0x2A, 0x03,
                                     0xE3, 0x81, 0x82, 0x30, 0x01}));
    CHECK(SendCommand(7, Command::kSubmit) ==
          Bytes({0x08, 0x05, 0x10, 0x07, 0x22, 0x02, 0x08, 0x02}));
}

TEST_CASE("mozc: output with result, preedit segments and candidates") {
    // preedit { cursor=2, group Segment { annotation=2, value="AB" }, group Segment {
    //   annotation=1, value="c" } }
    const std::string seg1 = Bytes({0x13, 0x18, 0x02}) + Field(0x22, "AB") + Bytes({0x14});
    const std::string seg2 = Bytes({0x13, 0x18, 0x01}) + Field(0x22, "c") + Bytes({0x14});
    const std::string preedit = Bytes({0x08, 0x02}) + seg1 + seg2;
    // candidate_window { focused_index=1, size=12, group Candidate { index=1, value="X",
    //   annotation { shortcut="2" } }, position=0, category=0 }
    const std::string cand = Bytes({0x1B, 0x20, 0x01}) + Field(0x2A, "X") +
                             Field(0x3A, Field(0x22, "2")) + Bytes({0x1C});
    const std::string window = Bytes({0x08, 0x01, 0x10, 0x0C}) + cand + Bytes({0x30, 0x00, 0x58, 0x00});
    const std::string out = Bytes({0x08, 0x2A, 0x18, 0x01}) + Field(0x22, Bytes({0x08, 0x01}) + Field(0x12, "done")) +
                            Field(0x2A, preedit) + Field(0x32, window) +
                            Bytes({0x40, 0x01});  // an unknown field (8) to skip
    Output o;
    CHECK(ParseOutput(out, &o));
    CHECK(o.id == 42);
    CHECK(o.consumed);
    CHECK(o.has_result && o.result == "done");
    CHECK(o.segments.size() == 2);
    CHECK(o.segments[0].annotation == Segment::kHighlight && o.segments[0].value == "AB");
    CHECK(o.PreeditText() == "ABc");
    CHECK(o.cursor == 2);
    CHECK(o.Converting());
    CHECK(o.ShowsCandidates());
    CHECK(o.size == 12 && o.has_focused && o.focused_index == 1);
    CHECK(o.candidates.size() == 1 && o.candidates[0].value == "X" && o.candidates[0].shortcut == "2");
}

TEST_CASE("mozc: suggestions while composing don't count as converting") {
    const std::string seg = Bytes({0x13, 0x18, 0x01}) + Field(0x22, "a") + Bytes({0x14});
    const std::string cand = Bytes({0x1B, 0x20, 0x00}) + Field(0x2A, "A") + Bytes({0x1C});
    const std::string window = Bytes({0x10, 0x01}) + cand + Bytes({0x58, 0x02});  // SUGGESTION
    const std::string out = Field(0x2A, Bytes({0x08, 0x01}) + seg) + Field(0x32, window);
    Output o;
    CHECK(ParseOutput(out, &o));
    CHECK(!o.Converting());
    CHECK(!o.ShowsCandidates());
}

TEST_CASE("mozc: error code and malformed input") {
    Output o;
    CHECK(ParseOutput(Bytes({0x58, 0x01}), &o));
    CHECK(o.failed);
    CHECK(!ParseOutput(Bytes({0x2A, 0x05, 0x08}), &o));  // truncated
}

TEST_CASE("mozc: UTF-8 conversion round trip") {
    const std::u16string s = u"a¥日\U0001F600";
    CHECK(FromUtf8(ToUtf8(s)) == s);
    CHECK(ToUtf8(u"あ") == "\xE3\x81\x82");
}
