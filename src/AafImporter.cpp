#include "AafImporter.h"
#include "helpers.h"
#include "FadeResolver.h"

#include <libaaf.h>

AafImporter::AafImporter(ProjectStateContext *ctx, const char *filepath) : m_writer{ctx}, m_aafi(aafi_alloc(nullptr)),
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

    const uint32_t samplerate = m_aafi->Audio->samplerate > 0 ? m_aafi->Audio->samplerate : 48000u;

    const double tcOffset = pos_to_seconds(m_aafi->compositionStart,
                                           m_aafi->compositionStart_editRate);

    int fps = 25;
    if (m_aafi->Timecode) fps = m_aafi->Timecode->fps;

    m_writer.line("<REAPER_PROJECT 0.1");
    m_writer.line("PROJOFFS %.10f 0 0", tcOffset);
    m_writer.line("TIMEMODE 1 5 -1 %d 0 0 -1", fps);
    m_writer.line("SMPTESYNC 0 %d 100 40 1000 300 0 0 0 0 0", fps);
    m_writer.line("SAMPLERATE %u 0 0", samplerate);

    writeMarkers();

    const aafiAudioTrack *track = nullptr;
    int trackIdx = 1;
    int itemCount = 1;
    AAFI_foreachAudioTrack(m_aafi, track) {
        writeTrack(track, trackIdx++, itemCount);
    }

    m_writer.line(">"); // </REAPER_PROJECT>

    aafi_release(&m_aafi);
    return 0;
}

void AafImporter::writeTrack(const aafiAudioTrack *track, int trackIdx, int &itemCounter) const {
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

    // Fixed track pan: AAF 0..1 → REAPER -1...+1
    double pan = 0.0;
    if (track->pan
        && (track->pan->flags & AAFI_AUDIO_GAIN_CONSTANT)
        && track->pan->pts_cnt >= 1
        && track->pan->value) {
        pan = clamp_pan((rational_to_double(track->pan->value[0]) - 0.5) * 2.0);
    }
    m_writer.line("<TRACK");
    m_writer.line("NAME \"%s\"", escape_rpp_string(trackName).c_str());
    m_writer.line("VOLPAN %.6f %.6f -1 -1 1", vol, pan);
    m_writer.line("MUTESOLO %d %d 0", track->mute != 0 ? 1 : 0, track->solo != 0 ? 1 : 0);
    m_writer.line("NCHAN %d", nchan);

    // Track-level automation: use composition length as the timescale
    const double compLen = pos_to_seconds(m_aafi->compositionLength,
                                          m_aafi->compositionLength_editRate);

    if (track->gain && (track->gain->flags & AAFI_AUDIO_GAIN_VARIABLE))
        writeVolEnvelope(track->gain, 0.0, compLen, "VOLENV2");

    if (track->pan && (track->pan->flags & AAFI_AUDIO_GAIN_VARIABLE))
        writePanEnvelope(track->pan, 0.0, compLen);

    // Pre-pass: map each clip timelineItem* to its adjacent xfade (if any).
    // aafi_getFadeIn/getFadeOut do NOT return AAFI_TRANS_XFADE items, so we
    // must find them ourselves and supply the data to write_item.
    const XFadeMap xFadeMap = buildXFadeMap(track);

    // Items — track->edit_rate is a pointer, pass it directly
    aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if (aafiAudioClip *clip = aafi_timelineItemToAudioClip(ti))
            writeItem(clip, ti, track->edit_rate, itemCounter++, xFadeMap);
        // AAFI_TRANS items are consumed via xfadeMap; nothing to emit for them.
    }

    m_writer.line(">"); // </TRACK>
}

void AafImporter::writeItem(aafiAudioClip *clip, const aafiTimelineItem *ti, const aafRational_t *trackEditRate,
                            int itemIdx, const XFadeMap &xFadeMap) const {
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

    const auto [fadeInLen, fadeInShape]   = resolveFadeIn (clip, ti, xFadeMap, trackEditRate);
    const auto [fadeOutLen, fadeOutShape] = resolveFadeOut(clip, ti, xFadeMap, trackEditRate);

    // Display name: subClipName (rarely set), else essence name
    const char *clipName = clip->subClipName;
    if (!clipName || clipName[0] == '\0') {
        if (clip->essencePointerList && clip->essencePointerList->essenceFile)
            clipName = clip->essencePointerList->essenceFile->name;
    }

    m_writer.line("<ITEM");
    m_writer.line("POSITION %.10f", pos);
    m_writer.line("LENGTH %.10f", len);
    m_writer.line("FADEIN %d %.10f 0", fadeInShape, fadeInLen);
    m_writer.line("FADEOUT %d %.10f 0", fadeOutShape, fadeOutLen);
    m_writer.line("MUTE %d 0", clip->mute != 0 ? 1 : 0);
    m_writer.line("NAME \"%s\"", clipName ? escape_rpp_string(clipName).c_str() : "");
    m_writer.line("VOLPAN %.6f 0.000000 1.000000 -1", gain_lin);
    m_writer.line("SOFFS %.10f", srcOffset);

    // Per-clip varying gain automation (clip->automation, not clip->gain)
    if (clip->automation && (clip->automation->flags & AAFI_AUDIO_GAIN_VARIABLE))
        writeVolEnvelope(clip->automation, pos, len, "VOLENV");

    writeSource(clip);

    m_writer.line(">"); // </ITEM>
}

