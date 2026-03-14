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

#ifdef _WIN32
#   include <windows.h>
#   include <direct.h>
#else
#   include <sys/stat.h>
#endif

#include <cerrno>
#include "helpers.h"
#include "reaper_plugin_functions.h"
#include "libaaf/AAFIface.h"

// Map AAFInterpolation flags to REAPER fade shape index.
// REAPER: 0=linear, 1=quarter-sine, 2=equal power, 3=slow start, 4=fast start, 5=bezier
int interpol_to_reaper_shape(const uint32_t flags) {
    if (flags & AAFI_INTERPOL_LINEAR) return 0;
    if (flags & AAFI_INTERPOL_POWER) return 4;
    if (flags & AAFI_INTERPOL_LOG) return 3;
    if (flags & AAFI_INTERPOL_BSPLINE) return 5;
    return 1; // default: quarter-sine
}

bool ensure_dir(const std::string &path) {
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0 || errno == EEXIST) return true;
#else
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) return true;
#endif
    return false;
}

void rlog(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ShowConsoleMsg(buf);
}

