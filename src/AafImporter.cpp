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
#include "AafEmitter.h"
#include "AafiHandle.h"
#include "FadeResolver.h"
#include "LogBuffer.h"
#include "helpers.h"

#include <libaaf.h>

// ReSharper disable once CppUnusedIncludeDirective
#include <defines.h>


// ---------------------------------------------------------------------------
// File-scope helpers (no AafImporter state)
// ---------------------------------------------------------------------------
namespace {
struct TrackLayout {
    int count;
    int nchan;
};

double resolveConstantGain(const aafiAudioGain *gain, const double defaultValue = 1.0) {
    if ( gain && gain->flags & AAFI_AUDIO_GAIN_CONSTANT && gain->pts_cnt >= 1 && gain->value )
        return rational_to_double(gain->value[0]);
    return defaultValue;
}

// not owned — points into LibAAF-owned string
const char *resolveClipName(const aafiAudioClip *clip) {
    if ( !clip )
        return "";
    const char *clipName = clip->subClipName;
    if ( !clipName || clipName[0] == '\0' ) {
        if ( clip->essencePointerList && clip->essencePointerList->essenceFile )
            clipName = clip->essencePointerList->essenceFile->name;
    }
    return clipName;
}

TrackLayout countRequiredTracks(const aafiAudioClip *clip) {
    int cnt = 0;
    int nchan = 2;
    const aafiAudioEssencePointer *p = nullptr;
    AAFI_foreachEssencePointer(clip->essencePointerList, p) {
        if ( !p->essenceFile ) {
            ++cnt;
            continue;
        }
        if ( p->essenceFile->channels > 1 ) {
            nchan = std::max(nchan, static_cast<int>(p->essenceFile->channels));
            cnt = 1;
            break;
        }
        ++cnt;
    }
    return {cnt, nchan};
}

// Find the pointer for the provided trackIdx.
// If the file is interleaved (channels > 1), only emit on trackIdx 0.
const aafiAudioEssencePointer *getAudioEssencePtr(const aafiAudioClip *clip, const int trackIdx) {
    int idx = 0;
    const aafiAudioEssencePointer *ptr = nullptr;
    AAFI_foreachEssencePointer(clip->essencePointerList, ptr) {
        if ( !ptr->essenceFile ) {
            ++idx;
            continue;
        }
        if ( ptr->essenceFile->channels > 1 || idx == trackIdx )
            return ptr;
        ++idx;
    }
    return nullptr;
}
} // namespace

// ---------------------------------------------------------------------------
// AafImporter implementation
// ---------------------------------------------------------------------------
void AafImporter::libaafLogCallback(aafLog *, void *, const int lib, const int type, const char *,
                                    const char *, int, const char *msg, void *user) {
    if ( lib != LOG_SRC_ID_AAF_IFACE || type != VERB_WARNING )
        return;
    if ( !msg || !user )
        return;
    const auto *self = static_cast<AafImporter *>(user);
    self->m_logBuffer.log(LogEntry::DEBUG, msg);
}


AafImporter::AafImporter(IRppSink *sink, const char *filepath, LogBuffer &logBuffer)
    : m_writer(sink), m_aafi(AafiHandle(aafi_alloc(nullptr))), m_filePath(filepath),
      m_extractDir(buildExtractDir(filepath)), m_logBuffer(logBuffer) {}

int AafImporter::run() {
    if ( !m_aafi ) {
        rlog("ReAAF: aafi_alloc() failed\n");
        return -1;
    }

    aafi_set_debug(m_aafi.get(), VERB_WARNING, 0, nullptr, &AafImporter::libaafLogCallback, this);
    aafi_set_option_int(m_aafi.get(), "protools", PROTOOLS_ALL_OPT);

    setMediaLocation();
    if ( !loadFile() )
        return -1;

    const CompositionData comp = extractComposition();
    auto proj =
        m_writer.project(comp.tcOffset, comp.maxProjLen, comp.fps, comp.isDrop, comp.samplerate);
    const AafEmitter emitter(m_writer);
    emitter.emit(comp);
    return 0;
}

void AafImporter::setMediaLocation() const {
    std::string dir(m_filePath);
    if ( const auto sep = dir.find_last_of("/\\"); sep != std::string::npos )
        dir.resize(sep);
    aafi_set_option_str(m_aafi.get(), "media_location", dir.c_str());
}

bool AafImporter::loadFile() const {
    if ( aafi_load_file(m_aafi.get(), m_filePath.c_str()) != 0 ) {
        rlog("ReAAF: failed to load '%s'\n", m_filePath.c_str());
        return false;
    }
    return true;
}

