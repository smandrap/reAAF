#include "RppWriter.h"
#include <cstdarg>
#include "reaper_plugin_functions.h"

void RppWriter::line(const char *fmt, ...) const {
    char buf[8192];
    std::va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ctx->AddLine("%s", buf);
}

