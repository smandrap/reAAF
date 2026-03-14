#include "AafImporter.h"
#include "helpers.h"
#include "FadeResolver.h"

#include <libaaf.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <defines.h>


AafImporter::AafImporter(ProjectStateContext *ctx, const char *filepath)
    : m_writer(ctx),
      m_aafi(aafi_alloc(nullptr)),
      m_filePath(filepath),
      m_extractDir(buildExtractDir(filepath)) {
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
    int itemCount = 1;

    const aafiVideoTrack *vtrack = nullptr;
    AAFI_foreachVideoTrack(m_aafi, vtrack) {
        processTrack_Video(vtrack, itemCount);
    }

    AAFI_foreachAudioTrack(m_aafi, track) {
        processTrack_Audio(track);
    }

    aafi_release(&m_aafi);
    return 0;
}

std::string AafImporter::buildExtractDir(const char *filepath) {
    std::string p(filepath);
    if (const auto dot = p.rfind('.'); dot != std::string::npos) p.resize(dot);
    p += "-media";
    return p;
}

const char *AafImporter::rppSourceTypeFromPath(const char *filePath) {
    auto srcType = "WAVE";
    if (const char *ext = strrchr(filePath, '.')) {
        if (strcasecmp(ext, ".mp3") == 0) srcType = "MP3";
        else if (strcasecmp(ext, ".flac") == 0) srcType = "FLAC";
        else if (strcasecmp(ext, ".ogg") == 0) srcType = "VORBIS";
        else if (strcasecmp(ext, ".mxf") == 0) srcType = "VIDEO";
    }
    return srcType;
}

double AafImporter::resolveConstantGain(const aafiAudioGain *gain, const double defaultValue) {
    if (gain
        && gain->flags & AAFI_AUDIO_GAIN_CONSTANT
        && gain->pts_cnt >= 1
        && gain->value)
        return rational_to_double(gain->value[0]);
    return defaultValue;
}

const char *AafImporter::resolveClipName(const aafiAudioClip *clip) {
    const char *clipName = clip->subClipName;
    if (!clipName || clipName[0] == '\0') {
        if (clip->essencePointerList && clip->essencePointerList->essenceFile)
            clipName = clip->essencePointerList->essenceFile->name;
    }
    return clipName;
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

void AafImporter::processTrackAutomation(const aafiAudioTrack *track, const double compLen) {
    if (track->gain && (track->gain->flags & AAFI_AUDIO_GAIN_VARIABLE))
        processEnvelope(track->gain, compLen, "VOLENV2",
                        [](const double v) { return clamp_volume(v); });

    if (track->pan && (track->pan->flags & AAFI_AUDIO_GAIN_VARIABLE))
        processEnvelope(track->pan, compLen, "PANENV2",
                        [](const double v) { return clamp_pan((v - 0.5) * -2.0); },
                        /*arm=*/true);
}

int AafImporter::countRequiredTracks(const aafiAudioClip *clip, int &nchan) {
    int cnt = 0;
    const aafiAudioEssencePointer *p = nullptr;
    AAFI_foreachEssencePointer(clip->essencePointerList, p) {
        if (p->essenceFile->channels > 1) {
            nchan = std::max(nchan, static_cast<int>(p->essenceFile->channels));
            cnt = 1;
            break;
        }
        ++cnt;
    }
    return cnt;
}

void AafImporter::processTrack_Audio(const aafiAudioTrack *track) {
    const char *trackName = track->name ? track->name : "";

    const double vol = clamp_volume(resolveConstantGain(track->gain));
    const double pan = clamp_pan((resolveConstantGain(track->pan, 0.5) - 0.5) * -2.0);
    const int mute = track->mute != 0 ? 1 : 0;
    const int solo = track->solo != 0 ? 1 : 0;

    const double compLen = pos_to_seconds(m_aafi->compositionLength,
                                          m_aafi->compositionLength_editRate);

    const XFadeMap xFadeMap = buildXFadeMap(track);

    // Pre-pass: count required REAPER tracks and channel count.
    // essenceFile->channels > 1 means interleaved — single track, break.
    // Otherwise, each pointer is a separate mono file — one track per pointer.
    int requiredTracks = 1;
    int nchan = 2;
    aafiTimelineItem *ti = nullptr;

    AAFI_foreachTrackItem(track, ti) {
        if (const auto *clip = aafi_timelineItemToAudioClip(ti)) {
            requiredTracks = countRequiredTracks(clip, nchan);
        }
    }

    for (int trackIdx = 0; trackIdx < requiredTracks; ++trackIdx) {
        std::string fullName = requiredTracks > 1
                                   ? std::string(trackName) + "_" + std::to_string(trackIdx + 1)
                                   : trackName;

        auto w_trk = m_writer.track(fullName.c_str(), vol, pan, mute, solo,
                                requiredTracks == 1 ? nchan : 2);

        processTrackAutomation(track, compLen);

        AAFI_foreachTrackItem(track, ti) {
            auto *clip = aafi_timelineItemToAudioClip(ti);
            if (!clip) continue;

            // Find the pointer for this trackIdx.
            // If the file is interleaved (channels > 1), only emit on trackIdx 0.
            int idx = 0;
            const aafiAudioEssencePointer *ptr = nullptr;
            AAFI_foreachEssencePointer(clip->essencePointerList, ptr) {
                if (ptr->essenceFile->channels > 1 || idx == trackIdx) break;
                ++idx;
            }

            if (!ptr || idx != trackIdx) continue;

            processItem_Audio(clip, ti, track->edit_rate, xFadeMap, ptr);
        }
    }
}

void AafImporter::processTrack_Video(const aafiVideoTrack *track, int &itemCounter) {
    auto w_trk = m_writer.track("VIDEO", 1.0, 0.0, 0, 0, 1);
    const aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if (ti->type == AAFI_VIDEO_CLIP)
            processItem_Video(static_cast<aafiVideoClip *>(ti->data), track->edit_rate);
    }
}


