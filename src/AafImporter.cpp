#include "AafImporter.h"
#include "helpers.h"
#include "FadeResolver.h"

#include <libaaf.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <defines.h>

// TODO: fix video support [MXF files audio is not grabbed properly]


AafImporter::AafImporter(ProjectStateContext *ctx, const char *filepath)
    : m_writer(ctx),
      m_aafi(aafi_alloc(nullptr)),
      m_extractDir(build_extract_dir(filepath)),
      m_filePath(filepath) {
}


void AafImporter::setMediaLocation() const {
    std::string dir(m_filePath);
    if (const auto sep = dir.find_last_of("/\\"); sep != std::string::npos) dir.resize(sep);
    aafi_set_option_str(m_aafi, "media_location", dir.c_str());
}

bool AafImporter::loadFile() {
    if (aafi_load_file(m_aafi, m_filePath.c_str()) != 0) {
        rlog("ReAAF: failed to load '%s'\n", m_filePath.c_str());
        aafi_release(&m_aafi);
        return false;
    }
    return true;
}

const char *AafImporter::rppSourceTypeFromPath(const char *filePath) {
    // Determine source type from extension
    auto srcType = "WAVE";
    if (const char *ext = strrchr(filePath, '.')) {
        if (strcasecmp(ext, ".mp3") == 0) srcType = "MP3";
        else if (strcasecmp(ext, ".flac") == 0) srcType = "FLAC";
        else if (strcasecmp(ext, ".ogg") == 0) srcType = "VORBIS";
        // .wav / .aif / .aiff → WAVE (default)
    }
    return srcType;
}

int AafImporter::run() {
    if (!m_aafi) {
        rlog("ReAAF: aafi_alloc() failed\n");
        return -1;
    }

    aafi_set_debug(m_aafi, VERB_WARNING, 0, nullptr, nullptr, nullptr);
    aafi_set_option_int(m_aafi, "protools", PROTOOLS_ALL_OPT);

    setMediaLocation();
    if (!loadFile()) return -1;

    const uint32_t samplerate =
            m_aafi->Audio->samplerate > 0 ? m_aafi->Audio->samplerate : 48000u;

    const double tcOffset = pos_to_seconds(m_aafi->compositionStart,
                                           m_aafi->compositionStart_editRate);

    const int fps = m_aafi->Timecode->fps;

    // Determine fractional timecode and/or drop frame rate.
    // isDrop: 0 = integer fps, 1 = drop, 2 = non-drop fractional (23.976 or 29.97 ND)
    const bool isFrac = m_aafi->Timecode->edit_rate->denominator != 1;
    const uint8_t isDrop = isFrac ? (fps == 24 ? 2 : (m_aafi->Timecode->drop > 0 ? 1 : 2)) : 0;

#ifdef REAAF_DEBUG
    rlog(":::TIMECODE:::\nAAFI: %d, %d, %d\nRPP: %d, %d\n\n", m_aafi->Timecode->edit_rate->numerator,
         m_aafi->Timecode->edit_rate->denominator, m_aafi->Timecode->drop, fps,
         isDrop);
#endif

    // Guard destroyed at end of scope → emits closing ">" for REAPER_PROJECT
    auto proj = m_writer.project(tcOffset, fps, isDrop, samplerate);

    processMarkers();

    const aafiAudioTrack *track = nullptr;
    int trackIdx = 1;
    int itemCount = 1;

    const aafiVideoTrack *vtrack = nullptr;
    AAFI_foreachVideoTrack(m_aafi, vtrack) {
        processTrack_Video(vtrack, trackIdx++, itemCount);
    }

    AAFI_foreachAudioTrack(m_aafi, track) {
        processTrack_Audio(track);
    }

    aafi_release(&m_aafi);
    return 0;
}

double AafImporter::resolveConstantGain(const aafiAudioGain *gain, const double defaultValue = 1.0) {
    if (gain
        && gain->flags & AAFI_AUDIO_GAIN_CONSTANT
        && gain->pts_cnt >= 1
        && gain->value)
        return rational_to_double(gain->value[0]);
    return defaultValue;
}


