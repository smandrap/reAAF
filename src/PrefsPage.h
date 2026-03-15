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

#ifndef REAPER_AAF_PREFSPAGE_H
#define REAPER_AAF_PREFSPAGE_H

#include "reaper_plugin.h"   // HWND, prefs_page_register_t

// RegisterFn matches the signature of reaper_plugin_info_t::Register
using RegisterFn = int (*)(const char*, void*);

class PrefsPage {
public:
    // Called from REAPER_PLUGIN_ENTRYPOINT to register the preferences page.
    static void registerPage(const reaper_plugin_info_t* rec);

    // Called from the atexit callback to unregister via the stored Register fn pointer.
    // g_prefs_reg is file-scope static in PrefsPage.cpp; REAPER must not call it after unload.
    static void unregisterPage_static(RegisterFn fn);

    // Read persisted verbosity (0=None, 1=Normal, 2=Verbose).
    // Returns 1 (Normal) if no SetExtState entry exists.
    static int  getVerbosity();
    static void setVerbosity(int v);

    // Factory function stored in g_prefs_reg.create — REAPER calls this to create
    // the preferences panel child HWND. Do not call directly.
    static HWND createHwnd(HWND par);
};

#endif // REAPER_AAF_PREFSPAGE_H
