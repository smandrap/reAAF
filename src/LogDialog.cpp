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
#include "reaper_plugin_functions.h"
#include "resource.h"

namespace {
void setupListView(HWND hwnd) {
    // Set up ListView extended styles and columns.
    HWND hwndList = GetDlgItem(hwnd, IDC_LOG_LIST);
    if ( !hwndList )
        return;
    ListView_SetExtendedListViewStyleEx(hwndList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

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
}

struct SelectionState {
    std::vector<int> selected;
    int focused = -1;
};

auto saveListViewSelection(HWND hwndList) -> SelectionState {
    SelectionState sel;
    for ( int i = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED); i != -1;
          i = ListView_GetNextItem(hwndList, i, LVNI_SELECTED) ) {
        LVITEM lvi = {};
        lvi.mask = LVIF_PARAM;
        lvi.iItem = i;
        ListView_GetItem(hwndList, &lvi);
        sel.selected.push_back(static_cast<int>(lvi.lParam));
    }
    if ( const int fi = ListView_GetNextItem(hwndList, -1, LVNI_FOCUSED); fi != -1 ) {
        LVITEM lvi = {};
        lvi.mask = LVIF_PARAM;
        lvi.iItem = fi;
        ListView_GetItem(hwndList, &lvi);
        sel.focused = static_cast<int>(lvi.lParam);
    }
    return sel;
}

void restoreListViewSelection(HWND hwnd, const SelectionState &selState,
                              const std::vector<int> &rowBufIdx) {
    ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    for ( int r = 0; r < static_cast<int>(rowBufIdx.size()); ++r ) {
        const int bi = rowBufIdx[r];
        UINT state = 0;
        if ( std::find(selState.selected.begin(), selState.selected.end(), bi) !=
             selState.selected.end() )
            state |= LVIS_SELECTED;
        if ( bi == selState.focused )
            state |= LVIS_FOCUSED;
        if ( state )
            ListView_SetItemState(hwnd, r, state, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void setClipboardText(HWND hwnd, const std::string &text) {
#ifdef _WIN32
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if ( wlen <= 0 )
        return;
    std::vector<wchar_t> wbuf(wlen);
    if ( MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wbuf.data(), wlen) <= 0 )
        return;
    const HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if ( !mem )
        return;
    wchar_t *dst = static_cast<wchar_t *>(GlobalLock(mem));
    if ( !dst ) {
        GlobalFree(mem);
        return;
    }
    std::copy(wbuf.begin(), wbuf.end(), dst);
    GlobalUnlock(mem);
    if ( !OpenClipboard(hwnd) ) {
        GlobalFree(mem);
        return;
    }
    EmptyClipboard();
    if ( !SetClipboardData(CF_UNICODETEXT, mem) )
        GlobalFree(mem);
    CloseClipboard();
#else
    HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if ( !mem )
        return;
    const auto dst = static_cast<char *>(GlobalLock(mem));
    if ( !dst ) {
        GlobalFree(mem);
        return;
    }
    std::copy(text.begin(), text.end(), dst);
    dst[text.size()] = '\0';
    GlobalUnlock(mem);
    if ( !OpenClipboard(hwnd) ) {
        GlobalFree(mem);
        return;
    }
    EmptyClipboard();
    const unsigned int fmt = RegisterClipboardFormat("SWELL__CF_TEXT");
    if ( !fmt ) {
        GlobalFree(mem);
        CloseClipboard();
        return;
    }
    SetClipboardData(fmt, mem);
    CloseClipboard();
#endif
}
} // namespace

extern REAPER_PLUGIN_HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// Singleton instance definition
// ---------------------------------------------------------------------------

std::unique_ptr<LogDialog> LogDialog::s_owner;

LogDialog::LogDialog(std::unique_ptr<LogBuffer> buf, const LogEntry::Severity minSeverity,
                     const bool isDebug)
    : m_buf(std::move(buf)), m_showInfo{minSeverity >= LogEntry::INFO},
      m_showWarn{minSeverity >= LogEntry::WARN}, m_isDebug{isDebug} {}


void LogDialog::setupResizer(HWND hwnd) {
    // Set up resizer — anchors are (left, top, right, bottom).
    // 0.0 = anchored to that edge of the dialog, 1.0 = moves with the opposite edge.
    m_resizer.init(hwnd);
    m_resizer.init_item(IDC_LOG_LABEL, 0.0f, 0.0f, 1.0f, 0.0f); // stretches right, fixed top
    m_resizer.init_item(IDC_LOG_LIST, 0.0f, 0.0f, 1.0f, 1.0f); // stretches both axes
    m_resizer.init_item(IDC_CLOSE_BTN, 1.0f, 1.0f, 1.0f, 1.0f); // anchors bottom-right
    m_resizer.init_item(IDC_COPY_BTN, 1.0f, 1.0f, 1.0f, 1.0f); // anchors bottom-right

    m_resizer.init_item(IDC_LOGFILTER_INFO, 0.0f, 1.0f, 0.0f, 1.0f);
    m_resizer.init_item(IDC_LOGFILTER_WARN, 0.0f, 1.0f, 0.0f, 1.0f);
    m_resizer.init_item(IDC_LOGFILTER_ERROR, 0.0f, 1.0f, 0.0f, 1.0f);
    m_resizer.init_item(IDC_LOGFILTER_DEBUG, 0.0f, 1.0f, 0.0f, 1.0f);
}

void LogDialog::registerAccel() {
    // Register keyboard accelerator so REAPER routes keys to HandleKey.
    m_accel.translateAccel = HandleKey;
    m_accel.isLocal = true;
    m_accel.user = static_cast<void *>(this);
    plugin_register("accelerator", &m_accel);
}

void LogDialog::setupFilterChecks(HWND hwnd, const LogDialog *self) {
    CheckDlgButton(hwnd, IDC_LOGFILTER_INFO, self->m_showInfo ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_LOGFILTER_WARN, self->m_showWarn ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_LOGFILTER_ERROR, self->m_showError ? BST_CHECKED : BST_UNCHECKED);


    ShowWindow(GetDlgItem(hwnd, IDC_LOGFILTER_DEBUG), self->m_isDebug ? SW_SHOW : SW_HIDE);
    if ( self->m_isDebug ) {
        CheckDlgButton(hwnd, IDC_LOGFILTER_DEBUG, self->m_showDebug ? BST_CHECKED : BST_UNCHECKED);
    }
}


WDL_DLGRET CALLBACK LogDialog::dialogProc(HWND hwnd, const UINT msg, const WPARAM wParam,
                                          LPARAM lParam) {
    // On WM_INITDIALOG lParam carries the LogDialog* passed via CreateDialogParam.
    // Store it in GWLP_USERDATA so every subsequent message can retrieve it.
    auto *self = reinterpret_cast<LogDialog *>(
        msg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch ( msg ) {
    case WM_INITDIALOG: {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
        self->m_hwnd = hwnd;

        setupListView(hwnd);
        self->populate();
        self->registerAccel();
        self->setupResizer(hwnd);
        setupFilterChecks(hwnd, self);

        return 1;
    }

    case WM_SIZE: {
        if ( wParam != SIZE_MINIMIZED )
            self->m_resizer.onResize();
        return 0;
    }

    case WM_COMMAND: {
        if ( const int id = LOWORD(wParam); id == IDC_CLOSE_BTN ) {
            self->close();
        } else if ( id == IDC_COPY_BTN ) {
            self->copyToClipboard();
        } else if ( id == IDC_LOGFILTER_INFO || id == IDC_LOGFILTER_WARN ||
                    id == IDC_LOGFILTER_ERROR || id == IDC_LOGFILTER_DEBUG ) {
            self->m_showInfo = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_INFO) == BST_CHECKED;
            self->m_showWarn = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_WARN) == BST_CHECKED;
            self->m_showError = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_ERROR) == BST_CHECKED;
            self->m_showDebug = IsDlgButtonChecked(hwnd, IDC_LOGFILTER_DEBUG) == BST_CHECKED;
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
        s_owner.reset();
        return 0;
    }

    default:
        return 0;
    }
}

bool LogDialog::shouldShow(const LogEntry::Severity s) const {
    switch ( s ) {
    case LogEntry::ERR:
        return m_showError;
    case LogEntry::WARN:
        return m_showWarn;
    case LogEntry::DEBUG:
        return m_showDebug;
    default:
        return m_showInfo;
    }
}


void LogDialog::updateSummaryLabel(HWND hwnd, const int info, const int warnings,
                                   const int errors) {
    char label[128];
    snprintf(label, sizeof(label), "Import complete: %d messages, %d warnings, %d errors", info,
             warnings, errors);
    SetDlgItemText(hwnd, IDC_LOG_LABEL, label);
}


auto LogDialog::insertRows(HWND hwndList) const -> InsertResult {
    InsertResult res;

    const int n = m_buf->size();
    for ( int i = 0; i < n; ++i ) {
        const LogEntry &e = m_buf->at(i);
        const char *level;
        switch ( e.severity ) {
        case LogEntry::DEBUG:
            level = "DEBUG";
            break;
        case LogEntry::ERR:
            level = "ERROR";
            ++res.errors;
            break;
        case LogEntry::WARN:
            level = "WARN";
            ++res.warnings;
            break;
        default:
            level = "INFO";
            ++res.info;
            break;
        }

        if ( !shouldShow(e.severity) )
            continue;
        LVITEM item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(res.rowBufIdx.size());
        item.lParam = static_cast<LPARAM>(i);
        item.pszText = const_cast<char *>(level);

        ListView_InsertItem(hwndList, &item);
        ListView_SetItemText(hwndList, item.iItem, 1, const_cast<char *>(e.text.c_str()));
        res.rowBufIdx.push_back(i);
    }

    return res;
}

void LogDialog::populate() const {
    HWND hwndList = GetDlgItem(m_hwnd, IDC_LOG_LIST);
    if ( !hwndList )
        return;

    const SelectionState sel = saveListViewSelection(hwndList);

    SendMessage(hwndList, WM_SETREDRAW, FALSE, 0);

    ListView_DeleteAllItems(hwndList);

    const auto [rowBufIdx, info, warnings, errors] = insertRows(hwndList);

    restoreListViewSelection(hwndList, sel, rowBufIdx);

    SendMessage(hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndList, nullptr, TRUE);

    updateSummaryLabel(m_hwnd, info, warnings, errors);

    if ( !rowBufIdx.empty() )
        ListView_EnsureVisible(hwndList, static_cast<int>(rowBufIdx.size() - 1), FALSE);
}


void LogDialog::open(std::unique_ptr<LogBuffer> buf, const LogEntry::Severity minSeverity,
                     const bool isDebug) {
    if ( s_owner ) {
        s_owner->m_buf = std::move(buf);
        s_owner->m_showInfo = (minSeverity >= LogEntry::INFO);
        s_owner->m_showWarn = (minSeverity >= LogEntry::WARN);
        s_owner->m_showDebug = isDebug;
        HWND hw = s_owner->m_hwnd;
        setupFilterChecks(hw, s_owner.get());
        s_owner->populate();
        SetForegroundWindow(s_owner->m_hwnd);
        return;
    }
    s_owner = std::make_unique<LogDialog>(std::move(buf), minSeverity, isDebug);
    HWND parent = GetMainHwnd();
    HWND hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_AAF_LOG_DIALOG), parent, dialogProc,
                                  reinterpret_cast<LPARAM>(s_owner.get()));

    if ( !hwnd ) {
        s_owner.reset();
        return;
    }
    ShowWindow(hwnd, SW_SHOW);
}

int LogDialog::HandleKey(MSG *msg, accelerator_register_t *accel) {
    const auto dlg = static_cast<LogDialog *>(accel->user);
    if ( !dlg || !dlg->m_isFocused )
        return 0;

    if ( const int key = static_cast<int>(msg->wParam); key == VK_ESCAPE ) {
        dlg->close();
        return 1;
    }
    return 0;
}

void LogDialog::close() const { DestroyWindow(m_hwnd); }

void LogDialog::copyToClipboard() const {
    std::string text;
    const int n = m_buf->size();
    for ( int i = 0; i < n; ++i ) {
#ifdef _WIN32
        constexpr const char *nl = "\r\n";
#else
        constexpr auto nl = "\n";
#endif
        const LogEntry &e = m_buf->at(i);
        if ( !shouldShow(e.severity) )
            continue;
        const char *level;
        switch ( e.severity ) {
        case LogEntry::ERR:
            level = "***[ ERROR ]*** :";
            break;
        case LogEntry::WARN:
            level = "*[ WARN ]* :";
            break;
        default:
            level = "[ INFO ] :";
            break;
        }
        text += level;
        text += '\t';
        text += e.text;
        text += nl;
    }
    setClipboardText(m_hwnd, text);
}
