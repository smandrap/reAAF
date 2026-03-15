/*
 * Copyright (C) 2026 Federico Manuppella
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ProgressDialog.h"
#include "LogBuffer.h"
#include "resource.h"
#include "reaper_plugin_functions.h"
#include "wdltypes.h"

#include <cstdio>    // snprintf
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// External declarations (defined in main.cpp)
// ---------------------------------------------------------------------------

extern LogBuffer g_logBuffer;
extern REAPER_PLUGIN_HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// Module-scope state
// ---------------------------------------------------------------------------

// HWND of the open dialog, or nullptr when closed.
static HWND s_hwnd = nullptr;
// Logical read cursor: index into LogBuffer from which the next drain starts.
static size_t s_readPos = 0;
// Auto-scroll: true = scroll list to latest entry on each drain tick.
static bool s_autoScroll = true;

// Timer ID used for the 100 ms drain loop.
static constexpr UINT_PTR TIMER_DRAIN = 1;

// ---------------------------------------------------------------------------
// formatEntry — build the display string for one LogEntry.
// Severity prefix mapping: ERROR→[ERROR], WARN→[WARN], INFO→[INFO], CLIP→[CLIP].
// Clip entries with a non-empty clipName: "[CLIP] {clipName}: {text}"
// ---------------------------------------------------------------------------

static std::string formatEntry(const LogEntry &e) {
    auto prefix = "";
    switch (e.severity) {
        case LogEntry::ERROR: prefix = "[ERROR]";
            break;
        case LogEntry::WARN: prefix = "[WARN]";
            break;
        case LogEntry::INFO: prefix = "[INFO]";
            break;
        case LogEntry::CLIP: prefix = "[CLIP]";
            break;
    }
    if (e.severity == LogEntry::CLIP && !e.clipName.empty())
        return std::string(prefix) + " " + e.clipName + ": " + e.text;
    return std::string(prefix) + " " + e.text;
}

// ---------------------------------------------------------------------------
// drain — called on each TIMER_DRAIN tick.
// Fetches new entries from g_logBuffer and appends them to IDC_LOG_LIST.
// Updates the progress label with running clip count.
// ---------------------------------------------------------------------------

static void drain(HWND hwnd) {
    HWND hwndList = GetDlgItem(hwnd, IDC_LOG_LIST);
    std::vector<LogEntry> newEntries;
    s_readPos = g_logBuffer.drainNew(s_readPos, newEntries);

    for (const LogEntry &e: newEntries) {
        std::string line = formatEntry(e);
        SendMessage(hwndList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }

    if (s_autoScroll && !newEntries.empty()) {
        if (const int count = static_cast<int>(SendMessage(hwndList, LB_GETCOUNT, 0, 0)); count > 0)
            SendMessage(hwndList, LB_SETCURSEL, static_cast<WPARAM>(count - 1), 0);
    }

    // Update progress label with running count.
    // (Indeterminate mode — determinate "N of total" is Phase 3.)
    if (const int total = static_cast<int>(SendMessage(hwndList, LB_GETCOUNT, 0, 0)); total > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Processing... (%d entries so far)", total);
        SetDlgItemText(hwnd, IDC_PROGRESS_LABEL, buf);
    }
}

// ---------------------------------------------------------------------------
// Dialog procedure
// ---------------------------------------------------------------------------

static WDL_DLGRET CALLBACK progressDialogProc(HWND hwnd, const UINT msg, const WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Reset module state for this dialog session.
            s_readPos = 0;
            s_autoScroll = true;
            SetDlgItemText(hwnd, IDC_PROGRESS_LABEL, "Ready");
            // Start drain timer — 100 ms interval (standard REAPER plugin poll rate).
            SetTimer(hwnd, TIMER_DRAIN, 100, nullptr);
            return 1;
        }

        case WM_TIMER: {
            if (wParam == TIMER_DRAIN)
                drain(hwnd);
            return 0;
        }

        case WM_VSCROLL: {
            // Detect manual scroll-up gestures to pause auto-scroll.
            // WM_VSCROLL forwarding from LISTBOX to dialog is best-effort on macOS
            // (may not always fire — treated as graceful degradation per RESEARCH.md).
            const int scrollCode = LOWORD(wParam);
            HWND hwndList = GetDlgItem(hwnd, IDC_LOG_LIST);
            if (scrollCode == SB_THUMBTRACK ||
                scrollCode == SB_LINEUP ||
                scrollCode == SB_PAGEUP) {
                s_autoScroll = false;
            } else if (scrollCode == SB_LINEDOWN ||
                       scrollCode == SB_PAGEDOWN ||
                       scrollCode == SB_BOTTOM) {
                // Resume auto-scroll if the user has scrolled back to the last item.
                const int count = static_cast<int>(SendMessage(hwndList, LB_GETCOUNT, 0, 0));
                if (const int sel = static_cast<int>(SendMessage(hwndList, LB_GETCURSEL, 0, 0));
                    count > 0 && sel >= count - 1)
                    s_autoScroll = true;
            }
            return 0;
        }

        case WM_SIZE: {
            // Stretch IDC_LOG_LIST to fill client area; keep label at top; anchor
            // IDC_CLOSE_BTN to bottom-right. Uses SetWindowPos with SWP_NOZORDER.
            // (MoveWindow is not in swell-functions.h — use SetWindowPos per RESEARCH.md.)
            RECT cr;
            GetClientRect(hwnd, &cr);
            const int W = cr.right;
            const int H = cr.bottom;

            static constexpr int kMargin = 4;
            static constexpr int kLabelH = 12;
            static constexpr int kBtnW = 50;
            static constexpr int kBtnH = 16;

            // Progress label: top row, full width.
            SetWindowPos(GetDlgItem(hwnd, IDC_PROGRESS_LABEL), nullptr,
                         kMargin, kMargin,
                         W - 2 * kMargin, kLabelH,
                         SWP_NOZORDER | SWP_NOACTIVATE);

            // List: fills remaining space above button row.
            constexpr int listTop = kMargin + kLabelH + kMargin;
            int listH = H - listTop - kMargin - kBtnH - kMargin;
            if (listH < 10) listH = 10; // minimum guard
            SetWindowPos(GetDlgItem(hwnd, IDC_LOG_LIST), nullptr,
                         kMargin, listTop,
                         W - 2 * kMargin, listH,
                         SWP_NOZORDER | SWP_NOACTIVATE);

            // Close button: bottom-right.
            SetWindowPos(GetDlgItem(hwnd, IDC_CLOSE_BTN), nullptr,
                         W - kMargin - kBtnW, H - kMargin - kBtnH,
                         kBtnW, kBtnH,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_CLOSE_BTN)
                DestroyWindow(hwnd);
            return 0;
        }

        case WM_CLOSE: {
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            // KillTimer MUST be first in WM_DESTROY (macOS NSTimer requirement —
            // established pattern from STATE.md and RESEARCH.md).
            KillTimer(hwnd, TIMER_DRAIN);
            s_hwnd = nullptr;
            return 0;
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// ProgressDialog_Open
// ---------------------------------------------------------------------------

void ProgressDialog_Open() {
    if (s_hwnd) {
        // Dialog already open — bring to front.
        SetForegroundWindow(s_hwnd);
        return;
    }
    HWND parent = GetMainHwnd(); // requires REAPERAPI_WANT_GetMainHwnd
    s_hwnd = CreateDialog(g_hInst,
                          MAKEINTRESOURCE(IDD_AAF_PROGRESS),
                          parent,
                          progressDialogProc);
    if (s_hwnd)
        ShowWindow(s_hwnd, SW_SHOW);
}

// ---------------------------------------------------------------------------
// ProgressDialog_MarkComplete
// ---------------------------------------------------------------------------

void ProgressDialog_MarkComplete(const int clips, const int warnings, const int errors) {
    if (!s_hwnd) return;

    // Update progress label to final summary state.
    char labelBuf[128];
    snprintf(labelBuf, sizeof(labelBuf),
             "Import complete: %d clips, %d warnings, %d errors",
             clips, warnings, errors);
    SetDlgItemText(s_hwnd, IDC_PROGRESS_LABEL, labelBuf);

    // Append summary line to log list (WIN-05).
    HWND hwndList = GetDlgItem(s_hwnd, IDC_LOG_LIST);
    const std::string summaryLine = std::string("[INFO] Import complete: ") +
                                    std::to_string(clips) + " clips, " +
                                    std::to_string(warnings) + " warnings, " +
                                    std::to_string(errors) + " errors";
    SendMessage(hwndList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(summaryLine.c_str()));

    // Scroll to summary line if auto-scroll is still enabled.
    if (s_autoScroll) {
        if (const int count = static_cast<int>(SendMessage(hwndList, LB_GETCOUNT, 0, 0)); count > 0)
            SendMessage(hwndList, LB_SETCURSEL, static_cast<WPARAM>(count - 1), 0);
    }
    // Window stays open (WIN-06): no DestroyWindow call here.
    // Timer keeps running. User closes via Close button.
}