CompositionData AafImporter::extractComposition() {
    if ( m_aafi->compositionName && m_aafi->compositionName[0] != '\0' )
        m_logBuffer.logf(LogEntry::INFO, "Composition: %s", m_aafi->compositionName);

    m_logBuffer.logf(LogEntry::INFO, "Audio tracks: %u  |  Sample rate: %u Hz  |  Bit depth: %u",
                     m_aafi->Audio->track_count, m_aafi->Audio->samplerate,
                     m_aafi->Audio->samplesize);

    if ( m_aafi->Audio->samplerate == 0 )
        m_logBuffer.log(LogEntry::WARN, "Sample rate missing from AAF, defaulting to 48000 Hz");

    CompositionData comp;
    comp.samplerate = m_aafi->Audio->samplerate > 0 ? m_aafi->Audio->samplerate : 48000u;
    comp.tcOffset = pos_to_seconds(m_aafi->compositionStart, m_aafi->compositionStart_editRate);
    comp.maxProjLen = pos_to_seconds(m_aafi->compositionLength, m_aafi->compositionStart_editRate);

    comp.fps = 25;
    comp.isDrop = 0;

    if ( m_aafi->Timecode ) {
        comp.fps = m_aafi->Timecode->fps;
        comp.isDrop = computeTimecodeIsDrop(comp.fps, m_aafi->Timecode->edit_rate->denominator,
                                            m_aafi->Timecode->drop);
        m_logBuffer.logf(LogEntry::INFO, "Timecode: %d fps%s", comp.fps,
                         comp.isDrop == 1 ? " drop" : (comp.isDrop == 2 ? " non-drop" : ""));
    }

    comp.markers = extractMarkers();

    const aafiVideoTrack *vtrack = nullptr;
    AAFI_foreachVideoTrack(m_aafi, vtrack) {
        comp.videoTracks.push_back(extractVideoTrack(vtrack));
    }

    const aafiAudioTrack *track = nullptr;
    AAFI_foreachAudioTrack(m_aafi, track) {
        auto subtracks = extractAudioTrack(track);
        for ( auto &st : subtracks )
            comp.audioTracks.push_back(std::move(st));
    }

    return comp;
}

std::vector<MarkerData> AafImporter::extractMarkers() const {
    std::vector<MarkerData> result;
    int id = 1;
    int markerCount = 0, regionCount = 0;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(m_aafi, m) {
        const double t = pos_to_seconds(m->start, m->edit_rate);

        const bool hasColor = m->RGBColor[0] || m->RGBColor[1] || m->RGBColor[2];
        const int color = hasColor ? aafiColorToReaper(m->RGBColor) : 0;

        MarkerData md;
        md.id = id++;
        md.t = t;
        md.name = m->name ? m->name : "";
        md.color = color;

        if ( m->length == 0 ) {
            md.isRegion = false;
            result.push_back(std::move(md));
            ++markerCount;
        } else {
            md.isRegion = true;
            md.endT = pos_to_seconds(m->start + m->length, m->edit_rate);
            result.push_back(std::move(md));
            ++regionCount;
        }
    }
    if ( markerCount + regionCount > 0 )
        m_logBuffer.logf(LogEntry::INFO, "Markers: %d  |  Regions: %d", markerCount, regionCount);
    return result;
}

std::vector<AudioTrackData> AafImporter::extractAudioTrack(const aafiAudioTrack *track) {
    const char *trackName = track->name ? track->name : "";

    m_logBuffer.logf(LogEntry::INFO, "Audio track %u: \"%s\"  clips: %d", track->number, trackName,
                     track->clipCount);

    if ( track->mute )
        m_logBuffer.logf(LogEntry::WARN, "Track %u (\"%s\") is muted", track->number, trackName);

    const double vol = clamp_volume(resolveConstantGain(track->gain));
    const double pan = clamp_pan((resolveConstantGain(track->pan, 0.5) - 0.5) * -2.0);
    const int mute = track->mute != 0 ? 1 : 0;
    const int solo = track->solo != 0 ? 1 : 0;

    const double compLen =
        pos_to_seconds(m_aafi->compositionLength, m_aafi->compositionLength_editRate);

    const XFadeMap xFadeMap = buildXFadeMap(track);

    // Pre-pass: count required REAPER tracks and channel count.
    int requiredTracks = 1;
    int nchan = 2;
    aafiTimelineItem *ti = nullptr;

    AAFI_foreachTrackItem(track, ti) {
        if ( const auto *clip = aafi_timelineItemToAudioClip(ti) ) {
            const auto [count, chan] = countRequiredTracks(clip);
            requiredTracks = std::max(requiredTracks, count);
            nchan = std::max(nchan, chan);
        }
    }

    if ( requiredTracks > 1 )
        m_logBuffer.logf(LogEntry::INFO, "Track %u split into %d mono sub-tracks", track->number,
                         requiredTracks);

    std::vector<AudioTrackData> result;
    result.reserve(static_cast<size_t>(requiredTracks));

    for ( int trackIdx = 0; trackIdx < requiredTracks; ++trackIdx ) {
        AudioTrackData at;
        at.name = requiredTracks > 1 ? std::string(trackName) + "_" + std::to_string(trackIdx + 1)
                                     : trackName;
        at.vol = vol;
        at.pan = pan;
        at.mute = mute;
        at.solo = solo;
        at.nchan = requiredTracks == 1 ? nchan : 2;

        extractTrackAutomation(track, compLen, at);

        AAFI_foreachTrackItem(track, ti) {
            auto *clip = aafi_timelineItemToAudioClip(ti);
            if ( !clip )
                continue;
            const auto *ptr = getAudioEssencePtr(clip, trackIdx);
            if ( !ptr )
                continue;
            at.clips.push_back(extractClip(clip, ti, track->edit_rate, xFadeMap, ptr));
        }

        result.push_back(std::move(at));
    }

    return result;
}

