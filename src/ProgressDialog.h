/*
* Copyright (C) 2026 Federico Manuppella
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef REAPER_AAF_PROGRESSDIALOG_H
#define REAPER_AAF_PROGRESSDIALOG_H

// Opens the modeless progress dialog (creates it if not already open).
// If already open, brings it to the foreground.
// Parent is GetMainHwnd() — requires REAPERAPI_WANT_GetMainHwnd in CMakeLists.
void ProgressDialog_Open();

// Signals import completion: appends summary line, updates label.
// clips / warnings / errors are the final counts.
// Call from main thread only (all dialog interaction must stay on main thread).
void ProgressDialog_MarkComplete(int clips, int warnings, int errors);

#endif // REAPER_AAF_PROGRESSDIALOG_H