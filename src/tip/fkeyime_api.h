// FKeyIME public API, version 1 (docs/api.md).
//
// Exported by fkeyime.dll (x64 and x86). Find the DLL through the permanent CLSID:
//   HKLM\SOFTWARE\Classes\CLSID\{66749D2A-1770-43D3-A269-A051B5BC1AAA}\InprocServer32
// 32-bit callers get the x86 DLL from the same key through registry redirection.
//
// The functions only read the session's state table; loading the DLL this way doesn't activate
// the IME, and callers may load and unload it per call.
#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FKEYIME_API_VERSION 1

// Return values of the query functions.
#define FKEYIME_NOT_ACTIVE (-1)   // FKeyIME isn't the active input method there (or unknown)
#define FKEYIME_UNAVAILABLE (-2)  // no state table: no FKeyIME instance has run in this session
#define FKEYIME_MODE_ENGLISH 0
#define FKEYIME_MODE_KOREAN 1
#define FKEYIME_MODE_JAPANESE 2

// FKEYIME_API_VERSION.
int WINAPI FKeyImeApiVersion(void);

// FKeyIME's mode on the thread that owns `window`, or a negative value above. `window` may be
// nullptr for the window with keyboard focus in the foreground.
int WINAPI FKeyImeQueryWindow(HWND window);

// The same for a thread id.
int WINAPI FKeyImeQueryThread(DWORD thread_id);

#ifdef __cplusplus
}
#endif
