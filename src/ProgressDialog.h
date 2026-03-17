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

#include "LogBuffer.h"

// Opens the modeless progress dialog and bulk-loads all entries from buf.
// If already open, brings it to the foreground.
// buf must remain valid for the lifetime of the dialog.
// Call from main thread only.
void ProgressDialog_Open(LogBuffer *buf);

#endif // REAPER_AAF_PROGRESSDIALOG_H
