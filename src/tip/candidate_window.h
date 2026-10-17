// The kanji conversion candidate list: a small topmost popup next to the text being converted.
// Never takes focus. Drawn from Mozc's candidate window data (one page at a time).
#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "mozc_protocol.h"

namespace fkeyime {

class CandidateWindow {
public:
    CandidateWindow() = default;
    ~CandidateWindow();
    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;

    // Shows the candidates of `out` below `anchor` (screen coordinates of the segment being
    // converted), or above it when there's no room below.
    void Show(const mozc::Output& out, const RECT& anchor);
    void Hide();
    bool Visible() const { return hwnd_ && IsWindowVisible(hwnd_); }

private:
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    void Paint(HDC dc);
    void UpdateFont();

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    UINT font_dpi_ = 0;
    struct Row {
        std::wstring label;  // "1"
        std::wstring text;
        bool focused = false;
    };
    std::vector<Row> rows_;
    std::wstring footer_;  // "2/36"
    int row_height_ = 0;
    int label_width_ = 0;
    int padding_ = 0;
};

}  // namespace fkeyime
