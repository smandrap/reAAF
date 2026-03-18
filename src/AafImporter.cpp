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

#include "AafImporter.h"
#include "helpers.h"
#include "FadeResolver.h"
#include "AafiHandle.h"
#include "LogBuffer.h"

#include <libaaf.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <defines.h>


void libaafLogCallback(aafLog *, void *, const int lib, const int type,
                       const char *, const char *, int,
                       const char *msg, void *user) {
    if (lib != LOG_SRC_ID_AAF_IFACE || type != VERB_WARNING) return;
    if (!msg || !user) return;
    if (const auto *self = static_cast<AafImporter *>(user); self->m_logBuffer)
        self->m_logBuffer->log(LogEntry::WARN, msg);
}


AafImporter::AafImporter(ProjectStateContext *ctx, const char *filepath, LogBuffer *logBuffer)
    : m_writer(ctx),
      m_aafi(AafiHandle(aafi_alloc(nullptr))),
      m_filePath(filepath),
      m_extractDir(buildExtractDir(filepath)),
      m_logBuffer(logBuffer) {}

int AafImporter::run() {
    if (!m_aafi) {
        rlog("ReAAF: aafi_alloc() failed\n");
        return -1;
    }

    if (!m_logBuffer) {
        rlog("ReAAF: Can't allocate log buffer\n");
        return -1;
    }

    aafi_set_debug(m_aafi.get(), VERB_WARNING, 0, nullptr, libaafLogCallback, this);
    aafi_set_option_int(m_aafi.get(), "protools", PROTOOLS_ALL_OPT);

    setMediaLocation();
    if (!loadFile()) return -1;

    const uint32_t samplerate =
            m_aafi->Audio->samplerate > 0 ? m_aafi->Audio->samplerate : 48000u;

    const double tcOffset = pos_to_seconds(m_aafi->compositionStart,
                                           m_aafi->compositionStart_editRate);

    int fps = 25;
    uint8_t isDrop = 0;

    if (m_aafi->Timecode) {
        fps = m_aafi->Timecode->fps;

        // Determine fractional timecode and/or drop frame rate.
        // isDrop: 0 = integer fps, 1 = drop, 2 = non-drop fractional (23.976 or 29.97 ND)
        const bool isFrac = m_aafi->Timecode->edit_rate->denominator != 1;
        isDrop = isFrac ? (fps == 24 ? 2 : (m_aafi->Timecode->drop > 0 ? 1 : 2)) : 0;
    }

    // Guard destroyed at end of scope, emits closing ">" for REAPER_PROJECT
    auto proj = m_writer.project(tcOffset, fps, isDrop, samplerate);

    processMarkers();

    const aafiAudioTrack *track = nullptr;

    const aafiVideoTrack *vtrack = nullptr;
    AAFI_foreachVideoTrack(m_aafi, vtrack) {
        processTrack_Video(vtrack);
    }

    AAFI_foreachAudioTrack(m_aafi, track) {
        processTrack_Audio(track);
    }
    m_logBuffer->log(LogEntry::ERROR, "TEST ERROR");
    m_logBuffer->log(LogEntry::WARN, "TEST WARN");

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
    if (!clip) return "";
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
    aafi_set_option_str(m_aafi.get(), "media_location", dir.c_str());
}

bool AafImporter::loadFile() const {
    if (aafi_load_file(m_aafi.get(), m_filePath.c_str()) != 0) {
        rlog("ReAAF: failed to load '%s'\n", m_filePath.c_str());
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

// Find the pointer for the provided trackIdx.
// If the file is interleaved (channels > 1), only emit on trackIdx 0.
const aafiAudioEssencePointer *AafImporter::getAudioEssencePtr(const aafiAudioClip *clip, const int trackIdx) {
    int idx = 0;
    const aafiAudioEssencePointer *ptr = nullptr;
    AAFI_foreachEssencePointer(clip->essencePointerList, ptr) {
        if (ptr->essenceFile->channels > 1 || idx == trackIdx) return ptr;
        ++idx;
    }
    return nullptr;
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

            const auto ptr = getAudioEssencePtr(clip, trackIdx);
            if (!ptr) continue;

            processItem_Audio(clip, ti, track->edit_rate, xFadeMap, ptr);
        }
    }
}

void AafImporter::processTrack_Video(const aafiVideoTrack *track) {
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
    if (!essPtr || !essPtr->essenceFile) {
        m_writer.emptySource();
        return;
    }

    aafiAudioEssenceFile *ess = essPtr->essenceFile;

    // Extract embedded essence if not yet done
    if (ess->is_embedded && !ess->usable_file_path) {
        // Create a media folder if it's not there already.
        if (!m_extractDirCreated) {
            if (!ensure_dir(m_extractDir)) {
                m_logBuffer->logf(LogEntry::ERROR, "could not create extract dir: %s", m_extractDir.c_str());
                m_writer.emptySource();
                return;
            }
            m_extractDirCreated = true;
        }

        // We don't care about outPath since we then use ess->usable_file_path, but the function needs it so...
        char *outPath = nullptr;
        const int rc = aafi_extractAudioEssenceFile(m_aafi.get(), ess,
                                                    AAFI_EXTRACT_DEFAULT,
                                                    m_extractDir.c_str(),
                                                    0, 0,
                                                    nullptr, &outPath);
        free(outPath);

        if (rc != 0) {
            m_logBuffer->logf(LogEntry::ERROR, "failed to extract '%s'",
                              ess->unique_name ? ess->unique_name : "(unnamed)");
            return;
        }
        m_logBuffer->logf(LogEntry::INFO, "Extracted '%s'", ess->unique_name);
    }

    // TODO: sanitize path, might contain invalid chars
    const char *filePath = ess->usable_file_path;
    if (!filePath || *filePath == '\0') {
        m_logBuffer->logf(LogEntry::WARN, "no usable path for '%s'",
                          ess->unique_name ? ess->unique_name : "(unnamed)");
        m_writer.emptySource();
        return;
    }

    const char *srcType = rppSourceTypeFromPath(filePath);

    auto w_src = m_writer.source(srcType, filePath);
}

void AafImporter::processSource_Video(const aafiVideoEssence *ess) {
    if (!ess || !ess->usable_file_path || *ess->usable_file_path == '\0') {
        m_logBuffer->log(LogEntry::WARN, "video essence has no usable path");
        m_writer.emptySource();
        return;
    }
    m_logBuffer->logf(LogEntry::INFO, "Processed video: '%s'", ess->usable_file_path);
    auto w_src = m_writer.source("VIDEO", ess->usable_file_path);
}


void AafImporter::processMarkers() const {
    int id = 1;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(m_aafi, m) {
        const double t = pos_to_seconds(m->start, m->edit_rate);

        const bool hasColor = m->RGBColor[0] || m->RGBColor[1] || m->RGBColor[2];
        const int color = hasColor ? aafiColorToReaper(m->RGBColor) : 0;

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
