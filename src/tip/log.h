// Diagnostic log shared by every process that loads the TIP (milestone M0 spike).
//
// One line per event, appended to %ProgramData%\fkeyime\fkeyime.log when that folder exists
// (tools/spike.ps1 creates it with write access for AppContainer apps), otherwise nothing is
// written. Timestamps are QueryPerformanceCounter microseconds, which are comparable across
// processes, so latencies between processes can be computed from the log.
#pragma once

namespace fkeyime::log {

// printf-style; the line gets "<qpc_us> <pid> <exe> " prepended.
void Write(const wchar_t* format, ...);

// Current QueryPerformanceCounter time in microseconds.
long long NowUs();

}  // namespace fkeyime::log
