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
#include "wdltypes.h"
#include "wingui/wndsize.h"
#include <memory>
#include <vector>
#ifdef _WIN32
#include <commctrl.h>
#endif

// Modeless dialog that displays log entries collected during an AAF import.
// At most one instance exists at a time.
class LogDialog {
  public:
    explicit LogDialog(std::unique_ptr<LogBuffer> buf, LogEntry::Severity minSeverity);

    LogDialog(const LogDialog &) = delete;

    LogDialog &operator=(const LogDialog &) = delete;

    LogDialog(LogDialog &&) = delete;

    LogDialog &operator=(LogDialog &&) = delete;

    ~LogDialog() = default;

    static void open(std::unique_ptr<LogBuffer> buf, LogEntry::Severity minSeverity);

  private:
    struct InsertResult {
        std::vector<int> rowBufIdx;
        int info = 0, warnings = 0, errors = 0;
    };

    std::unique_ptr<LogBuffer> m_buf;
    HWND m_hwnd = nullptr;
    WDL_WndSizer m_resizer;
    accelerator_register_t m_accel = {};
    bool m_isFocused = false;
    bool m_showInfo = true;
    bool m_showWarn = true;
    bool m_showError = true;
    bool m_showDebug = true;

    static std::unique_ptr<LogDialog> s_owner;

    void setupResizer(HWND hwnd);

    void registerAccel();

    static void setupFilterChecks(HWND hwnd, const LogDialog *self);

    [[nodiscard]] InsertResult insertRows(HWND hwndList) const;

    static void updateSummaryLabel(HWND hwnd, int info, int warnings, int errors);

    [[nodiscard]] bool shouldShow(LogEntry::Severity s) const;

    void close() const;

    void populate() const;

    void copyToClipboard() const;

    static int HandleKey(MSG *msg, accelerator_register_t *accel);

    static WDL_DLGRET CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

#endif // REAPER_AAF_LOGDIALOG_H
