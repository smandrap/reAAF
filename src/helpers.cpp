#include "helpers.h"
#include "reaper_plugin_functions.h"
#include "libaaf/AAFIface.h"

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define PATH_SEP '\\'
#else
#  include <unistd.h>
#  define PATH_SEP '/'
#endif

// Map AAFInterpolation flags to REAPER fade shape index.
// REAPER: 0=linear, 1=quarter-sine, 2=equal power, 3=slow start, 4=fast start, 5=bezier
int interpol_to_reaper_shape(const uint32_t flags) {
    if (flags & AAFI_INTERPOL_LINEAR) return 0;
    if (flags & AAFI_INTERPOL_POWER) return 4;
    if (flags & AAFI_INTERPOL_LOG) return 3;
    if (flags & AAFI_INTERPOL_BSPLINE) return 5;
    return 1; // default: quarter-sine
}

std::string build_extract_dir(const char *aaf_path) {
    std::string p(aaf_path);
    if (const auto dot = p.rfind('.'); dot != std::string::npos) p.resize(dot);
    p += "-media";
    return p;
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

