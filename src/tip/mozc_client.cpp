#include "mozc_client.h"

#include <bcrypt.h>
#include <sddl.h>
#include <shlobj.h>

#include <memory>

namespace fkeyime {
namespace {

constexpr DWORD kCallTimeoutMs = 2000;      // never freeze the app on a stuck server
constexpr DWORD kServerStartTimeoutMs = 10000;

// The pipe key, as Mozc derives it on Windows (ipc/ipc_path_manager.cc CreateIPCKey): the SHA-1
// of the user's SID string, as lower-case hex. The server also writes it to
// %LOCALAPPDATA%Low\FKeyIME Mozc\session.ipc, which AppContainer apps can't read, so it's
// computed here instead.
std::wstring IpcKey() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    auto buffer = std::make_unique<BYTE[]>(size);
    std::wstring key;
    char* sid_string = nullptr;
    if (GetTokenInformation(token, TokenUser, buffer.get(), size, &size) &&
        ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buffer.get())->User.Sid,
                               &sid_string)) {
        BYTE digest[20] = {};
        if (BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA1_ALG_HANDLE, nullptr, 0,
                                      reinterpret_cast<PUCHAR>(sid_string),
                                      static_cast<ULONG>(strlen(sid_string)), digest,
                                      sizeof(digest)))) {
            constexpr wchar_t kHex[] = L"0123456789abcdef";
            for (BYTE b : digest) {
                key += kHex[b >> 4];
                key += kHex[b & 15];
            }
        }
        LocalFree(sid_string);
    }
    CloseHandle(token);
    return key;
}

}  // namespace

std::wstring MozcClient::PipeName() {
    static const std::wstring name = L"\\\\.\\pipe\\fkeyime-mozc." + IpcKey() + L".session";
    return name;
}

std::wstring DefaultMozcServerPath(HINSTANCE dll) {
    wchar_t path[MAX_PATH] = L"";
    GetModuleFileNameW(dll, path, MAX_PATH);
    std::wstring dir(path);
    dir.resize(dir.find_last_of(L'\\'));            // ...\x64
    dir.resize(dir.find_last_of(L'\\'));            // ...
    return dir + L"\\mozc\\fkeyime_mozc_server.exe";
}

MozcClient::~MozcClient() { Close(); }

void MozcClient::Close() {
    if (!session_) return;
    std::string response;
    Exchange(mozc::DeleteSession(session_), &response);  // don't start a server just for this
    session_ = 0;
}

bool MozcClient::StartServer() {
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    std::wstring command = L"\"" + server_ + L"\"";
    // Outlive the app that started it (some apps kill their job's processes on exit).
    BOOL ok = CreateProcessW(server_.c_str(), command.data(), nullptr, nullptr, FALSE,
                             DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB, nullptr, nullptr, &si,
                             &pi);
    if (!ok) {
        ok = CreateProcessW(server_.c_str(), command.data(), nullptr, nullptr, FALSE,
                            DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
    }
    if (!ok) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    const std::wstring pipe = PipeName();
    for (DWORD waited = 0; waited < kServerStartTimeoutMs; waited += 50) {
        if (WaitNamedPipeW(pipe.c_str(), 50) || GetLastError() == ERROR_SEM_TIMEOUT) return true;
        Sleep(50);
    }
    return false;
}

bool MozcClient::Exchange(const std::string& request, std::string* response) {
    const std::wstring pipe = PipeName();
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 2 && h == INVALID_HANDLE_VALUE; ++attempt) {
        h = CreateFileW(pipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                        nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            if (GetLastError() != ERROR_PIPE_BUSY) return false;
            WaitNamedPipeW(pipe.c_str(), kCallTimeoutMs);
        }
    }
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(h, &mode, nullptr, nullptr);

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    auto wait = [&](BOOL started, DWORD* bytes) -> bool {
        if (!started && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_MORE_DATA) {
            return false;
        }
        if (WaitForSingleObject(ov.hEvent, kCallTimeoutMs) != WAIT_OBJECT_0) {
            CancelIo(h);
            return false;
        }
        return GetOverlappedResult(h, &ov, bytes, FALSE) || GetLastError() == ERROR_MORE_DATA;
    };

    bool ok = false;
    DWORD n = 0;
    ResetEvent(ov.hEvent);
    if (wait(WriteFile(h, request.data(), static_cast<DWORD>(request.size()), nullptr, &ov), &n)) {
        response->clear();
        char buffer[8192];
        for (;;) {
            ResetEvent(ov.hEvent);
            const BOOL done = ReadFile(h, buffer, sizeof(buffer), nullptr, &ov);
            DWORD got = 0;
            if (!wait(done, &got)) break;
            const bool more = GetLastError() == ERROR_MORE_DATA;
            response->append(buffer, got);
            if (!more) {
                ok = true;
                break;
            }
            SetLastError(0);
        }
    }
    CloseHandle(ov.hEvent);
    CloseHandle(h);
    return ok;
}

bool MozcClient::Call(const std::string& request, std::string* response) {
    if (Exchange(request, response)) return true;
    return StartServer() && Exchange(request, response);
}

bool MozcClient::EnsureSession() {
    if (session_) return true;
    std::string response;
    mozc::Output out;
    if (!Call(mozc::CreateSession(), &response) || !mozc::ParseOutput(response, &out) ||
        out.failed || !out.id) {
        return false;
    }
    session_ = out.id;
    // New sessions start with the IME off (direct input): turn it on, in hiragana.
    if (!Call(mozc::SendCommand(session_, mozc::Command::kTurnOnIme), &response)) {
        session_ = 0;
        return false;
    }
    return true;
}

template <typename MakeRequest>
bool MozcClient::Session(MakeRequest make, mozc::Output* out) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!EnsureSession()) return false;
        std::string response;
        if (Call(make(session_), &response) && mozc::ParseOutput(response, out) && !out->failed) {
            return true;
        }
        session_ = 0;  // unknown session (server restarted): make a new one and retry once
    }
    return false;
}

bool MozcClient::SendKey(const mozc::KeyEvent& key, mozc::Output* out) {
    return Session([&](std::uint64_t s) { return mozc::SendKey(s, key); }, out);
}

bool MozcClient::SendCommand(mozc::Command command, mozc::Output* out, std::int32_t id) {
    return Session([&](std::uint64_t s) { return mozc::SendCommand(s, command, id); }, out);
}

}  // namespace fkeyime