void AafImporter::processTrack_Audio(const aafiAudioTrack *track) {
    const char *trackName = track->name ? track->name : "";

    // format == channel count; AAFI_TRACK_FORMAT_UNKNOWN (99) → mono
    int nchan = track->format;
    if (nchan <= 0 || nchan == 99) nchan = 1;

    const double vol = clamp_volume(resolveConstantGain(track->gain));
    const double pan = clamp_pan((resolveConstantGain(track->pan, 0.5) - 0.5) * -2.0);

    auto t = m_writer.track(trackName, vol, pan,
                            track->mute != 0 ? 1 : 0,
                            track->solo != 0 ? 1 : 0,
                            nchan);

    // Track-level automation — composition length is the time base
    const double compLen = pos_to_seconds(m_aafi->compositionLength,
                                          m_aafi->compositionLength_editRate);

    if (track->gain && (track->gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) {
        processEnvelope(track->gain, compLen, "VOLENV2",
                        [](const double v) { return clamp_volume(v); });
    }

    if (track->pan && (track->pan->flags & AAFI_AUDIO_GAIN_VARIABLE)) {
        // AAF pan: 0=left, 0.5=center, 1=right → REAPER −1...+1 (negated)
        processEnvelope(track->pan, compLen, "PANENV2",
                        [](double v) { return clamp_pan((v - 0.5) * -2.0); },
                        /*arm=*/true);
    }

    const XFadeMap xFadeMap = buildXFadeMap(track);

    aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if (aafiAudioClip *clip = aafi_timelineItemToAudioClip(ti))
            processItem_Audio(clip, ti, track->edit_rate, xFadeMap);
        // AAFI_TRANS items are consumed via xFadeMap; nothing to emit directly.
    }
    // 't' destructor fires here → ">"
}


const char *AafImporter::resolveClipName(const aafiAudioClip *clip) {
    const char *clipName = clip->subClipName;
    if (!clipName || clipName[0] == '\0') {
        if (clip->essencePointerList && clip->essencePointerList->essenceFile)
            clipName = clip->essencePointerList->essenceFile->name;
    }
    return clipName;
}

void AafImporter::processItem_Audio(aafiAudioClip *clip,
                                    const aafiTimelineItem *ti,
                                    const aafRational_t *trackEditRate,
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

    const char *clipName = resolveClipName(clip);

    // Guard destroyed at end of scope → emits closing ">" for ITEM
    auto i = m_writer.item(clipName,
                           pos, len,
                           fadeInLen, fadeInShape,
                           fadeOutLen, fadeOutShape,
                           gainLin, srcOffset,
                           clip->mute != 0 ? 1 : 0);

    // Per-clip varying gain automation
    if (clip->automation && (clip->automation->flags & AAFI_AUDIO_GAIN_VARIABLE)) {
        processEnvelope(clip->automation, len, "VOLENV",
                        [](const double v) { return clamp_volume(v); });
    }

    processSource_Audio(clip);
    // 'i' destructor fires here ">"
}


void AafImporter::processSource_Audio(const aafiAudioClip *clip) {
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
        // Create a media folder if it's not there already.
        if (!m_extractDirCreated) {
            if (!ensure_dir(m_extractDir)) {
                rlog("ReAAF: could not create %s\n", m_extractDir.c_str());
                emitEmpty();
                return;
            }
            m_extractDirCreated = true;
        }

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

    const char *srcType = rppSourceTypeFromPath(filePath);

    auto s = m_writer.source(srcType, filePath);
    // 's' destructor fires here ">"
}

void AafImporter::processTrack_Video(const aafiVideoTrack *track, int trackIdx, int &itemCounter) {
    auto t = m_writer.track("VIDEO", 1.0, 0.0, 0, 0, 1);
    const aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if (ti->type == AAFI_VIDEO_CLIP)
            processItem_Video(static_cast<aafiVideoClip *>(ti->data), track->edit_rate, itemCounter++);
        //libaaf has no transitions on video items. Noooooooo
    }
}

void AafImporter::processItem_Video(const aafiVideoClip *clip, const aafRational_t *trackEditRate, int itemIdx) {
    const double pos = pos_to_seconds(clip->pos, trackEditRate);
    const double len = pos_to_seconds(clip->len, trackEditRate);
    const double srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);

    const char *clipName = clip->Essence ? clip->Essence->name : "Video";

    auto i = m_writer.item(clipName,
                           pos, len,
                           0.0, 0,
                           0.0, 0,
                           1.0, srcOffset,
                           0);

    processSource_Video(clip->Essence);
}

void AafImporter::processSource_Video(const aafiVideoEssence *ess) {
    if (!ess || !ess->usable_file_path || ess->usable_file_path[0] == '\0') {
        rlog("ReAAF: WARNING: video essence has no usable path.\n");
        auto s = m_writer.source(nullptr, nullptr);
        return;
    }

    auto s = m_writer.source("VIDEO", ess->usable_file_path);
}


void AafImporter::processEnvelope(const aafiAudioGain *gain,
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
        const double t = frac * segLenSec;
        const double val = transform(rational_to_double(gain->value[i]));
        m_writer.writeEnvPoint(t, val);
    }
    // 'env' destructor fires here ">"
}


void AafImporter::processMarkers() const {
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
