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

#ifndef CDRF_DODEFAULT
#  define CDRF_DODEFAULT        0x00000000
#endif
#ifndef CDRF_NOTIFYITEMDRAW
#  define CDRF_NOTIFYITEMDRAW   0x00000020
#endif
#ifndef CDIS_SELECTED
#  define CDIS_SELECTED         0x0001
#endif
#ifndef COLOR_WINDOWTEXT
#  define COLOR_WINDOWTEXT      8
#endif
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

            // Set up ListView extended styles and columns.
            HWND hwndList = GetDlgItem(hwnd, IDC_LOG_LIST);
            ListView_SetExtendedListViewStyleEx(hwndList,
                                                LVS_EX_FULLROWSELECT,
                                                LVS_EX_FULLROWSELECT);

            LVCOLUMN col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = const_cast<char *>("Level");
            col.cx = 55;
            ListView_InsertColumn(hwndList, 0, &col);

            col.pszText = const_cast<char *>("Message");
            RECT listRc;
            GetClientRect(hwndList, &listRc);
            col.cx = listRc.right;
            ListView_InsertColumn(hwndList, 1, &col);

            self->populate();

            // Register keyboard accelerator so REAPER routes keys to HandleKey.
            self->m_accel.translateAccel = HandleKey;
            self->m_accel.isLocal = true;
            self->m_accel.user = self;
            plugin_register("accelerator", &self->m_accel);

            // Set up resizer — anchors are (left, top, right, bottom).
            // 0.0 = anchored to that edge of the dialog, 1.0 = moves with the opposite edge.
            self->m_resizer.init(hwnd);
            self->m_resizer.init_item(IDC_PROGRESS_LABEL, 0.0f, 0.0f, 1.0f, 0.0f); // stretches right, fixed top
            self->m_resizer.init_item(IDC_LOG_LIST, 0.0f, 0.0f, 1.0f, 1.0f); // stretches both axes
            self->m_resizer.init_item(IDC_CLOSE_BTN, 1.0f, 1.0f, 1.0f, 1.0f); // anchors bottom-right

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

        case WM_ACTIVATE: {
            self->m_isFocused = LOWORD(wParam) == WA_ACTIVE;
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

        case WM_NOTIFY: {
            const auto *hdr = reinterpret_cast<NMHDR *>(lParam);
            if (hdr->idFrom != IDC_LOG_LIST || hdr->code != NM_CUSTOMDRAW)
                return 0;
            auto *nmcd = reinterpret_cast<NMLVCUSTOMDRAW *>(lParam);
            return self->onCustomDraw(nmcd);
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// Private: onCustomDraw
// ---------------------------------------------------------------------------

LRESULT LogDialog::onCustomDraw(NMLVCUSTOMDRAW *nmcd) const {
    switch (nmcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT: {
            // Let the system render selected rows naturally
            if (nmcd->nmcd.uItemState & CDIS_SELECTED)
                return CDRF_DODEFAULT;

            const int idx = static_cast<int>(nmcd->nmcd.dwItemSpec);
            if (idx < 0 || idx >= m_buf.size())
                return CDRF_DODEFAULT;

            const LogEntry &e = m_buf.at(idx);
            switch (e.severity) {
                case LogEntry::ERROR: nmcd->clrText = RGB(220, 0,   0);                  break;
                case LogEntry::WARN:  nmcd->clrText = RGB(180, 180, 0);                  break;
                default:              nmcd->clrText = GetSysColor(COLOR_WINDOWTEXT);     break;
            }
            return CDRF_DODEFAULT;
        }

        default:
            return CDRF_DODEFAULT;
    }
}

// ---------------------------------------------------------------------------
// Private: populate
// ---------------------------------------------------------------------------

void LogDialog::populate() const {
    HWND hwndList = GetDlgItem(m_hwnd, IDC_LOG_LIST);

    SendMessage(hwndList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hwndList);

    int warnings = 0, errors = 0;
    const int n = m_buf.size();
    for (int i = 0; i < n; ++i) {
        const LogEntry e = m_buf.at(i);

        auto level = "";
        switch (e.severity) {
            case LogEntry::ERROR: level = "ERROR";
                errors++;
                break;
            case LogEntry::WARN: level = "WARN";
                warnings++;
                break;
            case LogEntry::INFO: level = "INFO";
                break;
        }

        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = const_cast<char *>(level);
        ListView_InsertItem(hwndList, &item);

        ListView_SetItemText(hwndList, i, 1,
                             const_cast<char *>(e.text.c_str()));
    }

    SendMessage(hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndList, nullptr, TRUE);

    // Summary label
    char label[128];
    snprintf(label, sizeof(label),
             "Import complete: %d warnings, %d errors", warnings, errors);
    SetDlgItemText(m_hwnd, IDC_PROGRESS_LABEL, label);

    // Scroll to last row
    if (n > 0)
        ListView_EnsureVisible(hwndList, n - 1, FALSE);
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


int LogDialog::HandleKey(MSG *msg, accelerator_register_t *accel) {
    const auto dlg = static_cast<LogDialog *>(accel->user);
    if (!dlg || !dlg->m_isFocused)
        return 0;

    if (const int key = static_cast<int>(msg->wParam); key == VK_ESCAPE) {
        dlg->close();
        return 1;
    }
    return 0;
}

void LogDialog::close() const {
    DestroyWindow(m_hwnd);
}
