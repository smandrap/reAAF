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
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include "helpers.h"
#include "reaper_plugin_functions.h"
#include <cerrno>

bool ensure_dir(const std::string &path) {
#ifdef _WIN32
    if ( _mkdir(path.c_str()) == 0 || errno == EEXIST )
        return true;
#else
    if ( mkdir(path.c_str(), 0755) == 0 || errno == EEXIST )
        return true;
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
