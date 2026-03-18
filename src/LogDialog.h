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
// At most one instance exists at a time (singleton). The caller retains
// ownership of the LogBuffer — LogDialog never frees it.
class LogDialog {
public:
    // Opens the dialog and bulk-loads all entries from buf.
    // If already open, refreshes the list with the new buffer and brings it to the foreground.
    // buf must remain valid for the lifetime of the dialog.
    // Call from main thread only.
    static void open(LogBuffer *buf);

    static int HandleKey(MSG *msg, accelerator_register_t *accel);

    void close() const;

private:
    explicit LogDialog(LogBuffer *buf);
    ~LogDialog() = default; // m_buf is not owned — never freed here

    static std::string     formatEntry(const LogEntry &e);
    static WDL_DLGRET CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Clears the listbox and summary label, then repopulates from m_buf.
    void populate() const;

    LogBuffer              *m_buf;             // not owned
    HWND                    m_hwnd = nullptr;
    WDL_WndSizer            m_resizer;
    accelerator_register_t  m_accel = {};

    static LogDialog *s_instance;  // nullptr when no dialog is open
};

#endif // REAPER_AAF_LOGDIALOG_H
