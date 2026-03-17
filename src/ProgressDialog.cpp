/*
 * Copyright (C) 2026 Federico Manuppella
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ProgressDialog.h"
#include "LogBuffer.h"
#include "resource.h"
#include "reaper_plugin_functions.h"
#include "wdltypes.h"
#include "wingui/wndsize.h"

#include <cstdio>    // snprintf
#include <string>

// ---------------------------------------------------------------------------
// External declarations (defined in main.cpp)
// ---------------------------------------------------------------------------

extern REAPER_PLUGIN_HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// Module-scope state
// ---------------------------------------------------------------------------

// HWND of the open dialog, or nullptr when closed.
static HWND s_hwnd = nullptr;
// Injected buffer pointer — set before CreateDialog (SWELL WM_INITDIALOG caveat).
static LogBuffer *s_logBuffer = nullptr;
// Resizer — initialized in WM_INITDIALOG, drives WM_SIZE layout.
static WDL_WndSizer s_resizer;

// ---------------------------------------------------------------------------
// formatEntry — build the display string for one LogEntry.
// ---------------------------------------------------------------------------

static std::string formatEntry(const LogEntry &e) {
    const char *prefix = "";
    switch (e.severity) {
        case LogEntry::ERROR: prefix = "[ERROR]"; break;
        case LogEntry::WARN:  prefix = "[WARN]";  break;
        case LogEntry::INFO:  prefix = "[INFO]";  break;
    }
    return std::string(prefix) + " " + e.text;
}

// ---------------------------------------------------------------------------
// Dialog procedure
// ---------------------------------------------------------------------------

static WDL_DLGRET CALLBACK progressDialogProc(HWND hwnd, const UINT msg, const WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            HWND hwndList = GetDlgItem(hwnd, IDC_LOG_LIST);

            // Bulk-load all entries and compute summary counts in one pass.
            int warnings = 0, errors = 0;
            const int n = s_logBuffer->size();
            for (int i = 0; i < n; ++i) {
                const LogEntry e = s_logBuffer->at(i);
                std::string line = formatEntry(e);
                SendMessage(hwndList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
                switch (e.severity) {
                    case LogEntry::WARN:  warnings++; break;
                    case LogEntry::ERROR: errors++;   break;
                    default: break;
                }
            }

            // Set summary label.
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "Import complete: %d warnings, %d errors",
                     warnings, errors);
            SetDlgItemText(hwnd, IDC_PROGRESS_LABEL, buf);

            // Scroll to bottom.
            if (const int count = static_cast<int>(SendMessage(hwndList, LB_GETCOUNT, 0, 0)); count > 0)
                SendMessage(hwndList, LB_SETCURSEL, static_cast<WPARAM>(count - 1), 0);

            // Set up resizer — anchors are (left, top, right, bottom).
            // 0.0 = anchored to that edge of the dialog, 1.0 = moves with the opposite edge.
            s_resizer.init(hwnd);
            // s_resizer.init_item(IDC_PROGRESS_LABEL, 0.0f, 0.0f, 1.0f, 0.0f); // stretches right, fixed top
            s_resizer.init_item(IDC_LOG_LIST,        0.0f, 0.0f, 1.0f, 1.0f); // stretches both axes
            s_resizer.init_item(IDC_CLOSE_BTN,       1.0f, 1.0f, 1.0f, 1.0f); // anchors bottom-right

            return 1;
        }

        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED)
                s_resizer.onResize();
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

void ProgressDialog_Open(LogBuffer *buf) {
    if (s_hwnd) {
        SetForegroundWindow(s_hwnd);
        return;
    }
    s_logBuffer = buf; // MUST be assigned BEFORE CreateDialog (SWELL calls WM_INITDIALOG synchronously)
    HWND parent = GetMainHwnd();
    s_hwnd = CreateDialog(g_hInst,
                          MAKEINTRESOURCE(IDD_AAF_PROGRESS),
                          parent,
                          progressDialogProc);
    if (s_hwnd) ShowWindow(s_hwnd, SW_SHOW);
}
