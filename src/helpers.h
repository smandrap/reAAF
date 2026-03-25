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

#ifndef REAPER_AAF_HELPERS_H
#define REAPER_AAF_HELPERS_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#include "libaaf/AAFIface.h"
#include "libaaf/AAFTypes.h"


[[nodiscard]] constexpr double rational_to_double(const aafRational_t r) noexcept {
    if ( r.denominator == 0 )
        return 0.0;
    return static_cast<double>(r.numerator) / static_cast<double>(r.denominator);
}

// Convert a position in edit-rate units to seconds.
// editRate is a POINTER — may be null, returns 0 safely.
inline double pos_to_seconds(const aafPosition_t pos, const aafRational_t *editRate) noexcept {
    if ( !editRate )
        return 0.0;
    const double er = rational_to_double(*editRate);
    if ( er < 1e-10 )
        return 0.0;
    return static_cast<double>(pos) / er;
}

inline std::string escape_rpp_string(const char *raw) {
    if ( !raw )
        return {};
    std::string out;
    out.reserve(strlen(raw) + 8); // Do I really need to preallocate those 8 bytes?
    for ( const char *p = raw; *p; ++p ) {
        if ( *p == '\\' ) {
            out += "\\\\";
            continue;
        } // Escape backslash
        if ( *p == '"' ) {
            out += "\\\"";
            continue;
        } // Escape quotes
        if ( *p == '\n' ) {
            out += "\\n";
            continue;
        } // Escape newline (RPP line injection vector)
        if ( *p == '\r' ) {
            out += "\\r";
            continue;
        } // Escape carriage return
        if ( static_cast<unsigned char>(*p) < 0x20 ) {
            out += ' ';
            continue;
        } // Strip other control chars
        out += *p;
    }
    return out;
}

[[nodiscard]] constexpr double clamp_volume(const double lin) noexcept {
    return std::clamp(lin, 0.0, 4.0);
}

[[nodiscard]] constexpr double clamp_pan(const double pan) noexcept {
    return std::clamp(pan, -1.0, 1.0);
}

[[nodiscard]] constexpr int aafiColorToReaper(const uint16_t rgb[3]) noexcept {
    return 0x1000000 | rgb[0] << 16 | rgb[1] << 8 | rgb[2];
}


// Map AAFInterpolation flags to REAPER fade shape index.
// REAPER: 0=linear, 1=quarter-sine, 2=equal power, 3=slow start, 4=fast start, 5=bezier
[[nodiscard]] inline int interpol_to_reaper_shape(const uint32_t flags) noexcept {
    if ( flags & AAFI_INTERPOL_LINEAR )
        return 0;
    if ( flags & AAFI_INTERPOL_POWER )
        return 4;
    if ( flags & AAFI_INTERPOL_LOG )
        return 3;
    if ( flags & AAFI_INTERPOL_BSPLINE )
        return 5;
    return 1; // default: quarter-sine
}

// Strip extension and append "-media" to build the embedded-essence extract dir.
inline std::string buildExtractDir(const char *filepath) {
    std::string p(filepath);
    if ( const auto dot = p.rfind('.'); dot != std::string::npos )
        p.resize(dot);
    p += "-media";
    return p;
}

// Map a file extension to the REAPER source type string.
inline const char *rppSourceTypeFromPath(const char *filePath) {
    auto srcType = "WAVE";
    if ( const char *ext = strrchr(filePath, '.') ) {
        if ( strcasecmp(ext, ".mp3") == 0 )
            srcType = "MP3";
        else if ( strcasecmp(ext, ".flac") == 0 )
            srcType = "FLAC";
        else if ( strcasecmp(ext, ".ogg") == 0 )
            srcType = "VORBIS";
        else if ( strcasecmp(ext, ".mxf") == 0 )
            srcType = "VIDEO";
    }
    return srcType;
}

// Compute the REAPER isDrop timecode flag from AAF timecode fields.
// Returns: 0 = integer fps, 1 = drop-frame, 2 = non-drop fractional
[[nodiscard]] inline uint8_t computeTimecodeIsDrop(const int fps, const int denominator,
                                                   const int drop) noexcept {
    if ( denominator == 1 )
        return 0; // integer fps — REAPER ignores drop for these
    if ( fps == 24 )
        return 2; // 23.976 is always non-drop
    return drop > 0 ? 1 : 2;
}

bool ensure_dir(const std::string &path);

void rlog(const char *fmt, ...);

#endif // REAPER_AAF_HELPERS_H