VideoTrackData AafImporter::extractVideoTrack(const aafiVideoTrack *track) const {
    m_logBuffer.logf(LogEntry::INFO, "Video track %u", track->number);
    VideoTrackData vt;
    const aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        if ( ti->type == AAFI_VIDEO_CLIP )
            vt.clips.push_back(
                extractVideoClip(static_cast<const aafiVideoClip *>(ti->data), track->edit_rate));
    }
    return vt;
}

ClipData AafImporter::extractClip(aafiAudioClip *clip, const aafiTimelineItem *ti,
                                  const aafRational_t *trackEditRate, const XFadeMap &xFadeMap,
                                  const aafiAudioEssencePointer *essPtr) {
    ClipData cd;
    cd.pos = pos_to_seconds(clip->pos, trackEditRate);
    cd.len = pos_to_seconds(clip->len, trackEditRate);
    cd.srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);
    cd.gain = clamp_volume(resolveConstantGain(clip->gain));
    cd.mute = clip->mute != 0 ? 1 : 0;

    const auto [fadeInLen, fadeInShape] = resolveFadeIn(clip, ti, xFadeMap, trackEditRate);
    const auto [fadeOutLen, fadeOutShape] = resolveFadeOut(clip, ti, xFadeMap, trackEditRate);
    cd.fadeInLen = fadeInLen;
    cd.fadeInShape = fadeInShape;
    cd.fadeOutLen = fadeOutLen;
    cd.fadeOutShape = fadeOutShape;

    const char *clipName = resolveClipName(clip);
    cd.name = clipName ? clipName : "";

    if ( clip->mute )
        m_logBuffer.logf(LogEntry::WARN, "Clip \"%s\" is muted",
                         cd.name[0] ? clipName : "(unnamed)");

    // Per-clip varying gain automation
    if ( clip->automation && (clip->automation->flags & AAFI_AUDIO_GAIN_VARIABLE) ) {
        cd.automation = extractEnvelope(clip->automation, cd.len, "VOLENV",
                                        [](const double v) { return clamp_volume(v); });
    }

    if ( essPtr && essPtr->essenceFile ) {
        const aafiAudioEssenceFile *ess = essPtr->essenceFile;
        const char *essName = ess->unique_name ? ess->unique_name
                              : ess->name      ? ess->name
                                               : "(unnamed)";
        char fadeStr[64] = "";
        if ( fadeInLen > 0.0 && fadeOutLen > 0.0 )
            snprintf(fadeStr, sizeof(fadeStr), "  fadeIn: %.3fs  fadeOut: %.3fs", fadeInLen,
                     fadeOutLen);
        else if ( fadeInLen > 0.0 )
            snprintf(fadeStr, sizeof(fadeStr), "  fadeIn: %.3fs", fadeInLen);
        else if ( fadeOutLen > 0.0 )
            snprintf(fadeStr, sizeof(fadeStr), "  fadeOut: %.3fs", fadeOutLen);
        m_logBuffer.logf(LogEntry::INFO, "Source: \"%s\"  %u Hz / %u-bit / %uch  @ %.3fs%s",
                         essName, ess->samplerate, ess->samplesize, ess->channels, cd.pos, fadeStr);
    }

    cd.source = resolveAudioSource(essPtr);
    return cd;
}

VideoClipData AafImporter::extractVideoClip(const aafiVideoClip *clip,
                                            const aafRational_t *trackEditRate) const {
    VideoClipData vc;
    vc.pos = pos_to_seconds(clip->pos, trackEditRate);
    vc.len = pos_to_seconds(clip->len, trackEditRate);
    vc.srcOffset = pos_to_seconds(clip->essence_offset, trackEditRate);
    vc.name = clip->Essence ? clip->Essence->name : "Video";
    vc.source = resolveVideoSource(clip->Essence);
    return vc;
}

