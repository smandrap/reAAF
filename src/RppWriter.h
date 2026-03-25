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

#ifndef REAPER_AAF_RPPWRITER_H
#define REAPER_AAF_RPPWRITER_H

#include "IRppSink.h"

// ---------------------------------------------------------------------------
// RppWriter
//
//   - line()        : private raw emitter — all RPP syntax lives here
//   - Chunk guards  : public nested RAII types, one per chunk kind
//
// Usage:
//   {
//       auto t = m_writer.track("Drums", vol, pan, 0, 0, 2);
//       {
//           auto i = m_writer.item(...);
//           m_writer.source("WAVE", "/audio/kick.wav");  // self-closing
//       }  // <-- ">" emitted here for ITEM
//   }  // <-- ">" emitted here for TRACK
// ---------------------------------------------------------------------------
class RppWriter {
  public:
    explicit RppWriter(IRppSink *sink) : m_sink(sink) {}

    // Non-copyable — guards hold a reference back to the writer.
    RppWriter(const RppWriter &) = delete;

    RppWriter &operator=(const RppWriter &) = delete;

    // -----------------------------------------------------------------------
    // RAII chunk guard — emits the closing ">" on destruction.
    // Only RppWriter may construct; all factory methods return Chunk.
    // -----------------------------------------------------------------------
    class Chunk {
        friend class RppWriter;

      public:
        Chunk(Chunk &&other) noexcept : m_writer(other.m_writer), m_closed(other.m_closed) {
            other.m_closed = true; // moved-from guard must not double-close
        }

        ~Chunk() { close(); }

        // Explicit early close
        void close() {
            if ( !m_closed ) {
                m_writer.line(">");
                m_closed = true;
            }
        }

        Chunk(const Chunk &) = delete;

        Chunk &operator=(const Chunk &) = delete;

      private:
        explicit Chunk(RppWriter &w) : m_writer(w), m_closed(false) {}

        RppWriter &m_writer;
        bool m_closed;
    };

    // -----------------------------------------------------------------------
    // Factory methods — open the chunk and return its RAII guard.
    // Mark [[nodiscard]] so callers can't silently discard the guard
    // (which would open AND immediately close the chunk on the same line).
    // -----------------------------------------------------------------------

    [[nodiscard]] Chunk project(double tcOffsetSec, double maxProjLen, int fps, int isDrop,
                                unsigned samplerate);

    [[nodiscard]] Chunk track(const char *name, double vol, double pan, int mute, int solo,
                              int nchan);

    [[nodiscard]] Chunk item(const char *name, double posSec, double lenSec, double fadeInLen,
                             int fadeInShape, double fadeOutLen, int fadeOutShape, double gainLin,
                             double srcOffsSec, int mute);

    // source() is self-closing (FILE line + ">") — no inner content needed.
    // Returns a guard anyway for symmetry and in case callers need .close().
    [[nodiscard]] Chunk source(const char *type, const char *filePath);

    [[nodiscard]] Chunk envelope(const char *tag, bool arm = false);

    // this can be discarded, it's a clean <SOURCE EMPTY> block
    Chunk emptySource();


    // -----------------------------------------------------------------------
    // Flat line-level writers (no matching close needed)
    // -----------------------------------------------------------------------
    void writeMarker(int id, double timeSec, const char *name, bool isRegionBoundary,
                     int color) const;

    void writeEnvPoint(double timeSec, double value) const;

  private:
    IRppSink *m_sink; // not owned — lifetime tied to caller

    void line(const char *fmt, ...) const;
};

#endif // REAPER_AAF_RPPWRITER_H
