#ifndef REAPER_AAF_RPPWRITER_H
#define REAPER_AAF_RPPWRITER_H

class ProjectStateContext;

// ---------------------------------------------------------------------------
// RppWriter
//
// Two-layer design:
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
//
// Chunks cannot be left open by accident: ">" is emitted in the guard's
// destructor, so early returns and exceptions are always safe.
// ---------------------------------------------------------------------------
class RppWriter {
public:
    explicit RppWriter(ProjectStateContext *ctx) : m_ctx(ctx) {
    }

    // Non-copyable — guards hold a reference back to the writer.
    RppWriter(const RppWriter &) = delete;

    RppWriter &operator=(const RppWriter &) = delete;

    // -----------------------------------------------------------------------
    // RAII chunk guard — base for all chunk types.
    // Emits the closing ">" on destruction.
    // -----------------------------------------------------------------------
    class Chunk {
    public:
        Chunk(Chunk &&other) noexcept
            : m_writer(other.m_writer), m_closed(other.m_closed) {
            other.m_closed = true; // moved-from guard must not double-close
        }

        ~Chunk() { close(); }

        // Explicit early close
        void close() {
            if (!m_closed) {
                m_writer.line(">");
                m_closed = true;
            }
        }

        Chunk(const Chunk &) = delete;

        Chunk &operator=(const Chunk &) = delete;

    protected:
        explicit Chunk(RppWriter &w) : m_writer(w), m_closed(false) {
        }

        RppWriter &m_writer;

    private:
        bool m_closed;
    };

    // -----------------------------------------------------------------------
    // Concrete chunk guard types.
    // Distinct types prevent accidentally using a TrackChunk where an
    // ItemChunk is expected — if you ever want to add chunk-specific helpers.
    // -----------------------------------------------------------------------
    struct ProjectChunk : Chunk {
        explicit ProjectChunk(RppWriter &w) : Chunk(w) {
        }
    };

    struct TrackChunk : Chunk {
        explicit TrackChunk(RppWriter &w) : Chunk(w) {
        }
    };

    struct ItemChunk : Chunk {
        explicit ItemChunk(RppWriter &w) : Chunk(w) {
        }
    };

    struct SourceChunk : Chunk {
        explicit SourceChunk(RppWriter &w) : Chunk(w) {
        }
    };

    struct EnvChunk : Chunk {
        explicit EnvChunk(RppWriter &w) : Chunk(w) {
        }
    };

    // -----------------------------------------------------------------------
    // Factory methods — open the chunk and return its RAII guard.
    // Mark [[nodiscard]] so callers can't silently discard the guard
    // (which would open AND immediately close the chunk on the same line).
    // -----------------------------------------------------------------------

    [[nodiscard]] ProjectChunk project(double tcOffsetSec, int fps, int isDrop, unsigned samplerate);

    [[nodiscard]] TrackChunk track(const char *name,
                                   double vol, double pan,
                                   int mute, int solo, int nchan);

    [[nodiscard]] ItemChunk item(const char *name,
                                 double posSec, double lenSec,
                                 double fadeInLen, int fadeInShape,
                                 double fadeOutLen, int fadeOutShape,
                                 double gainLin, double srcOffsSec,
                                 int mute);

    // source() is self-closing (FILE line + ">") — no inner content needed.
    // Returns a guard anyway for symmetry and in case callers need .close().
    [[nodiscard]] SourceChunk source(const char *type, const char *filePath);

    [[nodiscard]] EnvChunk envelope(const char *tag, bool arm = false);

    // -----------------------------------------------------------------------
    // Flat line-level writers (no matching close needed)
    // -----------------------------------------------------------------------
    void writeMarker(int id, double timeSec, const char *name, bool isRegionBoundary) const;

    void writeEnvPoint(double timeSec, double value) const;

private:
    ProjectStateContext *m_ctx;

    void line(const char *fmt, ...) const;
};

#endif // REAPER_AAF_RPPWRITER_H
