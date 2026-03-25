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

#include "AafEmitter.h"

AafEmitter::AafEmitter(RppWriter &writer) : m_writer(writer) {}

void AafEmitter::emit(const CompositionData &comp) const {
    emitMarkers(comp.markers);
    for ( const auto &vt : comp.videoTracks )
        emitVideoTrack(vt);
    for ( const auto &at : comp.audioTracks )
        emitAudioTrack(at);
}

void AafEmitter::emitMarkers(const std::vector<MarkerData> &markers) const {
    for ( const auto &[id, t, name, isRegion, endT, color] : markers ) {
        m_writer.writeMarker(id, t, name.c_str(), isRegion, color);
        if ( isRegion )
            m_writer.writeMarker(id, endT, "", true, color);
    }
}

void AafEmitter::emitAudioTrack(const AudioTrackData &track) const {
    auto w_trk = m_writer.track(track.name.c_str(), track.vol, track.pan, track.mute, track.solo,
                                track.nchan);
    if ( track.gainEnv )
        emitEnvelope(*track.gainEnv);
    if ( track.panEnv )
        emitEnvelope(*track.panEnv);
    for ( const auto &clip : track.clips )
        emitClip(clip);
}

void AafEmitter::emitVideoTrack(const VideoTrackData &track) const {
    auto w_trk = m_writer.track("VIDEO", 1.0, 0.0, 0, 0, 1);
    for ( const auto &clip : track.clips )
        emitVideoClip(clip);
}

void AafEmitter::emitClip(const ClipData &clip) const {
    auto w_itm =
        m_writer.item(clip.name.c_str(), clip.pos, clip.len, clip.fadeInLen, clip.fadeInShape,
                      clip.fadeOutLen, clip.fadeOutShape, clip.gain, clip.srcOffset, clip.mute);
    if ( clip.automation )
        emitEnvelope(*clip.automation);
    emitSource(clip.source);
}

void AafEmitter::emitEnvelope(const EnvelopeData &env) const {
    auto w_env = m_writer.envelope(env.tag.c_str(), env.arm);
    for ( const auto &[t, value] : env.points )
        m_writer.writeEnvPoint(t, value);
}

void AafEmitter::emitSource(const SourceData &source) const {
    if ( source.filePath.empty() )
        m_writer.emptySource();
    else {
        auto w_src = m_writer.source(source.type.c_str(), source.filePath.c_str());
    }
}

void AafEmitter::emitVideoClip(const VideoClipData &clip) const {
    auto w_itm = m_writer.item(clip.name.c_str(), clip.pos, clip.len, 0.0, 0, 0.0, 0, 1.0,
                               clip.srcOffset, 0);
    emitSource(clip.source);
}