bool AafImporter::extractEmbeddedEssence(aafiAudioEssenceFile *ess) {
    if ( !m_extractDirCreated ) {
        if ( !ensure_dir(m_extractDir) ) {
            m_logBuffer.logf(LogEntry::ERR, "could not create extract dir: %s",
                             m_extractDir.c_str());
            return false;
        }
        m_extractDirCreated = true;
    }

    // We don't care about outPath since we then use ess->usable_file_path, but the function needs
    // it so...
    char *outPath = nullptr;
    const int rc = aafi_extractAudioEssenceFile(m_aafi.get(), ess, AAFI_EXTRACT_DEFAULT,
                                                m_extractDir.c_str(), 0, 0, nullptr, &outPath);
    free(outPath);

    if ( rc != 0 ) {
        m_logBuffer.logf(LogEntry::ERR, "failed to extract '%s'",
                         ess->unique_name ? ess->unique_name : "(unnamed)");
        return false;
    }
    m_logBuffer.logf(LogEntry::INFO, "Extracted '%s'", ess->unique_name);
    return true;
}

SourceData AafImporter::resolveAudioSource(const aafiAudioEssencePointer *essPtr) {
    if ( !essPtr || !essPtr->essenceFile ) {
        m_logBuffer.logf(LogEntry::ERR, "Missing source essence in clip");
        return {}; // empty → emptySource()
    }

    aafiAudioEssenceFile *ess = essPtr->essenceFile;

    if ( ess->is_embedded && !ess->usable_file_path ) {
        if ( !extractEmbeddedEssence(ess) ) {
            m_logBuffer.logf(LogEntry::ERR, "'%s' : embedded extraction failed",
                             ess->unique_name ? ess->unique_name : "(unnamed)");
            return {rppSourceTypeFromPath(ess->original_file_path),
                    ess->original_file_path ? ess->original_file_path : ""};
        }
    }

    const char *filePath = ess->usable_file_path;
    if ( !filePath || *filePath == '\0' ) {
        m_logBuffer.logf(LogEntry::WARN, "'%s' has no usable path, using original file path",
                         ess->unique_name ? ess->unique_name : "(unnamed)");
        return {rppSourceTypeFromPath(ess->original_file_path),
                ess->original_file_path ? ess->original_file_path : ""};
    }

    return {rppSourceTypeFromPath(filePath), filePath};
}

SourceData AafImporter::resolveVideoSource(const aafiVideoEssence *ess) const {
    if ( !ess ) {
        m_logBuffer.log(LogEntry::ERR, "Missing source essence in clip");
        return {}; // empty → emptySource()
    }
    if ( !ess->usable_file_path || *ess->usable_file_path == '\0' ) {
        m_logBuffer.logf(LogEntry::WARN, "'%s': Video has no usable path, using original path",
                         ess->unique_name ? ess->unique_name : "(unnamed)");
        return {rppSourceTypeFromPath(ess->original_file_path),
                ess->original_file_path ? ess->original_file_path : ""};
    }

    m_logBuffer.logf(LogEntry::INFO, "Processed video: '%s'", ess->usable_file_path);
    return {"VIDEO", ess->usable_file_path};
}

void AafImporter::extractTrackAutomation(const aafiAudioTrack *track, const double compLen,
                                         AudioTrackData &out) {
    if ( track->gain && (track->gain->flags & AAFI_AUDIO_GAIN_VARIABLE) )
        out.gainEnv = extractEnvelope(track->gain, compLen, "VOLENV2",
                                      [](const double v) { return clamp_volume(v); });

    if ( track->pan && (track->pan->flags & AAFI_AUDIO_GAIN_VARIABLE) )
        out.panEnv = extractEnvelope(
            track->pan, compLen, "PANENV2",
            [](const double v) { return clamp_pan((v - 0.5) * -2.0); },
            /*arm=*/true);
}

std::optional<EnvelopeData>
AafImporter::extractEnvelope(const aafiAudioGain *gain, const double segLenSec, const char *tag,
                             const std::function<double(double)> &transform, const bool arm) {
    if ( !gain )
        return std::nullopt;
    if ( !(gain->flags & AAFI_AUDIO_GAIN_VARIABLE) )
        return std::nullopt;
    if ( gain->pts_cnt == 0 || !gain->time || !gain->value )
        return std::nullopt;

    EnvelopeData env;
    env.tag = tag;
    env.arm = arm;
    env.points.reserve(gain->pts_cnt);

    for ( unsigned int i = 0; i < gain->pts_cnt; ++i ) {
        const double frac = rational_to_double(gain->time[i]); // 0.0 .. 1.0
        const double t = frac * segLenSec;
        const double val = transform(rational_to_double(gain->value[i]));
        env.points.push_back({t, val});
    }

    return env;
}
