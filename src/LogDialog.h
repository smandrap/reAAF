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

#ifndef REAPER_AAF_LOGDIALOG_H
#define REAPER_AAF_LOGDIALOG_H

#include "LogBuffer.h"
#include "reaper_plugin.h"
#include "swell-types.h"
#include "wdltypes.h"
#include "wingui/wndsize.h"

// Modeless dialog that displays log entries collected during an AAF import.
// At most one instance exists at a time. The dialog owns its LogBuffer by
// value. Call from main thread only.
class LogDialog {
public:
    // Opens the dialog with a fresh buffer. If already open, refreshes the
    // list with the new data and brings the window to the foreground.
    static void open(LogBuffer buf, bool showInfo, bool showWarn, bool showError);

private:
    explicit LogDialog(LogBuffer buf, bool showInfo, bool showWarn, bool showError);

    ~LogDialog() = default;

    void close() const;

    void populate() const;

    LRESULT onCustomDraw(NMLVCUSTOMDRAW *nmcd) const;

    static int HandleKey(MSG *msg, accelerator_register_t *accel);

    static WDL_DLGRET CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    LogBuffer m_buf;
    HWND m_hwnd = nullptr;
    WDL_WndSizer m_resizer;
    accelerator_register_t m_accel = {};
    bool m_isFocused = false;
    bool m_showInfo  = true;
    bool m_showWarn  = true;
    bool m_showError = true;

    static LogDialog *s_instance;
};

#endif // REAPER_AAF_LOGDIALOG_H
