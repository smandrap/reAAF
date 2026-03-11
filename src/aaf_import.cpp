#include "aaf_import.h"
#include "reaper_plugin_functions.h"

#include <libaaf.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <sys/stat.h>
#include <cerrno>

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define PATH_SEP '\\'
#else
#  include <unistd.h>
#  define PATH_SEP '/'
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline double rational_to_double(aafRational_t r) {
    if (r.denominator == 0) return 0.0;
    return static_cast<double>(r.numerator) / static_cast<double>(r.denominator);
}

// Convert a position in edit-rate units to seconds.
// editRate is a POINTER — may be null, returns 0 safely.
static inline double pos_to_seconds(const aafPosition_t pos, const aafRational_t *editRate) {
    if (!editRate) return 0.0;
    const double er = rational_to_double(*editRate);
    if (er == 0.0) return 0.0;
    return static_cast<double>(pos) / er;
}

static std::string escape_rpp_string(const char *raw) {
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

static inline double clamp_volume(double lin) {
    if (lin < 0.0) lin = 0.0;
    if (lin > 4.0) lin = 4.0;
    return lin;
}

static inline double clamp_pan(double pan) {
    if (pan < -1.0) pan = -1.0;
    if (pan > 1.0) pan = 1.0;
    return pan;
}

// Map AAFInterpolation flags to REAPER fade shape index.
// REAPER: 0=linear, 1=quarter-sine, 2=equal power, 3=slow start, 4=fast start, 5=bezier
static int interpol_to_reaper_shape(uint32_t flags) {
    if (flags & AAFI_INTERPOL_LINEAR) return 0;
    if (flags & AAFI_INTERPOL_POWER) return 4;
    if (flags & AAFI_INTERPOL_LOG) return 3;
    if (flags & AAFI_INTERPOL_BSPLINE) return 5;
    return 1; // default: quarter-sine
}

static std::string build_extract_dir(const char *aaf_path) {
    std::string p(aaf_path);
    auto dot = p.rfind('.');
    if (dot != std::string::npos) p.resize(dot);
    p += "-media";
    return p;
}

static bool ensure_dir(const std::string &path) {
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0 || errno == EEXIST) return true;
#else
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) return true;
#endif
    return false;
}

static void rlog(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ShowConsoleMsg(buf);
}

// ---------------------------------------------------------------------------
// RPP writer
// ---------------------------------------------------------------------------
struct RppWriter {
    ProjectStateContext *ctx;

    void line(const char *fmt, ...) const {
        char buf[8192];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        ctx->AddLine("%s", buf);
    }
};

// ---------------------------------------------------------------------------
// Automation envelopes
//
// aafiAudioGain (from AAFIface.h):
//   uint32_t       flags;    — AAFI_AUDIO_GAIN_CONSTANT or AAFI_AUDIO_GAIN_VARIABLE
//   unsigned int   pts_cnt;  — number of control points
//   aafRational_t *time;     — array[pts_cnt]: 0.0–1.0 fraction of segment duration
//   aafRational_t *value;    — array[pts_cnt]: amplitude multipliers
//
// gain->time[i] is a 0.0–1.0 fraction of the enclosing segment's duration.
// Absolute seconds = seg_start_sec + time[i] * seg_len_sec
// ---------------------------------------------------------------------------

