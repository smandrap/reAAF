#include "AafImporter.h"
#include "helpers.h"
#include "FadeResolver.h"

#include <libaaf.h>

// ---------------------------------------------------------------------------
// Constructor / run
// ---------------------------------------------------------------------------

AafImporter::AafImporter(ProjectStateContext *ctx, const char *filepath)
    : m_writer(ctx),
      m_aafi(aafi_alloc(nullptr)),
      m_extractDir(build_extract_dir(filepath)),
      m_filePath(filepath) {
}

int AafImporter::run() {
    if (!m_aafi) {
        rlog("ReAAF: aafi_alloc() failed\n");
        return -1;
    }

    aafi_set_debug(m_aafi, VERB_WARNING, 0, nullptr, nullptr, nullptr);

    aafi_set_option_int(m_aafi, "protools",
                        AAFI_PROTOOLS_OPT_REPLACE_CLIP_FADES |
                        AAFI_PROTOOLS_OPT_REMOVE_SAMPLE_ACCURATE_EDIT);

    {
        std::string dir(m_filePath);
        const auto sep = dir.find_last_of("/\\");
        if (sep != std::string::npos) dir.resize(sep);
        aafi_set_option_str(m_aafi, "media_location", dir.c_str());
    }

    if (aafi_load_file(m_aafi, m_filePath.c_str()) != 0) {
        rlog("ReAAF: failed to load '%s'\n", m_filePath.c_str());
        aafi_release(&m_aafi);
        return -1;
    }

    if (!ensure_dir(m_extractDir))
        rlog("ReAAF: WARNING: could not create '%s'\n", m_extractDir.c_str());

    const uint32_t samplerate =
            m_aafi->Audio->samplerate > 0 ? m_aafi->Audio->samplerate : 48000u;

    const double tcOffset = pos_to_seconds(m_aafi->compositionStart,
                                           m_aafi->compositionStart_editRate);

    int fps = 25;
    if (m_aafi->Timecode) fps = m_aafi->Timecode->fps;

    {
        // Guard destroyed at end of scope → emits closing ">" for REAPER_PROJECT
        auto proj = m_writer.project(tcOffset, fps, samplerate);

        writeMarkers();

        const aafiAudioTrack *track = nullptr;
        int trackIdx = 1;
        int itemCount = 1;
        AAFI_foreachAudioTrack(m_aafi, track) {
            writeTrack(track, trackIdx++, itemCount);
        }
    }

    aafi_release(&m_aafi);
    return 0;
}

// ---------------------------------------------------------------------------
// Track
// ---------------------------------------------------------------------------

void AafImporter::writeTrack(const aafiAudioTrack *track,
                             int /*trackIdx*/,
                             int &itemCounter) {
    const char *trackName = track->name ? track->name : "";

    // format == channel count; AAFI_TRACK_FORMAT_UNKNOWN (99) → mono
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

    // Fixed track pan: AAF 0..1 → REAPER −1...+1
    double pan = 0.0;
    if (track->pan
        && (track->pan->flags & AAFI_AUDIO_GAIN_CONSTANT)
        && track->pan->pts_cnt >= 1
        && track->pan->value) {
        pan = clamp_pan((rational_to_double(track->pan->value[0]) - 0.5) * 2.0);
    }

    // Guard destroyed at end of scope → emits closing ">" for TRACK
    auto t = m_writer.track(trackName, vol, pan,
                            track->mute != 0 ? 1 : 0,
                            track->solo != 0 ? 1 : 0,
                            nchan);

    // Track-level automation — composition length is the time base
    const double compLen = pos_to_seconds(m_aafi->compositionLength,
                                          m_aafi->compositionLength_editRate);

    if (track->gain && (track->gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) {
        writeEnvelope(track->gain, 0.0, compLen, "VOLENV2",
                      [](const double v) { return clamp_volume(v); });
    }

    if (track->pan && (track->pan->flags & AAFI_AUDIO_GAIN_VARIABLE)) {
        // AAF pan: 0=left, 0.5=center, 1=right → REAPER −1...+1 (negated)
        writeEnvelope(track->pan, 0.0, compLen, "PANENV2",
                      [](double v) { return clamp_pan((v - 0.5) * -2.0); },
                      /*arm=*/true);
    }

    const XFadeMap xFadeMap = buildXFadeMap(track);

    aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if (aafiAudioClip *clip = aafi_timelineItemToAudioClip(ti))
            writeItem(clip, ti, track->edit_rate, itemCounter++, xFadeMap);
        // AAFI_TRANS items are consumed via xFadeMap; nothing to emit directly.
    }
    // 't' destructor fires here → ">"
}

// ---------------------------------------------------------------------------
// Item
// ---------------------------------------------------------------------------

