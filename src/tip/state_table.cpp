#include "state_table.h"

#include <sddl.h>

namespace fkeyime::state {

bool ThreadAlive(DWORD thread_id, DWORD process_id) {
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, thread_id);
    if (!thread) {
        // Access denied means it exists (e.g. an elevated or protected process): trust the slot.
        return GetLastError() == ERROR_ACCESS_DENIED;
    }
    DWORD code = 0;
    const bool alive = GetExitCodeThread(thread, &code) && code == STILL_ACTIVE &&
                       GetProcessIdOfThread(thread) == process_id;
    CloseHandle(thread);
    return alive;
}

void Init(Table* table) {
    if (table->magic == kMagic) return;
    table->version = kVersion;
    table->slot_count = kSlotCount;
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&table->magic), static_cast<LONG>(kMagic));
}

namespace {

void Fill(Slot& slot, DWORD process_id, LONG mode, LONGLONG now_us) {
    slot.process_id = process_id;
    slot.updated_us = now_us;
    InterlockedExchange(&slot.mode, mode);
}

}  // namespace

bool Claim(Table* table, DWORD thread_id, DWORD process_id, LONG mode, LONGLONG now_us,
           AliveFn alive) {
    for (Slot& slot : table->slots) {
        if (slot.thread_id == static_cast<LONG>(thread_id)) {
            Fill(slot, process_id, mode, now_us);
            return true;
        }
    }
    // Two passes: free slots first, then slots whose thread died without releasing them.
    for (int pass = 0; pass < 2; ++pass) {
        for (Slot& slot : table->slots) {
            const LONG owner = slot.thread_id;
            if (owner && (pass == 0 || alive(static_cast<DWORD>(owner), slot.process_id))) {
                continue;
            }
            if (InterlockedCompareExchange(&slot.thread_id, static_cast<LONG>(thread_id), owner) ==
                owner) {
                Fill(slot, process_id, mode, now_us);
                return true;
            }
        }
    }
    return false;
}

void SetMode(Table* table, DWORD thread_id, LONG mode, LONGLONG now_us) {
    for (Slot& slot : table->slots) {
        if (slot.thread_id == static_cast<LONG>(thread_id)) {
            slot.updated_us = now_us;
            InterlockedExchange(&slot.mode, mode);
            return;
        }
    }
}

void Release(Table* table, DWORD thread_id) {
    for (Slot& slot : table->slots) {
        InterlockedCompareExchange(&slot.thread_id, 0, static_cast<LONG>(thread_id));
    }
}

LONG Find(const Table* table, DWORD thread_id, AliveFn alive) {
    if (!thread_id || table->magic != kMagic) return -1;
    for (const Slot& slot : table->slots) {
        if (slot.thread_id == static_cast<LONG>(thread_id)) {
            return alive(thread_id, slot.process_id) ? slot.mode : -1;
        }
    }
    return -1;
}

LONG SessionFlags(const Table* table) {
    return table->magic == kMagic ? table->session_flags : 0;
}

void SetSessionFlags(Table* table, LONG flags) { InterlockedExchange(&table->session_flags, flags); }

Table* Shared() {
    static Table* table = [] () -> Table* {
        // Everyone and ALL APPLICATION PACKAGES may read and write; low-integrity writers allowed.
        PSECURITY_DESCRIPTOR sd = nullptr;
        ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;GA;;;WD)(A;;GA;;;AC)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &sd, nullptr);
        SECURITY_ATTRIBUTES sa = {sizeof(sa), sd, FALSE};
        HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, sd ? &sa : nullptr,
                                            PAGE_READWRITE, 0, sizeof(Table), kMappingName);
        if (sd) LocalFree(sd);
        if (!mapping) return nullptr;
        auto* view = static_cast<Table*>(
            MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(Table)));
        // The handle stays open for the life of the process, so the table outlives each TIP.
        if (view) Init(view);
        return view;
    }();
    return table;
}

}  // namespace fkeyime::state
