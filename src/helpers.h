#ifndef REAPER_AAF_HELPERS_H
#define REAPER_AAF_HELPERS_H

#include <string>
#include "libaaf/AAFTypes.h"


inline double rational_to_double(const aafRational_t r) {
    if (r.denominator == 0) return 0.0;
    return static_cast<double>(r.numerator) / static_cast<double>(r.denominator);
}

// Convert a position in edit-rate units to seconds.
// editRate is a POINTER — may be null, returns 0 safely.
inline double pos_to_seconds(const aafPosition_t pos, const aafRational_t *editRate) {
    if (!editRate) return 0.0;
    const double er = rational_to_double(*editRate);
    if (er == 0.0) return 0.0;
    return static_cast<double>(pos) / er;
}

inline std::string escape_rpp_string(const char *raw) {
    if (!raw) return {};
    std::string out;
    out.reserve(strlen(raw) + 8);
    for (const char *p = raw; *p; ++p) {
        if (*p == '\\') {
            out += "\\\\";
            continue;
        }
        if (*p == '"') {
            out += "\\\"";
            continue;
        }
        out += *p;
    }
    return out;
}

inline double clamp_volume(double lin) {
    if (lin < 0.0) lin = 0.0;
    if (lin > 4.0) lin = 4.0;
    return lin;
}

inline double clamp_pan(double pan) {
    if (pan < -1.0) pan = -1.0;
    if (pan > 1.0) pan = 1.0;
    return pan;
}

inline int aafiColorToReaper(const uint16_t rgb[3]) {
    return 0x1000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
}


int interpol_to_reaper_shape(uint32_t flags);

bool ensure_dir(const std::string &path);

void rlog(const char *fmt, ...);

#endif //REAPER_AAF_HELPERS_H
