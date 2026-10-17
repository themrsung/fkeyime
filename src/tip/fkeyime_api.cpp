#include "fkeyime_api.h"

#include "state_table.h"

namespace {

DWORD FocusThread() {
    GUITHREADINFO info = {sizeof(info)};
    if (!GetGUIThreadInfo(0, &info)) return 0;  // 0: the foreground thread
    HWND window = info.hwndFocus ? info.hwndFocus : info.hwndActive;
    return window ? GetWindowThreadProcessId(window, nullptr) : 0;
}

}  // namespace

extern "C" int WINAPI FKeyImeApiVersion(void) { return FKEYIME_API_VERSION; }

extern "C" int WINAPI FKeyImeQueryThread(DWORD thread_id) {
    using namespace fkeyime::state;
    // Open read-only per call: callers such as AutoHotkey load and unload the DLL every time, so
    // nothing may stay mapped.
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, kMappingName);
    if (!mapping) return FKEYIME_UNAVAILABLE;
    int result = FKEYIME_UNAVAILABLE;
    if (const auto* table =
            static_cast<const Table*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(Table)))) {
        result = Find(table, thread_id);
        UnmapViewOfFile(table);
    }
    CloseHandle(mapping);
    return result;
}

extern "C" int WINAPI FKeyImeQueryWindow(HWND window) {
    const DWORD thread_id = window ? GetWindowThreadProcessId(window, nullptr) : FocusThread();
    return thread_id ? FKeyImeQueryThread(thread_id) : FKEYIME_NOT_ACTIVE;
}
