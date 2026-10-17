#Requires AutoHotkey v2.0
#SingleInstance Force
; hhkb-ekj-trilingual (Windows): one-way language keys sent by the HHKB Studio, keymap Profile 4.
;   F18 -> Korean (한)   F19 -> English (Japanese IME off)   F20 -> Japanese hiragana, kana typing
;   F17 -> 한자 in Korean, nothing elsewhere   Pause (Fn1+P) -> Media Play/Pause
; FKeyIME-aware (https://github.com/themrsung/fkeyime docs/api.md). Win+Space picks who handles
; the language keys:
;   - FKeyIME is the session's input method: this script steps aside. A window that isn't on
;     FKeyIME yet is moved to it, and the key is sent again for FKeyIME to handle.
;   - A Microsoft IME is the session's input method: the keys work as in v1.5.2.
; Without FKeyIME installed, this behaves exactly like the hhkb-ekj-trilingual v1.5.2 script.

HKL_KO := 0x04120412, LANG_KO := 0x0412
HKL_JA := 0x04110411, LANG_JA := 0x0411
LANG_EN := 0x0409

; IME conversion-mode bits (imm.h)
CM_NATIVE := 0x01, CM_KATAKANA := 0x02, CM_FULLSHAPE := 0x08, CM_ROMAN := 0x10

; Targets: language, IME open status, conversion mode to set, and the bits that must match.
KOREAN   := {hkl: HKL_KO, lang: LANG_KO, open: 1, cmode: CM_NATIVE, mask: CM_NATIVE}
ENGLISH  := {hkl: HKL_JA, lang: LANG_JA, open: 0}
JAPANESE := {hkl: HKL_JA, lang: LANG_JA, open: 1, cmode: CM_NATIVE | CM_FULLSHAPE
           , mask: CM_NATIVE | CM_KATAKANA | CM_FULLSHAPE | CM_ROMAN}

FKEYIME_CLSID := "{66749D2A-1770-43D3-A269-A051B5BC1AAA}"
FKEYIME_EN := "{8F61A2C0-75BC-4042-B739-CC7D63B542E5}"
FKEYIME_KO := "{28C201BF-390E-482F-8259-CD26E05844A9}"
FKEYIME_JA := "{1AC13F3E-07AC-4B03-A606-967BD48BFD99}"
; The Microsoft IMEs these targets mean (language, CLSID, profile), for UseMicrosoftIme().
KOREAN.tip   := [LANG_KO, "{A028AE76-01B1-46C2-99C4-ACD9858AE02F}", "{B5FE1F02-D5F2-4445-9C03-C568F23C99A1}"]
ENGLISH.tip  := [LANG_JA, "{03B5835F-F03C-411B-9CE2-AA23E1171E36}", "{A76C93D9-5523-4E90-AAFA-4DB112F9AC76}"]
JAPANESE.tip := ENGLISH.tip

; FKeyIME chosen, but this window isn't on it yet: move it there and let FKeyIME have the key.
#HotIf !FKeyImeActive() && FKeyImeSession()
F16::ToFKeyIme("F16", LANG_JA, FKEYIME_JA)
F18::ToFKeyIme("F18", LANG_KO, FKEYIME_KO)
F19::ToFKeyIme("F19", LANG_EN, FKEYIME_EN)
F20::ToFKeyIme("F20", LANG_JA, FKEYIME_JA)
; A Microsoft IME chosen: v1.5.2's switching.
#HotIf !FKeyImeActive() && !FKeyImeSession()
F18::SwitchTo(KOREAN)
F19::SwitchTo(ENGLISH)
F20::SwitchTo(JAPANESE)
F17::ContextAction()
#HotIf
Pause::Send("{Media_Play_Pause}")

; True while FKeyIME is the input method of the focused window. The DLL is found through FKeyIME's
; permanent CLSID (32-bit AutoHotkey gets the x86 DLL through registry redirection) and loaded
; only for the call, so it never stays locked.
FKeyImeActive() {
    try dll := RegRead("HKLM\SOFTWARE\Classes\CLSID\" FKEYIME_CLSID "\InprocServer32")
    catch
        return false                            ; FKeyIME isn't installed
    try return DllCall(dll "\FKeyImeQueryWindow", "ptr", FocusHwnd(), "int") >= 0
    catch
        return false                            ; DLL missing or without the API: act as before
}

; ITfInputProcessorProfileMgr, for reading and choosing the session's input method.
ProfileMgr() {
    static mgr := ComObject("{33C53A50-F456-4884-B049-85FD643ECFED}", "{71C6E74C-0F28-11D8-A82A-00065B84435C}")
    return mgr
}

Guid(text) {
    g := Buffer(16)
    DllCall("ole32\CLSIDFromString", "str", text, "ptr", g)
    return g
}

; True when the session's input method (what Win+Space last chose) is FKeyIME. This thread has no
; text input of its own, so it sees the session's choice (measured: it follows session changes).
FKeyImeSession() {
    static keyboard := Guid("{34745C63-B2F0-4784-8B67-5E12C8701A31}")  ; GUID_TFCAT_TIP_KEYBOARD
    static fkeyime := Guid(FKEYIME_CLSID)
    profile := Buffer(96, 0)                    ; TF_INPUTPROCESSORPROFILE; clsid at offset 8
    try ComCall(10, ProfileMgr(), "ptr", keyboard, "ptr", profile)  ; GetActiveProfile
    catch
        return false
    return DllCall("ntdll\RtlCompareMemory", "ptr", profile.Ptr + 8, "ptr", fkeyime, "uptr", 16
                 , "uptr") = 16
}

; Selects one of FKeyIME's profiles for the whole session (which moves this window too), waits for
; FKeyIME to be active here, and sends the key again so FKeyIME applies its own rules to it.
ToFKeyIme(key, lang, profile) {
    try ComCall(3, ProfileMgr(), "uint", 1, "ushort", lang, "ptr", Guid(FKEYIME_CLSID)  ; ActivateProfile
              , "ptr", Guid(profile), "ptr", 0, "uint", 0x20000004)  ; FORSESSION | DONTCARECURRENTINPUTLANGUAGE
    deadline := A_TickCount + 300
    while !FKeyImeActive() && A_TickCount < deadline
        Sleep(10)
    if FKeyImeActive()
        SendEvent("{" key "}")
}

; A language change picks that language's last-used input method, which can be FKeyIME's. Select
; the Microsoft IME explicitly instead (ITfInputProcessorProfileMgr::ActivateProfile, whole session).
UseMicrosoftIme(t) {
    try ComCall(3, ProfileMgr(), "uint", 1, "ushort", t.tip[1], "ptr", Guid(t.tip[2])
              , "ptr", Guid(t.tip[3]), "ptr", 0, "uint", 0x20000000)  ; FORSESSION
}

SwitchTo(t) {
    hwnd := FocusHwnd()
    if !hwnd
        return
    ; Always apply, even if t already looks active: a stale state must not leave the key dead.
    Apply(hwnd, t)
    ; Verify once, and retry only if the switch didn't take.
    SetTimer(() => (IsAt(hwnd, t) || Apply(hwnd, t)), -150)
}

Apply(hwnd, t) {
    DllCall("PostMessageW", "ptr", hwnd, "uint", 0x50, "ptr", 0, "ptr", t.hkl)  ; WM_INPUTLANGCHANGEREQUEST
    deadline := A_TickCount + 150
    while LangOf(hwnd) != t.lang && A_TickCount < deadline
        Sleep(10)
    if FKeyImeActive() {                        ; landed in FKeyIME's profile for this language
        UseMicrosoftIme(t)
        deadline := A_TickCount + 150
        while FKeyImeActive() && A_TickCount < deadline
            Sleep(10)
    }
    ImeCtl(hwnd, 0x006, t.open)                 ; IMC_SETOPENSTATUS
    if t.open
        ImeCtl(hwnd, 0x002, t.cmode)            ; IMC_SETCONVERSIONMODE
}

IsAt(hwnd, t) {
    if FKeyImeActive() || LangOf(hwnd) != t.lang || ImeCtl(hwnd, 0x005) != t.open   ; IMC_GETOPENSTATUS
        return false
    return !t.open || (ImeCtl(hwnd, 0x001) & t.mask) = t.cmode    ; IMC_GETCONVERSIONMODE
}

ContextAction() {
    hwnd := FocusHwnd()
    if hwnd && LangOf(hwnd) = LANG_KO
        Send("{vk19}")                          ; VK_HANJA: Korean IME's 한자 conversion
    ; Other languages: nothing yet.
}

; The window that has keyboard focus (a child control, if the app uses them).
FocusHwnd() {
    if !(active := WinExist("A"))
        return 0
    tid := DllCall("GetWindowThreadProcessId", "ptr", active, "ptr", 0, "uint")
    gti := Buffer(A_PtrSize = 8 ? 72 : 48, 0)   ; GUITHREADINFO
    NumPut("uint", gti.Size, gti, 0)
    if DllCall("GetGUIThreadInfo", "uint", tid, "ptr", gti)
        && (focus := NumGet(gti, A_PtrSize = 8 ? 16 : 12, "ptr"))
        return focus
    return active
}

LangOf(hwnd) {
    tid := DllCall("GetWindowThreadProcessId", "ptr", hwnd, "ptr", 0, "uint")
    return DllCall("GetKeyboardLayout", "uint", tid, "ptr") & 0xFFFF
}

ImeCtl(hwnd, cmd, value := 0) {
    if !(ime := DllCall("imm32\ImmGetDefaultIMEWnd", "ptr", hwnd, "ptr"))
        return -1
    result := 0
    DllCall("SendMessageTimeoutW", "ptr", ime, "uint", 0x283, "ptr", cmd, "ptr", value  ; WM_IME_CONTROL
          , "uint", 0x2, "uint", 200, "ptr*", &result)                                  ; SMTO_ABORTIFHUNG
    return result
}