void AafImporter::processItem_Audio(aafiAudioClip *clip,
                                    const aafiTimelineItem *ti,
                                    const aafRational_t *trackEditRate,
                                    const XFadeMap &xFadeMap,
                                    const aafiAudioEssencePointer *essPtr) {
    const double pos = pos_to_seconds(clip->pos, trackEditRate);
    const double len = pos_to_seconds(clip->len, trackEditRate);
    const double srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);
    const double gainLin = clamp_volume(resolveConstantGain(clip->gain));

    const auto [fadeInLen, fadeInShape] = resolveFadeIn(clip, ti, xFadeMap, trackEditRate);
    const auto [fadeOutLen, fadeOutShape] = resolveFadeOut(clip, ti, xFadeMap, trackEditRate);

    const char *clipName = resolveClipName(clip);

    auto w_itm = m_writer.item(clipName,
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

    processSource_Audio(essPtr);
    // 'w_itm' destructor fires here ">"
}

void AafImporter::processItem_Video(const aafiVideoClip *clip, const aafRational_t *trackEditRate) {
    const double pos = pos_to_seconds(clip->pos, trackEditRate);
    const double len = pos_to_seconds(clip->len, trackEditRate);
    const double srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);

    const char *clipName = clip->Essence ? clip->Essence->name : "Video";

    auto w_itm = m_writer.item(clipName,
                           pos, len,
                           0.0, 0,
                           0.0, 0,
                           1.0, srcOffset,
                           0);

    processSource_Video(clip->Essence);
}

void AafImporter::processSource_Audio(const aafiAudioEssencePointer *essPtr) {
    const auto emitEmpty = [this] {
        auto s = m_writer.source(nullptr, nullptr);
    };

    if (!essPtr || !essPtr->essenceFile) {
        emitEmpty();
        return;
    }

    aafiAudioEssenceFile *ess = essPtr->essenceFile;

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

    auto w_src = m_writer.source(srcType, filePath);
}

void AafImporter::processSource_Video(const aafiVideoEssence *ess) {
    if (!ess || !ess->usable_file_path || ess->usable_file_path[0] == '\0') {
        rlog("ReAAF: WARNING: video essence has no usable path.\n");
        auto s = m_writer.source(nullptr, nullptr);
        return;
    }
    auto s = m_writer.source("VIDEO", ess->usable_file_path);
}


void AafImporter::processMarkers() const {
    int id = 1;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(m_aafi, m) {
        const double t = pos_to_seconds(m->start, m->edit_rate);

        const bool hasColor = m->RGBColor[0] || m->RGBColor[1] || m->RGBColor[2];
        const int color = hasColor ? aafiColorToReaper(m->RGBColor) : 0;

#ifdef REAAF_DEBUG
        rlog("Marker color: %d, %d, %d\n", m->RGBColor[0], m->RGBColor[1], m->RGBColor[2]);
#endif

        if (m->length == 0) {
            m_writer.writeMarker(id++, t, m->name, false, color);
            continue;
        }
        // it's a region
        const double endT = pos_to_seconds(m->start + m->length, m->edit_rate);
        m_writer.writeMarker(id, t, m->name, true, color);
        m_writer.writeMarker(id++, endT, "", true, color);
    }
}

void AafImporter::processEnvelope(const aafiAudioGain *gain,
                                  const double segLenSec,
                                  const char *tag,
                                  const std::function<double(double)> &transform,
                                  const bool arm) {
    if (!gain) return;
    if (!(gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (gain->pts_cnt == 0 || !gain->time || !gain->value) return;

    auto w_env = m_writer.envelope(tag, arm);

    for (unsigned int i = 0; i < gain->pts_cnt; ++i) {
        const double frac = rational_to_double(gain->time[i]); // 0.0 .. 1.0
        const double t = frac * segLenSec;
        const double val = transform(rational_to_double(gain->value[i]));
        m_writer.writeEnvPoint(t, val);
    }
    // 'env' destructor fires here ">"
}
