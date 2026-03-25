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

#ifndef REAPER_AAF_AAFEMITTER_H
#define REAPER_AAF_AAFEMITTER_H

#include <vector>

#include "AafData.h"
#include "RppWriter.h"

// ---------------------------------------------------------------------------
// AafEmitter — pure RPP emission from AafData structs
//
// No LibAAF dependency. All data quality problems are caught at extraction
// time (AafImporter); this class only converts already-clean data to RPP.
// ---------------------------------------------------------------------------
class AafEmitter {
  public:
    explicit AafEmitter(RppWriter &writer);

    // Emit markers + all video and audio tracks (no outer PROJECT chunk).
    void emit(const CompositionData &comp) const;

    void emitMarkers(const std::vector<MarkerData> &markers) const;
    void emitAudioTrack(const AudioTrackData &track) const;
    void emitVideoTrack(const VideoTrackData &track) const;
    void emitClip(const ClipData &clip) const;
    void emitEnvelope(const EnvelopeData &env) const;
    void emitSource(const SourceData &source) const;

  private:
    RppWriter &m_writer;

    void emitVideoClip(const VideoClipData &clip) const;
};

#endif // REAPER_AAF_AAFEMITTER_H
