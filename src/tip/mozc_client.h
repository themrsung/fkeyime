// Talks to FKeyIME's bundled Mozc conversion server (a renamed mozc_server, docs/spec.md
// section 9) over its named pipe: one request and one response per connection, like Mozc's own
// IPC client (ipc/win32_ipc.cc). Starts the server when it isn't running.
#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

#include "mozc_protocol.h"

namespace fkeyime {

class MozcClient {
public:
    // `server` is the full path of fkeyime_mozc_server.exe.
    explicit MozcClient(std::wstring server) : server_(std::move(server)) {}
    ~MozcClient();
    MozcClient(const MozcClient&) = delete;
    MozcClient& operator=(const MozcClient&) = delete;

    // Each returns false when the server can't be reached or answers with an error; the
    // session is recreated once if the server lost it (restarted).
    bool SendKey(const mozc::KeyEvent& key, mozc::Output* out);
    bool SendCommand(mozc::Command command, mozc::Output* out, std::int32_t id = -1);

    // Ends the session on the server (on deactivation).
    void Close();

    // The pipe name, e.g. \\.\pipe\fkeyime-mozc.<key>.session. Exposed for tools.
    static std::wstring PipeName();

private:
    bool EnsureSession();
    bool Call(const std::string& request, std::string* response);
    bool Exchange(const std::string& request, std::string* response);  // one connection
    bool StartServer();
    template <typename MakeRequest>
    bool Session(MakeRequest make, mozc::Output* out);

    std::wstring server_;
    std::uint64_t session_ = 0;
};

// Where the TIP DLL expects the server: <dll dir>\..\mozc\fkeyime_mozc_server.exe.
std::wstring DefaultMozcServerPath(HINSTANCE dll);

}  // namespace fkeyime
