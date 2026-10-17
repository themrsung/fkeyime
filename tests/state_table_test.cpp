#include "state_table.h"

#include <memory>

#include "check.h"

using namespace fkeyime::state;

// The v1 ABI layout other programs rely on (docs/api.md).
static_assert(sizeof(Table) == 16 + 256 * 32);

namespace {

bool AllAlive(DWORD, DWORD) { return true; }
bool NoneAlive(DWORD, DWORD) { return false; }

std::unique_ptr<Table> NewTable() {
    auto table = std::make_unique<Table>();
    Init(table.get());
    return table;
}

}  // namespace

TEST_CASE("state: a claimed thread is found with its mode, a released one isn't") {
    auto t = NewTable();
    CHECK(Find(t.get(), 100, AllAlive) == -1);
    CHECK(Claim(t.get(), 100, 7, 1, 0, AllAlive));
    CHECK(Find(t.get(), 100, AllAlive) == 1);
    Release(t.get(), 100);
    CHECK(Find(t.get(), 100, AllAlive) == -1);
}

TEST_CASE("state: SetMode updates only that thread") {
    auto t = NewTable();
    Claim(t.get(), 100, 7, 0, 0, AllAlive);
    Claim(t.get(), 200, 7, 0, 0, AllAlive);
    SetMode(t.get(), 200, 2, 1);
    CHECK(Find(t.get(), 100, AllAlive) == 0);
    CHECK(Find(t.get(), 200, AllAlive) == 2);
}

TEST_CASE("state: claiming twice re-uses the slot") {
    auto t = NewTable();
    Claim(t.get(), 100, 7, 0, 0, AllAlive);
    Claim(t.get(), 100, 7, 2, 0, AllAlive);
    int slots = 0;
    for (const Slot& s : t->slots) slots += s.thread_id == 100;
    CHECK(slots == 1);
    CHECK(Find(t.get(), 100, AllAlive) == 2);
}

TEST_CASE("state: dead threads are ignored and their slots reclaimed when full") {
    auto t = NewTable();
    for (DWORD i = 1; i <= kSlotCount; ++i) CHECK(Claim(t.get(), i, 7, 0, 0, AllAlive));
    CHECK(!Claim(t.get(), 9999, 7, 0, 0, AllAlive));   // full of live threads
    CHECK(Find(t.get(), 5, NoneAlive) == -1);          // crashed owner: not active
    CHECK(Claim(t.get(), 9999, 7, 1, 0, NoneAlive));   // reclaims a dead slot
    CHECK(Find(t.get(), 9999, AllAlive) == 1);
}

TEST_CASE("state: an uninitialized table and thread 0 report not active") {
    Table raw = {};
    CHECK(Find(&raw, 100, AllAlive) == -1);
    auto t = NewTable();
    CHECK(Find(t.get(), 0, AllAlive) == -1);
}

TEST_CASE("state: header is version 1 with 256 slots") {
    auto t = NewTable();
    CHECK(t->magic == kMagic);
    CHECK(t->version == 1);
    CHECK(t->slot_count == 256);
}
