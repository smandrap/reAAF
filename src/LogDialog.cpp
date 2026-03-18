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

#include "LogDialog.h"
#include "resource.h"
#include "reaper_plugin_functions.h"
#include "reaper_plugin.h"

#include <cstdio>    // snprintf
#include <string>
#include <utility>   // std::move

// ---------------------------------------------------------------------------
// External declarations (defined in main.cpp)
// ---------------------------------------------------------------------------

extern REAPER_PLUGIN_HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// Singleton instance definition
// ---------------------------------------------------------------------------

LogDialog *LogDialog::s_instance = nullptr;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LogDialog::LogDialog(LogBuffer buf) : m_buf(std::move(buf)) {}

// ---------------------------------------------------------------------------
// Private: formatEntry
// ---------------------------------------------------------------------------

std::string LogDialog::formatEntry(const LogEntry &e) {
    auto prefix = "";
    switch (e.severity) {
        case LogEntry::ERROR: prefix = "[ERROR]"; break;
        case LogEntry::WARN:  prefix = "[WARN]";  break;
        case LogEntry::INFO:  prefix = "[INFO]";  break;
    }
    return std::string(prefix) + " " + e.text;
}

// ---------------------------------------------------------------------------
// Private: dialogProc
// ---------------------------------------------------------------------------

WDL_DLGRET CALLBACK LogDialog::dialogProc(HWND hwnd, const UINT msg, const WPARAM wParam, LPARAM lParam) {
    // On WM_INITDIALOG lParam carries the LogDialog* passed via CreateDialogParam.
    // Store it in GWLP_USERDATA so every subsequent message can retrieve it.
    auto *self = reinterpret_cast<LogDialog *>(
        msg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hwnd, GWLP_USERDATA)
    );

    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
            self->m_hwnd = hwnd;

            self->populate();

            // Register keyboard accelerator so REAPER routes keys to HandleKey.
            self->m_accel.translateAccel = HandleKey;
            self->m_accel.isLocal        = true;
            self->m_accel.user           = self;
            plugin_register("accelerator", &self->m_accel);

            // Set up resizer — anchors are (left, top, right, bottom).
            // 0.0 = anchored to that edge of the dialog, 1.0 = moves with the opposite edge.
            self->m_resizer.init(hwnd);
            self->m_resizer.init_item(IDC_PROGRESS_LABEL, 0.0f, 0.0f, 1.0f, 0.0f); // stretches right, fixed top
            self->m_resizer.init_item(IDC_LOG_LIST,        0.0f, 0.0f, 1.0f, 1.0f); // stretches both axes
            self->m_resizer.init_item(IDC_CLOSE_BTN,       1.0f, 1.0f, 1.0f, 1.0f); // anchors bottom-right

            return 1;
        }

        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED)
                self->m_resizer.onResize();
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_CLOSE_BTN)
               self->close();
            return 0;
        }

        case WM_CLOSE: {
            self->close();
            return 0;
        }

        case WM_DESTROY: {
            plugin_register("-accelerator", &self->m_accel);
            s_instance = nullptr;
            delete self;
            return 0;
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// Private: populate
// ---------------------------------------------------------------------------

void LogDialog::populate() const {
    HWND hwndList = GetDlgItem(m_hwnd, IDC_LOG_LIST);

    // Clear any existing content.
    SendMessage(hwndList, LB_RESETCONTENT, 0, 0);

    // Bulk-load all entries and compute summary counts in one pass.
    int warnings = 0, errors = 0;
    const int n = m_buf.size();
    for (int i = 0; i < n; ++i) {
        const LogEntry e = m_buf.at(i);
        std::string line = formatEntry(e);
        SendMessage(hwndList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
        switch (e.severity) {
            case LogEntry::WARN:  warnings++; break;
            case LogEntry::ERROR: errors++;   break;
            default: break;
        }
    }

    // Set summary label.
    char label[128];
    snprintf(label, sizeof(label),
             "Import complete: %d warnings, %d errors",
             warnings, errors);
    SetDlgItemText(m_hwnd, IDC_PROGRESS_LABEL, label);

    // Scroll to bottom.
    if (const int count = static_cast<int>(SendMessage(hwndList, LB_GETCOUNT, 0, 0)); count > 0)
        SendMessage(hwndList, LB_SETCURSEL, static_cast<WPARAM>(count - 1), 0);
}

// ---------------------------------------------------------------------------
// Public: open
// ---------------------------------------------------------------------------

void LogDialog::open(LogBuffer buf) {
    if (s_instance) {
        s_instance->m_buf = std::move(buf);
        s_instance->populate();
        SetForegroundWindow(s_instance->m_hwnd);
        return;
    }
    s_instance = new LogDialog(std::move(buf));
    HWND parent = GetMainHwnd();
    HWND hwnd = CreateDialogParam(g_hInst,
                                  MAKEINTRESOURCE(IDD_AAF_PROGRESS),
                                  parent,
                                  dialogProc,
                                  reinterpret_cast<LPARAM>(s_instance));
    if (!hwnd) {
        delete s_instance;
        s_instance = nullptr;
        return;
    }
    ShowWindow(hwnd, SW_SHOW);
}

// ---------------------------------------------------------------------------
// Public: HandleKey / close
// ---------------------------------------------------------------------------

int LogDialog::HandleKey(MSG *msg, accelerator_register_t *accel) {
    const auto dlg = static_cast<LogDialog *>(accel->user);
    if (!dlg)
        return 0;
    accel->isLocal = true;

    if (const int key = static_cast<int>(msg->wParam); key == VK_ESCAPE) {
        dlg->close();
        return 1;
    }
    return 0;
}

void LogDialog::close() const {
    DestroyWindow(m_hwnd);
}
