// TSF edit sessions: the only way a TIP may change the document.
#pragma once

#include <msctf.h>

#include <functional>
#include <string_view>

namespace fkeyime {

// Runs `body` with a read-write edit cookie. Synchronously when TSF allows it (always the case
// inside key event handlers), so text lands before the key that caused it is passed on;
// otherwise as soon as TSF can.
HRESULT RunEditSession(ITfContext* context, TfClientId client_id,
                       std::function<HRESULT(TfEditCookie)> body);

// Inserts `text` at the selection (replacing it) and puts the caret after it. Returns the range
// of the inserted text in `inserted` when that's not nullptr.
HRESULT InsertAtSelection(TfEditCookie ec, ITfContext* context, std::wstring_view text,
                          ITfRange** inserted = nullptr);

// Puts a collapsed selection (the caret) at the end of `range`.
HRESULT CaretToEnd(TfEditCookie ec, ITfContext* context, ITfRange* range);

}  // namespace fkeyime
