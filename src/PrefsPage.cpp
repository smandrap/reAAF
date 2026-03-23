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

#include <cstdio>    // snprintf
#include <cstdlib>   // strtol

#include "PrefsPage.h"
#include "reaper_plugin_functions.h"
#include "resource.h"
#include "wdltypes.h"


static constexpr auto kSection = "reaper_aaf";
static constexpr auto kKeyVerb = "verbosity";


// Forward declarations
static WDL_DLGRET CALLBACK prefsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern REAPER_PLUGIN_HINSTANCE g_hInst;


static prefs_page_register_t g_prefs_reg = {
    "reaper_aaf_prefs", // idstr — globally unique
    "AAF Import", // displayname — shown in REAPER Preferences tree
    PrefsPage::createHwnd,
    0x9a, "",
    0
};


void PrefsPage::registerPage() {
    plugin_register("prefpage", &g_prefs_reg);
}

void PrefsPage::unregisterPage() {
    plugin_register("-prefpage", &g_prefs_reg);
}

PrefsPage::LogVerbosity PrefsPage::getVerbosity() {
    if (!HasExtState(kSection, kKeyVerb)) return LogVerbosity::ERR; // default: On Errors or Warnings
    const char *s = GetExtState(kSection, kKeyVerb);
    char *end;
    const long v = strtol(s, &end, 10);
    if (end == s || v < 0 || v > 2) return LogVerbosity::ERR; // unparseable or out of range
    return static_cast<LogVerbosity>(v);
}

void PrefsPage::setVerbosity(const int v) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", v);
    SetExtState(kSection, kKeyVerb, buf, true);
}

// REAPER sends this message to the active prefs page when Apply is clicked.
static constexpr UINT WM_PREFS_APPLY = WM_USER * 2;
// REAPER's Apply button control ID — enable it when a change is pending.
static constexpr int IDC_PREFS_APPLY = 0x478;

static WDL_DLGRET CALLBACK prefsDialogProc(HWND hwnd, const UINT msg, const WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            HWND combo = GetDlgItem(hwnd, IDC_COMBO_VERBOSITY);
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Never"));
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("On Errors or Warnings"));
            SendMessage(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Always"));
            SendMessage(combo, CB_SETCURSEL, static_cast<int>(PrefsPage::getVerbosity()), 0);
            EnableWindow(GetDlgItem(hwnd, IDC_VERSION_LABEL), FALSE);
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
    return CreateDialog(g_hInst,
                        MAKEINTRESOURCE(IDD_AAF_PREFS),
                        par,
                        prefsDialogProc);
}
