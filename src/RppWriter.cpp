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


void RppWriter::line(const char *fmt, ...) const {
    char buf[8192];
    std::va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    m_ctx->AddLine("%s", buf);
}


RppWriter::ProjectChunk RppWriter::project(const double tcOffsetSec, const int fps, const int isDrop,
                                           const unsigned samplerate) {
    line("<REAPER_PROJECT 0.1");
    line("PROJOFFS %.10f 0 0", tcOffsetSec);
    line("TIMEMODE 1 5 -1 %d %d 0 -1", fps, isDrop);
    line("SMPTESYNC 0 %d 100 40 1000 300 0 0 0 0 0", fps);
    line("SAMPLERATE %u 0 0", samplerate);
    return ProjectChunk{*this};
}


RppWriter::TrackChunk RppWriter::track(const char *name, const double vol, const double pan,
                                       const int mute, const int solo, const int nchan) {
    line("<TRACK");
    line("NAME \"%s\"", name ? escape_rpp_string(name).c_str() : "");
    line("VOLPAN %.6f %.6f -1 -1 1", vol, pan);
    line("MUTESOLO %d %d 0", mute, solo);
    line("NCHAN %d", nchan);
    return TrackChunk{*this};
}


RppWriter::ItemChunk RppWriter::item(const char *name,
                                     const double posSec, const double lenSec,
                                     const double fadeInLen, const int fadeInShape,
                                     const double fadeOutLen, const int fadeOutShape,
                                     const double gainLin, const double srcOffsSec,
                                     const int mute) {
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
    return ItemChunk{*this};
}

RppWriter::SourceChunk RppWriter::source(const char *type, const char *filePath) {
    assert(type && filePath && filePath != '\0');
    line("<SOURCE %s", type);
    line("FILE \"%s\"", escape_rpp_string(filePath).c_str());
    return SourceChunk{*this};
}

RppWriter::SourceChunk RppWriter::emptySource() {
    line("<SOURCE EMPTY");
    return SourceChunk{*this};
}


RppWriter::EnvChunk RppWriter::envelope(const char *tag, const bool arm) {
    line("<%s", tag);
    line("VIS 1 1 1");
    if (arm) line("ARM 1");
    return EnvChunk{*this};
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

