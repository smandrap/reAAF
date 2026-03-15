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

#include "PrefsPage.h"
#include "LogBuffer.h"
#include "reaper_plugin_functions.h"
#include "resource.h"
#include "wdltypes.h"

#include <cstdio>    // snprintf
#include <cstdlib>   // atoi

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr const char *kSection = "reaper_aaf";
static constexpr const char *kKeyVerb = "verbosity";

// ---------------------------------------------------------------------------
// External declarations
// ---------------------------------------------------------------------------

// g_logBuffer is defined in main.cpp (module-scope, not static)
extern LogBuffer g_logBuffer;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static WDL_DLGRET CALLBACK prefsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern REAPER_PLUGIN_HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// File-scope static struct — CRITICAL: must be file-scope, not local or heap.
// REAPER holds a raw pointer and writes back hwndCache and treeitem here.
// ---------------------------------------------------------------------------

static prefs_page_register_t g_prefs_reg = {
    "reaper_aaf_prefs", // idstr — globally unique
    "AAF Import", // displayname — shown in REAPER Preferences tree
    PrefsPage::createHwnd,
    0, // par_id — top-level entry
    nullptr, // par_idstr — nullptr for top-level entry
    0, // childrenFlag — leaf node
    nullptr, // treeitem — REAPER fills in
    nullptr, // hwndCache — REAPER fills in
    {} // _extra[64] — zero-init reserved
};

// ---------------------------------------------------------------------------
// PrefsPage::registerPage / unregisterPage_static
// ---------------------------------------------------------------------------

void PrefsPage::registerPage(const reaper_plugin_info_t *rec) {
    rec->Register("prefpage", &g_prefs_reg);
}

void PrefsPage::unregisterPage_static(RegisterFn fn) {
    if (fn) fn("-prefpage", &g_prefs_reg);
}

// ---------------------------------------------------------------------------
// PrefsPage::getVerbosity / setVerbosity
// ---------------------------------------------------------------------------

int PrefsPage::getVerbosity() {
    if (!HasExtState(kSection, kKeyVerb)) return 1; // default: Normal
    // atoi immediately — do NOT store the pointer from GetExtState
    return atoi(GetExtState(kSection, kKeyVerb));
}

void PrefsPage::setVerbosity(int v) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", v);
    SetExtState(kSection, kKeyVerb, buf, /*persist=*/true);
    // Sync the runtime LogBuffer so the new verbosity takes effect immediately
    g_logBuffer.setVerbosity(v);
}

// REAPER sends this message to the active prefs page when Apply is clicked.
static constexpr UINT WM_PREFS_APPLY = WM_USER * 2;
// REAPER's Apply button control ID — enable it when a change is pending.
static constexpr int IDC_PREFS_APPLY = 0x478;

// ---------------------------------------------------------------------------
// Dialog procedure
// ---------------------------------------------------------------------------

static WDL_DLGRET CALLBACK prefsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            HWND combo = GetDlgItem(hwnd, IDC_COMBO_VERBOSITY);
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("None"));
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Errors and Warnings"));
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Full Log"));
            SendMessage(combo, CB_SETCURSEL, PrefsPage::getVerbosity(), 0);
            return 1;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_COMBO_VERBOSITY && HIWORD(wParam) == CBN_SELCHANGE)
                EnableWindow(GetDlgItem(GetParent(hwnd), IDC_PREFS_APPLY), TRUE);
            return 0;
        }

        default:
            if (msg == WM_PREFS_APPLY) {
                if (const int v = static_cast<int>(SendMessage(GetDlgItem(hwnd, IDC_COMBO_VERBOSITY), CB_GETCURSEL, 0,
                                                               0)); v >= 0)
                    PrefsPage::setVerbosity(v);
            }
            return 0;
    }
}

// ---------------------------------------------------------------------------
// PrefsPage::createHwnd — create child HWND embedded in REAPER Preferences pane
// ---------------------------------------------------------------------------

HWND PrefsPage::createHwnd(HWND par) {
    // CreateDialog with resource ID IDD_AAF_PREFS creates a child window.
    // On macOS/Linux this uses the resource defined in resource_swell.cpp.
    // On Windows this uses the resource defined in resource.rc.
    // The SWELL_DLG_WS_CHILD | SWELL_DLG_WS_FLIPPED flags ensure correct layout.
    return CreateDialog(g_hInst,
                        MAKEINTRESOURCE(IDD_AAF_PREFS),
                        par,
                        prefsDialogProc);
}