static void write_volume_envelope(const RppWriter &w,
                                  const aafiAudioGain *gain,
                                  double seg_start_sec,
                                  double seg_len_sec,
                                  const char *envTag) {
    if (!gain) return;
    if (!(gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (gain->pts_cnt == 0 || !gain->time || !gain->value) return;

    w.line("<%s", envTag);
    // w.line("ACT 0 -1");
    w.line("VIS 1 1 1");

    for (unsigned int i = 0; i < gain->pts_cnt; ++i) {
        const double frac = rational_to_double(gain->time[i]); // 0.0 .. 1.0
        const double t = seg_start_sec + frac * seg_len_sec;
        const double val = clamp_volume(rational_to_double(gain->value[i]));
        w.line("PT %.10f %.10f 0", t, val);
    }

    w.line(">");
}

static void write_pan_envelope(const RppWriter &w,
                               const aafiAudioGain *pan,
                               const double seg_start_sec,
                               const double seg_len_sec) {
    // if (!pan) return;
    if (!(pan->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (pan->pts_cnt == 0 || !pan->time || !pan->value) return;

    // AAF pan: 0=left, 0.5=centre, 1=right → REAPER: -1=left, 0=centre, +1=right
    w.line("<PANENV2");
    // w.line("ACT 1 -1"); //Uncomment to always active
    w.line("VIS 1 1 1");
    w.line("ARM 1");

    for (unsigned int i = 0; i < pan->pts_cnt; ++i) {
        const double frac = rational_to_double(pan->time[i]);
        const double t = seg_start_sec + frac * seg_len_sec;
        const double aafPan = rational_to_double(pan->value[i]);
        const double rPan = clamp_pan((aafPan - 0.5) * -2.0); // multiply negative otherwise panning is reversed
        w.line("PT %.10f %.10f 0", t, rPan);
    }

    w.line(">");
}

// ---------------------------------------------------------------------------
// Markers
//
// aafiMarker (from AAFIface.h):
//   aafPosition_t  start
//   aafPosition_t  length     — > 0 means region
//   aafRational_t *edit_rate  — POINTER
//   char          *name, *comment
//   uint16_t       RGBColor[3]
// ---------------------------------------------------------------------------
static void write_markers(const RppWriter &w, const AAF_Iface *aafi) {
    int id = 1;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(aafi, m) {
        // edit_rate is a pointer
        const double t = pos_to_seconds(m->start, m->edit_rate);

        if (const bool isRegion = (m->length > 0); !isRegion) {
            w.line("MARKER %d %.10f \"%s\" 0 0 1",
                   id++, t,
                   m->name ? escape_rpp_string(m->name).c_str() : "");
        } else {
            const double end = pos_to_seconds(m->start + m->length, m->edit_rate);
            w.line("MARKER %d %.10f \"%s\" 0 1 1",
                   id, t, m->name ? escape_rpp_string(m->name).c_str() : "");
            w.line("MARKER %d %.10f \"%s\" 0 1 1",
                   id + 1, end, m->name ? escape_rpp_string(m->name).c_str() : "");
            id += 2;
        }
    }
}

// ---------------------------------------------------------------------------
// Source block
// ---------------------------------------------------------------------------
static void write_source(const RppWriter &w,
                         const aafiAudioClip *clip,
                         const std::string &extractDir,
                         AAF_Iface *aafi) {
    if (!clip->essencePointerList) {
        w.line("<SOURCE EMPTY");
        w.line(">");
        return;
    }

    aafiAudioEssenceFile *ess = clip->essencePointerList->essenceFile;
    if (!ess) {
        w.line("<SOURCE EMPTY");
        w.line(">");
        return;
    }

    // Extract embedded essence if not yet done.
    // aafi_extractAudioEssenceFile() sets ess->usable_file_path on success.

    if (ess->is_embedded && !ess->usable_file_path) {
        char *outPath = nullptr;
        const int rc = aafi_extractAudioEssenceFile(aafi, ess,
                                                    AAFI_EXTRACT_DEFAULT,
                                                    extractDir.c_str(),
                                                    0, 0,
                                                    nullptr,
                                                    &outPath);

        if (rc != 0)
            rlog("reaper_aaf: WARNING: failed to extract '%s'\n",
                 ess->unique_name ? ess->unique_name : "(unnamed)");
        free(outPath);
    }

    const char *filePath = ess->usable_file_path;

    if (!filePath || filePath[0] == '\0') {
        rlog("reaper_aaf: WARNING: no usable path for '%s'\n",
             ess->unique_name ? ess->unique_name : "(unnamed)");
        w.line("<SOURCE EMPTY");
        w.line(">");
        return;
    }

    const char *srcType = "WAVE";
    if (const char *ext = strrchr(filePath, '.')) {
        if (strcasecmp(ext, ".mp3") == 0) srcType = "MP3";
        else if (strcasecmp(ext, ".flac") == 0) srcType = "FLAC";
        else if (strcasecmp(ext, ".ogg") == 0) srcType = "VORBIS";
        // .wav / .aif / .aiff → WAVE (default)
    }

    w.line("<SOURCE %s", srcType);
    w.line("FILE \"%s\"", escape_rpp_string(filePath).c_str());
    w.line(">");
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Crossfade lookup
//
// aafi_getFadeIn / aafi_getFadeOut only find AAFI_TRANS_FADE_IN /
// AAFI_TRANS_FADE_OUT transitions — they do NOT return AAFI_TRANS_XFADE
// items. A xfade sits between two clips in the linked list; neither
// flanking clip returns it via the fade helpers.
//
// We solve this with a pre-pass over the track's timeline items:
// for every AAFI_TRANS_XFADE we record it keyed by the timelineItem*
// of both its predecessor (fade-out side) and its successor (fade-in side).
// write_item then merges the xfade data with any plain fade-in/out.
// ---------------------------------------------------------------------------
// Per-clip xfade record: a clip can have an xfade on BOTH sides simultaneously
// (it is the outgoing clip of one xfade AND the incoming clip of the next).
// Storing a single value per clip key would silently overwrite one side.
struct ClipXfades {
    const aafiTransition *fadeIn = nullptr; // xfade where this clip is the incoming side
    const aafiTransition *fadeOut = nullptr; // xfade where this clip is the outgoing side
};

using XfadeMap = std::unordered_map<const aafiTimelineItem *, ClipXfades>;

static XfadeMap build_xfade_map(const aafiAudioTrack *track) {
    XfadeMap m;
    aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        const aafiTransition *xf = aafi_timelineItemToCrossFade(ti);
        if (!xf) continue;
        if (!(xf->flags & AAFI_TRANS_XFADE)) continue;

        // Use separate fields so a clip between two xfades keeps both.
        if (ti->prev) m[ti->prev].fadeOut = xf;
        if (ti->next) m[ti->next].fadeIn = xf;
    }
    return m;
}

// ---------------------------------------------------------------------------
// Item (clip)
//
// aafiAudioClip (from AAFIface.h):
//   aafPosition_t  pos            — track edit_rate units
//   aafPosition_t  len            — track edit_rate units
//   aafPosition_t  essence_offset — track edit_rate units (SourceClip::StartTime)
//   aafiAudioGain *gain           — fixed scalar (CONSTANT), may be NULL
//   aafiAudioGain *automation     — varying curve (VARIABLE), may be NULL
//   int            mute
//   char          *subClipName    — clip label (often NULL)
//
// aafiAudioTrack.edit_rate is a POINTER — pass it through as such.
// ---------------------------------------------------------------------------
static void write_item(const RppWriter &w,
                       aafiAudioClip *clip,
                       const aafiTimelineItem *ti,
                       const aafRational_t *trackEditRate,
                       const std::string &extractDir,
                       AAF_Iface *aafi,
                       int itemIdx,
                       const XfadeMap &xfadeMap) {
    const double pos = pos_to_seconds(clip->pos, trackEditRate);
    const double len = pos_to_seconds(clip->len, trackEditRate);

    // essence_offset is in the same track edit-rate units (per AAFIface.h comment)
    const double srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);

    // Fixed clip gain
    double gain_lin = 1.0;
    if (clip->gain
        && (clip->gain->flags & AAFI_AUDIO_GAIN_CONSTANT)
        && clip->gain->pts_cnt >= 1
        && clip->gain->value) {
        gain_lin = clamp_volume(rational_to_double(clip->gain->value[0]));
    }

    // --- Fade-in ---
    // Priority: plain AAFI_TRANS_FADE_IN on this clip, else xfade incoming side.
    const aafiTransition *fadein = aafi_getFadeIn(clip);
    double fadeInLen = 0.0;
    int fadeInShape = 1;

    if (fadein) {
        fadeInLen = pos_to_seconds(fadein->len, trackEditRate);
        fadeInShape = interpol_to_reaper_shape(fadein->flags);
    } else {
        if (const auto it = xfadeMap.find(ti); it != xfadeMap.end() && it->second.fadeIn) {
            const aafiTransition *xf = it->second.fadeIn;
            fadeInLen = pos_to_seconds(xf->len, trackEditRate);
            fadeInShape = interpol_to_reaper_shape(xf->flags);
        }
    }

    // --- Fade-out ---
    // Priority: plain AAFI_TRANS_FADE_OUT on this clip, else xfade outgoing side.
    const aafiTransition *fadeout = aafi_getFadeOut(clip);
    double fadeOutLen = 0.0;
    int fadeOutShape = 1;

    if (fadeout) {
        fadeOutLen = pos_to_seconds(fadeout->len, trackEditRate);
        fadeOutShape = interpol_to_reaper_shape(fadeout->flags);
    } else {
        if (const auto it = xfadeMap.find(ti); it != xfadeMap.end() && it->second.fadeOut) {
            const aafiTransition *xf = it->second.fadeOut;
            fadeOutLen = pos_to_seconds(xf->len, trackEditRate);
            fadeOutShape = interpol_to_reaper_shape(xf->flags);
        }
    }

    // Display name: subClipName (rarely set), else essence name
    const char *clipName = clip->subClipName;
    if (!clipName || clipName[0] == '\0') {
        if (clip->essencePointerList && clip->essencePointerList->essenceFile)
            clipName = clip->essencePointerList->essenceFile->name;
    }

    w.line("<ITEM");
    w.line("POSITION %.10f", pos);
    w.line("LENGTH %.10f", len);
    w.line("FADEIN %d %.10f 0", fadeInShape, fadeInLen);
    w.line("FADEOUT %d %.10f 0", fadeOutShape, fadeOutLen);
    w.line("MUTE %d 0", clip->mute != 0 ? 1 : 0);
    w.line("NAME \"%s\"", clipName ? escape_rpp_string(clipName).c_str() : "");
    w.line("VOLPAN %.6f 0.000000 1.000000 -1", gain_lin);
    w.line("SOFFS %.10f", srcOffset);

    // Per-clip varying gain automation (clip->automation, not clip->gain)
    if (clip->automation && (clip->automation->flags & AAFI_AUDIO_GAIN_VARIABLE))
        write_volume_envelope(w, clip->automation, pos, len, "VOLENV");

    w.line("NAME \"%s\"", clipName ? escape_rpp_string(clipName).c_str() : "");

    write_source(w, clip, extractDir, aafi);

    w.line(">"); // </ITEM>
}

// ---------------------------------------------------------------------------
// Track
//
// aafiAudioTrack (from AAFIface.h):
//   uint32_t        number
//   uint16_t        format     — channel count (aafiTrackFormat_e values match channels)
//   char           *name
//   aafiAudioGain  *gain       — track volume fader, may be NULL
//   aafiAudioPan   *pan        — track pan, may be NULL (same struct as aafiAudioGain)
//   char            solo, mute — char
//   aafRational_t  *edit_rate  — POINTER
//   No color field exists on aafiAudioTrack.
// ---------------------------------------------------------------------------
static void write_track(const RppWriter &w,
                        const aafiAudioTrack *track,
                        const std::string &extractDir,
                        AAF_Iface *aafi,
                        const int trackIdx,
                        int &itemCounter) {
    const char *trackName = track->name ? track->name : "";

    // format value equals channel count for standard formats;
    // AAFI_TRACK_FORMAT_UNKNOWN = 99, treat as mono
    int nchan = track->format;
    if (nchan <= 0 || nchan == 99) nchan = 1;

    // Fixed track volume
    double vol = 1.0;
    if (track->gain
        && (track->gain->flags & AAFI_AUDIO_GAIN_CONSTANT)
        && track->gain->pts_cnt >= 1
        && track->gain->value) {
        vol = clamp_volume(rational_to_double(track->gain->value[0]));
    }


    // Fixed track pan: AAF 0..1 → REAPER -1..+1
    double pan = 0.0;
    if (track->pan
        && (track->pan->flags & AAFI_AUDIO_GAIN_CONSTANT)
        && track->pan->pts_cnt >= 1
        && track->pan->value) {
        pan = clamp_pan((rational_to_double(track->pan->value[0]) - 0.5) * 2.0);
    }
    w.line("<TRACK");
    w.line("NAME \"%s\"", escape_rpp_string(trackName).c_str());
    w.line("VOLPAN %.6f %.6f -1 -1 1", vol, pan);
    w.line("MUTESOLO %d %d 0", track->mute != 0 ? 1 : 0, track->solo != 0 ? 1 : 0);
    w.line("NCHAN %d", nchan);

    // Track-level automation: use composition length as the timescale
    const double compLen = pos_to_seconds(aafi->compositionLength,
                                    aafi->compositionLength_editRate);

    if (track->gain && (track->gain->flags & AAFI_AUDIO_GAIN_VARIABLE))
        write_volume_envelope(w, track->gain, 0.0, compLen, "VOLENV2");

    if (track->pan && (track->pan->flags & AAFI_AUDIO_GAIN_VARIABLE))
        write_pan_envelope(w, track->pan, 0.0, compLen);

    // Pre-pass: map each clip timelineItem* to its adjacent xfade (if any).
    // aafi_getFadeIn/getFadeOut do NOT return AAFI_TRANS_XFADE items, so we
    // must find them ourselves and supply the data to write_item.
    const XfadeMap xfadeMap = build_xfade_map(track);

    // Items — track->edit_rate is a pointer, pass it directly
    aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if (aafiAudioClip *clip = aafi_timelineItemToAudioClip(ti))
            write_item(w, clip, ti, track->edit_rate, extractDir, aafi,
                       itemCounter++, xfadeMap);
        // AAFI_TRANS items are consumed via xfadeMap; nothing to emit for them.
    }

    w.line(">"); // </TRACK>
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------
int ImportAAF(const char *filepath, ProjectStateContext *ctx) {
    if (!filepath || !ctx) return -1;

    AAF_Iface *aafi = aafi_alloc(nullptr);
    if (!aafi) {
        rlog("ReAAF: aafi_alloc() failed\n");
        return -1;
    }

    aafi_set_debug(aafi, VERB_WARNING, 0, nullptr, nullptr, nullptr);

    aafi_set_option_int(aafi, "protools",
                        AAFI_PROTOOLS_OPT_REPLACE_CLIP_FADES |
                        AAFI_PROTOOLS_OPT_REMOVE_SAMPLE_ACCURATE_EDIT);

    // Point LibAAF at the AAF's directory for external essence lookup
    {
        std::string dir(filepath);
        auto sep = dir.find_last_of("/\\");
        if (sep != std::string::npos) dir.resize(sep);
        aafi_set_option_str(aafi, "media_location", dir.c_str());
    }

    if (aafi_load_file(aafi, filepath) != 0) {
        rlog("ReAAF: failed to load '%s'\n", filepath);
        aafi_release(&aafi);
        return -1;
    }

    const std::string extractDir = build_extract_dir(filepath);
    if (!ensure_dir(extractDir))
        rlog("ReAAF: WARNING: could not create '%s'\n", extractDir.c_str());

    // ---- Project-level values ----
    int samplerate = aafi->Audio->samplerate;
    if (samplerate <= 0) samplerate = 48000;

    // compositionStart_editRate and compositionLength_editRate are POINTERS
    const double tcOffset = pos_to_seconds(aafi->compositionStart,
                                           aafi->compositionStart_editRate);

    int fps = 25;
    if (aafi->Timecode) fps = aafi->Timecode->fps;

    const RppWriter w{ctx};

    // ---- Project header ----
    w.line("<REAPER_PROJECT 0.1");
    w.line("PROJOFFS %.10f 0 0", tcOffset);
    w.line("TIMEMODE 1 5 -1 %d 0 0 -1", fps);
    w.line("SMPTESYNC 0 %d 100 40 1000 300 0 0 0 0 0", fps);
    w.line("SAMPLERATE %d 0 0", samplerate);


    // ---- Markers ----
    write_markers(w, aafi);

    // ---- Tracks ----
    const aafiAudioTrack *audioTrack = nullptr;
    int trackIdx = 1;
    int itemCount = 1;

    AAFI_foreachAudioTrack(aafi, audioTrack) {
        write_track(w, audioTrack, extractDir, aafi, trackIdx++, itemCount);
    }

    w.line(">"); // </REAPER_PROJECT>

    aafi_release(&aafi);

    // rlog("ReAAF: import complete — %d track(s), %d item(s)\n",
    //      trackIdx - 1, itemCount - 1);

    return 0;
}