void AafImporter::writeItem(aafiAudioClip *clip,
                            const aafiTimelineItem *ti,
                            const aafRational_t *trackEditRate,
                            int /*itemIdx*/,
                            const XFadeMap &xFadeMap) {
    const double pos = pos_to_seconds(clip->pos, trackEditRate);
    const double len = pos_to_seconds(clip->len, trackEditRate);
    const double srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);

    // Fixed clip gain
    double gainLin = 1.0;
    if (clip->gain
        && (clip->gain->flags & AAFI_AUDIO_GAIN_CONSTANT)
        && clip->gain->pts_cnt >= 1
        && clip->gain->value) {
        gainLin = clamp_volume(rational_to_double(clip->gain->value[0]));
    }

    const auto [fadeInLen, fadeInShape] = resolveFadeIn(clip, ti, xFadeMap, trackEditRate);
    const auto [fadeOutLen, fadeOutShape] = resolveFadeOut(clip, ti, xFadeMap, trackEditRate);

    // Display name: prefer subClipName, fall back to essence file name
    const char *clipName = clip->subClipName;
    if (!clipName || clipName[0] == '\0') {
        if (clip->essencePointerList && clip->essencePointerList->essenceFile)
            clipName = clip->essencePointerList->essenceFile->name;
    }

    // Guard destroyed at end of scope → emits closing ">" for ITEM
    auto i = m_writer.item(clipName,
                           pos, len,
                           fadeInLen, fadeInShape,
                           fadeOutLen, fadeOutShape,
                           gainLin, srcOffset,
                           clip->mute != 0 ? 1 : 0);

    // Per-clip varying gain automation
    if (clip->automation && (clip->automation->flags & AAFI_AUDIO_GAIN_VARIABLE)) {
        writeEnvelope(clip->automation, pos, len, "VOLENV",
                      [](const double v) { return clamp_volume(v); });
    }

    writeSource(clip);
    // 'i' destructor fires here ">"
}

// ---------------------------------------------------------------------------
// Source
// ---------------------------------------------------------------------------

void AafImporter::writeSource(const aafiAudioClip *clip) {
    // Helper lambda: emit an EMPTY source and return.
    const auto emitEmpty = [this] {
        auto s = m_writer.source(nullptr, nullptr); // guard closes immediately
    };

    if (!clip->essencePointerList) {
        emitEmpty();
        return;
    }

    aafiAudioEssenceFile *ess = clip->essencePointerList->essenceFile;
    if (!ess) {
        emitEmpty();
        return;
    }

    // Extract embedded essence if not yet done
    if (ess->is_embedded && !ess->usable_file_path) {
        char *outPath = nullptr;
        const int rc = aafi_extractAudioEssenceFile(m_aafi, ess,
                                                    AAFI_EXTRACT_DEFAULT,
                                                    m_extractDir.c_str(),
                                                    0, 0, nullptr, &outPath);
        if (rc != 0)
            rlog("reaper_aaf: WARNING: failed to extract '%s'\n",
                 ess->unique_name ? ess->unique_name : "(unnamed)");
        free(outPath);
    }

    const char *filePath = ess->usable_file_path;
    if (!filePath || filePath[0] == '\0') {
        rlog("reaper_aaf: WARNING: no usable path for '%s'\n",
             ess->unique_name ? ess->unique_name : "(unnamed)");
        emitEmpty();
        return;
    }

    // Determine source type from extension
    const char *srcType = "WAVE";
    if (const char *ext = strrchr(filePath, '.')) {
        if (strcasecmp(ext, ".mp3") == 0) srcType = "MP3";
        else if (strcasecmp(ext, ".flac") == 0) srcType = "FLAC";
        else if (strcasecmp(ext, ".ogg") == 0) srcType = "VORBIS";
        // .wav / .aif / .aiff → WAVE (default)
    }

    auto s = m_writer.source(srcType, filePath);
    // 's' destructor fires here ">"
}

// ---------------------------------------------------------------------------
// Envelopes
// ---------------------------------------------------------------------------

void AafImporter::writeEnvelope(const aafiAudioGain *gain,
                                const double segStartSec,
                                const double segLenSec,
                                const char *tag,
                                const std::function<double(double)> &transform,
                                const bool arm) {
    if (!gain) return;
    if (!(gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (gain->pts_cnt == 0 || !gain->time || !gain->value) return;

    // Guard destroyed at end of scope → emits closing ">" for envelope
    auto env = m_writer.envelope(tag, arm);

    for (unsigned int i = 0; i < gain->pts_cnt; ++i) {
        const double frac = rational_to_double(gain->time[i]); // 0.0 .. 1.0
        const double t = segStartSec + frac * segLenSec;
        const double val = transform(rational_to_double(gain->value[i]));
        m_writer.writeEnvPoint(t, val);
    }
    // 'env' destructor fires here ">"
}

// ---------------------------------------------------------------------------
// Markers
// ---------------------------------------------------------------------------

void AafImporter::writeMarkers() const {
    int id = 1;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(m_aafi, m) {
        const double t = pos_to_seconds(m->start, m->edit_rate);

        if (m->length == 0) {
            m_writer.writeMarker(id++, t, m->name, false);
        } else {
            const double endT = pos_to_seconds(m->start + m->length, m->edit_rate);
            m_writer.writeMarker(id, t, m->name, true);
            m_writer.writeMarker(id + 1, endT, m->name, true);
            id += 2;
        }
    }
}
