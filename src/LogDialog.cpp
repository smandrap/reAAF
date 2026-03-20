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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "LogDialog.h"
#include "resource.h"
#include "reaper_plugin_functions.h"


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

LogDialog::LogDialog(LogBuffer buf, bool showInfo, bool showWarn, bool showError)
    : m_buf(std::move(buf))
      , m_showInfo(showInfo), m_showWarn(showWarn), m_showError(showError) {}

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
            char colLevel[] = "Level";
            col.pszText = colLevel;
            col.cx = 55;
            ListView_InsertColumn(hwndList, 0, &col);

            char colMessage[] = "Message";
            col.pszText = colMessage;
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

            self->m_resizer.init_item(IDC_LOGFILTER_INFO, 0.0f, 1.0f, 0.0f, 1.0f);
            self->m_resizer.init_item(IDC_LOGFILTER_WARN, 0.0f, 1.0f, 0.0f, 1.0f);
            self->m_resizer.init_item(IDC_LOGFILTER_ERROR, 0.0f, 1.0f, 0.0f, 1.0f);

            CheckDlgButton(hwnd, IDC_LOGFILTER_INFO, self->m_showInfo ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_LOGFILTER_WARN, self->m_showWarn ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_LOGFILTER_ERROR, self->m_showError ? BST_CHECKED : BST_UNCHECKED);

            return 1;
        }

        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED)
                self->m_resizer.onResize();
            return 0;
        }

        case WM_COMMAND: {
            if (const int id = LOWORD(wParam); id == IDC_CLOSE_BTN) {
                self->close();
            } else if (id == IDC_LOGFILTER_INFO || id == IDC_LOGFILTER_WARN || id == IDC_LOGFILTER_ERROR) {
                self->m_showInfo = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_INFO) == BST_CHECKED;
                self->m_showWarn = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_WARN) == BST_CHECKED;
                self->m_showError = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_ERROR) == BST_CHECKED;
                self->populate();
            }
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

        default:
            return 0;
    }
}


// ---------------------------------------------------------------------------
// Private: populate
// ---------------------------------------------------------------------------

void LogDialog::populate() const {
    HWND hwndList = GetDlgItem(m_hwnd, IDC_LOG_LIST);

    // Save buffer indices of selected/focused rows using the idiomatic LVNI_ iteration
    std::vector<int> selBufIndices;
    int focusBufIdx = -1;
    for (int j = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED); j != -1;
         j = ListView_GetNextItem(hwndList, j, LVNI_SELECTED)) {
        LVITEM lvi = {};
        lvi.mask = LVIF_PARAM;
        lvi.iItem = j;
        ListView_GetItem(hwndList, &lvi);
        selBufIndices.push_back(static_cast<int>(lvi.lParam));
    }
    {
        if (const int fj = ListView_GetNextItem(hwndList, -1, LVNI_FOCUSED); fj != -1) {
            LVITEM lvi = {};
            lvi.mask = LVIF_PARAM;
            lvi.iItem = fj;
            ListView_GetItem(hwndList, &lvi);
            focusBufIdx = static_cast<int>(lvi.lParam);
        }
    }

    SendMessage(hwndList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hwndList);

    std::vector<int> rowBufIdx;
    int info = 0, warnings = 0, errors = 0, row = 0;
    const int n = m_buf.size();
    for (int i = 0; i < n; ++i) {
        const LogEntry &e = m_buf.at(i);

        const char *level;
        bool show;
        switch (e.severity) {
            case LogEntry::ERR: level = "ERROR";
                ++errors;
                show = m_showError;
                break;
            case LogEntry::WARN: level = "WARN";
                ++warnings;
                show = m_showWarn;
                break;
            default: level = "INFO";
                ++info;
                show = m_showInfo;
                break;
        }
        if (!show) continue;

        LVITEM item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.lParam = static_cast<LPARAM>(i);
        item.pszText = const_cast<char *>(level);
        ListView_InsertItem(hwndList, &item);

        std::string rowText = e.text;
        ListView_SetItemText(hwndList, row, 1, rowText.data());
        rowBufIdx.push_back(i);
        ++row;
    }

    ListView_SetItemState(hwndList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    for (int r = 0; r < row; ++r) {
        const int bi = rowBufIdx[r];
        UINT newState = 0;
        if (std::find(selBufIndices.begin(), selBufIndices.end(), bi) != selBufIndices.end())
            newState |= LVIS_SELECTED;
        if (bi == focusBufIdx)
            newState |= LVIS_FOCUSED;
        if (newState)
            ListView_SetItemState(hwndList, r, newState, LVIS_SELECTED | LVIS_FOCUSED);
    }

    SendMessage(hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndList, nullptr, TRUE);

    // Summary label
    char label[128];
    snprintf(label, sizeof(label),
             "Import complete: %d messages, %d warnings, %d errors", info, warnings, errors);
    SetDlgItemText(m_hwnd, IDC_PROGRESS_LABEL, label);

    // Scroll to last visible row
    if (row > 0)
        ListView_EnsureVisible(hwndList, row - 1, FALSE);
}


void LogDialog::open(LogBuffer buf, const bool showInfo, const bool showWarn, const bool showError) {
    if (s_instance) {
        s_instance->m_buf = std::move(buf);
        s_instance->m_showInfo = showInfo;
        s_instance->m_showWarn = showWarn;
        s_instance->m_showError = showError;
        HWND hw = s_instance->m_hwnd;
        CheckDlgButton(hw, IDC_LOGFILTER_INFO, showInfo ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hw, IDC_LOGFILTER_WARN, showWarn ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hw, IDC_LOGFILTER_ERROR, showError ? BST_CHECKED : BST_UNCHECKED);
        s_instance->populate();
        SetForegroundWindow(s_instance->m_hwnd);
        return;
    }
    s_instance = new LogDialog(std::move(buf), showInfo, showWarn, showError);
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
