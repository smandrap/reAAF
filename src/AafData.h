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

#ifndef REAPER_AAF_AAFDATA_H
#define REAPER_AAF_AAFDATA_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// AafData — project-owned intermediate representation
//
// Plain C++ structs; no LibAAF or REAPER dependency.
// Contract between AafImporter (extraction) and AafEmitter (emission).
// ---------------------------------------------------------------------------

struct EnvPoint {
    double t;
    double value;
};

struct EnvelopeData {
    std::string tag; // "VOLENV2", "PANENV2", "VOLENV"
    bool arm = false;
    std::vector<EnvPoint> points;
};

struct SourceData {
    std::string type; // "WAVE", "MP3", "FLAC", "VORBIS", "VIDEO"
    std::string filePath; // empty → emptySource()
};

struct ClipData {
    std::string name;
    double pos = 0.0;
    double len = 0.0;
    double srcOffset = 0.0;
    double gain = 1.0;
    int mute = 0;
    double fadeInLen = 0.0;
    int fadeInShape = 0;
    double fadeOutLen = 0.0;
    int fadeOutShape = 0;
    std::optional<EnvelopeData> automation; // clip-level VOLENV
    SourceData source;
};

struct AudioTrackData {
    std::string name;
    double vol = 1.0;
    double pan = 0.0;
    int mute = 0;
    int solo = 0;
    int nchan = 2;
    std::vector<ClipData> clips;
    std::optional<EnvelopeData> gainEnv;
    std::optional<EnvelopeData> panEnv;
};

struct MarkerData {
    int id = 0;
    double t = 0.0;
    std::string name;
    bool isRegion = false;
    double endT = 0.0; // only if isRegion
    int color = 0;
};

struct VideoClipData {
    std::string name;
    double pos = 0.0;
    double len = 0.0;
    double srcOffset = 0.0;
    SourceData source;
};

struct VideoTrackData {
    std::vector<VideoClipData> clips;
};

struct CompositionData {
    double tcOffset = 0.0;
    double maxProjLen = 0.0;
    uint32_t samplerate = 48000;
    int fps = 25;
    uint8_t isDrop = 0;
    std::vector<MarkerData> markers;
    std::vector<VideoTrackData> videoTracks;
    std::vector<AudioTrackData> audioTracks;
};

#endif // REAPER_AAF_AAFDATA_H
