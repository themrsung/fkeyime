// The display attribute of composing text (an underline), so apps show what's still being typed.
// TSF asks the TIP for it through ITfDisplayAttributeProvider (text_service.h).
#pragma once

#include <msctf.h>

namespace fkeyime {

// A new enumerator over FKeyIME's display attributes (just kDisplayAttributeInput).
IEnumTfDisplayAttributeInfo* NewDisplayAttributeEnum();

// The info object for `guid`, or nullptr if it isn't FKeyIME's.
ITfDisplayAttributeInfo* NewDisplayAttributeInfo(REFGUID guid);

}  // namespace fkeyime
