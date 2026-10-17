// Where FKeyIME is active: a small table in session-wide shared memory.
//
// Every thread that has an FKeyIME profile active holds one slot (claimed in ActivateEx, released
// in Deactivate) with its current mode. Other programs read it through the public API in
// fkeyime_api.h, for example so the v1.5.2 AutoHotkey script can step aside while FKeyIME is
// active (docs/api.md). The layout below is version 1 of that ABI: only ever append fields.
#pragma once

#include <windows.h>

#include <cstdint>

namespace fkeyime::state {

inline constexpr wchar_t kMappingName[] = L"Local\\fkeyime.state.v1";
inline constexpr std::uint32_t kMagic = 0x4D494B46;  // "FKIM"
inline constexpr std::uint32_t kVersion = 1;
inline constexpr std::uint32_t kSlotCount = 256;

struct Slot {
    volatile LONG thread_id;  // 0 = free
    DWORD process_id;
    volatile LONG mode;       // 0 English, 1 Korean, 2 Japanese (fkeyime::Mode)
    DWORD reserved;
    LONGLONG updated_us;      // QueryPerformanceCounter microseconds of the last write
    LONGLONG reserved2;
};
static_assert(sizeof(Slot) == 32);

struct Table {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t slot_count;
    volatile LONG session_flags;  // kFlagRomaji: global Japanese input (was reserved, 0)
    Slot slots[kSlotCount];
};

inline constexpr LONG kFlagRomaji = 1;

// Whether a thread still exists and belongs to the process. Injected so tests can fake it.
using AliveFn = bool (*)(DWORD thread_id, DWORD process_id);
bool ThreadAlive(DWORD thread_id, DWORD process_id);

// Fills in the header of a zeroed table. Idempotent.
void Init(Table* table);

// Claims (or re-uses) the slot of `thread_id` and sets its mode. Reclaims slots of dead threads
// when the table is full. Returns false only when every slot belongs to a live thread.
bool Claim(Table* table, DWORD thread_id, DWORD process_id, LONG mode, LONGLONG now_us,
           AliveFn alive = ThreadAlive);
void SetMode(Table* table, DWORD thread_id, LONG mode, LONGLONG now_us);
void Release(Table* table, DWORD thread_id);

// The mode of `thread_id` if FKeyIME is active on it, otherwise -1. Ignores dead threads.
LONG Find(const Table* table, DWORD thread_id, AliveFn alive = ThreadAlive);

// Session-wide flags (kFlagRomaji), so every window agrees on kana vs romaji.
LONG SessionFlags(const Table* table);
void SetSessionFlags(Table* table, LONG flags);

// The session's table, created on first use with access for every process in the session,
// AppContainer and low-integrity ones included. nullptr if it can't be opened.
Table* Shared();

}  // namespace fkeyime::state
