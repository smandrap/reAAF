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

#include "RppWriter.h"

#include <cassert>

#include "helpers.h"
#include "reaper_plugin_functions.h"

#include <cstdarg>
#include <string>


void RppWriter::line(const char *fmt, ...) const {
    char buf[8192];
    std::va_list ap;
    va_start(ap, fmt);
    std::va_list ap2;
    va_copy(ap2, ap);
    const int written = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (written >= 0 && written < static_cast<int>(sizeof(buf))) {
        va_end(ap2);
        m_ctx->AddLine("%s", buf);
        return;
    }

    if (written >= static_cast<int>(sizeof(buf))) {
        std::string large(static_cast<size_t>(written), '\0');
        vsnprintf(large.data(), static_cast<size_t>(written) + 1, fmt, ap2);
        va_end(ap2);
        m_ctx->AddLine("%s", large.c_str());
        return;
    }

    va_end(ap2);
    if (m_onError) {
        char errmsg[128];
        snprintf(errmsg, sizeof(errmsg), "line formatting failed (vsnprintf returned %d)", written);
        m_onError(ErrorKind::LineTruncated, errmsg);
    }
}


auto RppWriter::project(const double tcOffsetSec, const double maxProjLen,const int fps, const int isDrop,
                        const unsigned samplerate) -> Chunk {
    line("<REAPER_PROJECT 0.1");
    line("PROJOFFS %.10f 0 0", tcOffsetSec);
    line("MAXPROJLEN 0 %.10f", maxProjLen + 60); // Limit project length 1 min after composition length
    line("TIMEMODE 1 5 -1 %d %d 0 -1", fps, isDrop);
    line("SMPTESYNC 0 %d 100 40 1000 300 0 0 0 0 0", fps);
    line("SAMPLERATE %u 0 0", samplerate);
    return Chunk{*this};
}


auto RppWriter::track(const char *name, const double vol, const double pan,
                      const int mute, const int solo, const int nchan) -> Chunk {
    line("<TRACK");
    line("NAME \"%s\"", name ? escape_rpp_string(name).c_str() : "");
    line("VOLPAN %.6f %.6f -1 -1 1", vol, pan);
    line("MUTESOLO %d %d 0", mute, solo);
    line("NCHAN %d", nchan);
    return Chunk{*this};
}


auto RppWriter::item(const char *name,
                     const double posSec, const double lenSec,
                     const double fadeInLen, const int fadeInShape,
                     const double fadeOutLen, const int fadeOutShape,
                     const double gainLin, const double srcOffsSec,
                     const int mute) -> Chunk {
    line("<ITEM");
    line("POSITION %.10f", posSec);
    line("LENGTH %.10f", lenSec);
    line("FADEIN %d %.10f 0", fadeInShape, fadeInLen);
    line("FADEOUT %d %.10f 0", fadeOutShape, fadeOutLen);
    line("MUTE %d 0", mute);
    line("NAME \"%s\"", name ? escape_rpp_string(name).c_str() : "");
    line("VOLPAN %.6f 0.000000 1.000000 -1", gainLin);
    line("SOFFS %.10f", srcOffsSec);
    line("LOOP 0");
    return Chunk{*this};
}

auto RppWriter::source(const char *type, const char *filePath) -> Chunk {
    assert(type && filePath && *filePath != '\0');
    line("<SOURCE %s", type);
    line("FILE \"%s\"", escape_rpp_string(filePath).c_str());
    return Chunk{*this};
}

auto RppWriter::emptySource() -> Chunk {
    line("<SOURCE EMPTY");
    return Chunk{*this};
}


auto RppWriter::envelope(const char *tag, const bool arm) -> Chunk {
    line("<%s", tag);
    line("VIS 1 1 1");
    if (arm) line("ARM 1");
    return Chunk{*this};
}

void RppWriter::writeMarker(const int id, const double timeSec, const char *name, const bool isRegionBoundary,
                            const int color) const {
    line("MARKER %d %.10f \"%s\" %d %d 1",
         id, timeSec,
         name ? escape_rpp_string(name).c_str() : "",
         isRegionBoundary ? 1 : 0,
         color);
}

void RppWriter::writeEnvPoint(const double timeSec, const double value) const {
    line("PT %.10f %.10f 0", timeSec, value);
}