void AafImporter::writeSource(const aafiAudioClip *clip) const {
    if (!clip->essencePointerList || !clip->essencePointerList->essenceFile) {
        m_writer.line("<SOURCE EMPTY");
        m_writer.line(">");
        return;
    }

    aafiAudioEssenceFile *ess = clip->essencePointerList->essenceFile;

    // Extract embedded essence if not yet done.
    // aafi_extractAudioEssenceFile() sets ess->usable_file_path on success.

    if (ess->is_embedded && !ess->usable_file_path) {
        char *outPath = nullptr;
        const int rc = aafi_extractAudioEssenceFile(m_aafi, ess,
                                                    AAFI_EXTRACT_DEFAULT,
                                                    m_extractDir.c_str(),
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
        m_writer.line("<SOURCE EMPTY");
        m_writer.line(">");
        return;
    }

    auto srcType = "WAVE";
    if (const char *ext = strrchr(filePath, '.')) {
        if (strcasecmp(ext, ".mp3") == 0) srcType = "MP3";
        else if (strcasecmp(ext, ".flac") == 0) srcType = "FLAC";
        else if (strcasecmp(ext, ".ogg") == 0) srcType = "VORBIS";
    }

    m_writer.line("<SOURCE %s", srcType);
    m_writer.line("FILE \"%s\"", escape_rpp_string(filePath).c_str());
    m_writer.line(">");
}

void AafImporter::writeVolEnvelope(const aafiAudioGain *gain, double seg_start_sec, double seg_len_sec,
                                   const char *envTag) const {
    if (!gain) return;
    if (!(gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (gain->pts_cnt == 0 || !gain->time || !gain->value) return;

    m_writer.line("<%s", envTag);
    // w.line("ACT 0 -1");
    m_writer.line("VIS 1 1 1");

    for (unsigned int i = 0; i < gain->pts_cnt; ++i) {
        const double frac = rational_to_double(gain->time[i]); // 0.0 .. 1.0
        const double t = seg_start_sec + frac * seg_len_sec;
        const double val = clamp_volume(rational_to_double(gain->value[i]));

        m_writer.line("PT %.10f %.10f 0", t, val);
    }

    m_writer.line(">");
}

// ReSharper disable once CppDFAConstantParameter
// seg_start_sec is always 0.0 for now, but might be useful later to add item pan automation
void AafImporter::writePanEnvelope(const aafiAudioGain *pan, const double seg_start_sec,
                                   const double seg_len_sec) const {
    if (!(pan->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (pan->pts_cnt == 0 || !pan->time || !pan->value) return;

    // AAF pan: 0=left, 0.5=center, 1=right → REAPER: -1=left, 0=centre, +1=right
    m_writer.line("<PANENV2");
    // w.line("ACT 1 -1"); //Uncomment to always active
    m_writer.line("VIS 1 1 1");
    m_writer.line("ARM 1");

    for (unsigned int i = 0; i < pan->pts_cnt; ++i) {
        const double frac = rational_to_double(pan->time[i]);
        const double t = seg_start_sec + frac * seg_len_sec;
        const double aafPan = rational_to_double(pan->value[i]);
        const double rPan = clamp_pan((aafPan - 0.5) * -2.0); // multiply negative otherwise panning is reversed
        m_writer.line("PT %.10f %.10f 0", t, rPan);
    }

    m_writer.line(">");
}

void AafImporter::writeMarkers() const {
    int id = 1;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(m_aafi, m) {
        // edit_rate is a pointer
        const double t = pos_to_seconds(m->start, m->edit_rate);

        if (const bool isRegion = (m->length > 0); !isRegion) {
            m_writer.line("MARKER %d %.10f \"%s\" 0 0 1",
                          id++, t,
                          m->name ? escape_rpp_string(m->name).c_str() : "");
        } else {
            const double end = pos_to_seconds(m->start + m->length, m->edit_rate);
            m_writer.line("MARKER %d %.10f \"%s\" 0 1 1",
                          id, t, m->name ? escape_rpp_string(m->name).c_str() : "");
            m_writer.line("MARKER %d %.10f \"%s\" 0 1 1",
                          id + 1, end, m->name ? escape_rpp_string(m->name).c_str() : "");
            id += 2;
        }
    }
}
