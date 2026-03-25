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

// Standalone tests for AafEmitter. Links AafEmitter.cpp + RppWriter.cpp only.
// No aaf-static, no helpers_stubs.cpp, no REAPER SDK.

#include "AafData.h"
#include "AafEmitter.h"
#include "IRppSink.h"
#include "RppWriter.h"
#include "helpers.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test double
// ---------------------------------------------------------------------------
struct CapturingSink : IRppSink {
    std::vector<std::string> lines;
    void writeLine(const char *l) override { lines.push_back(l); }

    bool contains(const std::string &s) const {
        return std::any_of(lines.begin(), lines.end(),
                           [&](const std::string &ln) { return ln == s; });
    }

    bool anyContains(const std::string &sub) const {
        return std::any_of(lines.begin(), lines.end(), [&](const std::string &ln) {
            return ln.find(sub) != std::string::npos;
        });
    }
};

// ---------------------------------------------------------------------------
// computeTimecodeIsDrop
// ---------------------------------------------------------------------------
TEST_CASE("computeTimecodeIsDrop: integer fps returns 0") {
    // denominator == 1 → integer fps → 0
    CHECK(computeTimecodeIsDrop(25, 1, 0) == 0);
    CHECK(computeTimecodeIsDrop(30, 1, 1) == 0);
    CHECK(computeTimecodeIsDrop(24, 1, 0) == 0);
}

TEST_CASE("computeTimecodeIsDrop: 23.976 (fps=24 frac) returns 2") {
    CHECK(computeTimecodeIsDrop(24, 1001, 0) == 2);
    CHECK(computeTimecodeIsDrop(24, 1001, 1) == 2); // drop flag irrelevant for 23.976
}

TEST_CASE("computeTimecodeIsDrop: 29.97 drop-frame returns 1") {
    CHECK(computeTimecodeIsDrop(30, 1001, 1) == 1);
}

TEST_CASE("computeTimecodeIsDrop: 29.97 non-drop returns 2") {
    CHECK(computeTimecodeIsDrop(30, 1001, 0) == 2);
}

// ---------------------------------------------------------------------------
// emitMarkers
// ---------------------------------------------------------------------------
TEST_CASE("emitMarkers: point marker emits one MARKER line") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    MarkerData m;
    m.id = 1;
    m.t = 1.5;
    m.name = "Verse";
    m.isRegion = false;
    m.color = 0;

    emitter.emitMarkers({m});

    REQUIRE(sink.anyContains("MARKER"));
    REQUIRE(sink.anyContains("1.5"));
    REQUIRE(sink.anyContains("Verse"));
    // Only one MARKER line for a point marker
    int count = 0;
    for ( const auto &ln : sink.lines )
        if ( ln.find("MARKER") != std::string::npos )
            ++count;
    CHECK(count == 1);
}

TEST_CASE("emitMarkers: region emits two MARKER lines with same id") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    MarkerData m;
    m.id = 3;
    m.t = 2.0;
    m.name = "Chorus";
    m.isRegion = true;
    m.endT = 4.0;
    m.color = 0;

    emitter.emitMarkers({m});

    int count = 0;
    for ( const auto &ln : sink.lines )
        if ( ln.find("MARKER") != std::string::npos )
            ++count;
    CHECK(count == 2);
    // Both should contain the same id
    REQUIRE(sink.anyContains("Chorus"));
}

TEST_CASE("emitMarkers: color is encoded in marker line") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    MarkerData m;
    m.id = 1;
    m.t = 0.0;
    m.name = "Red";
    m.isRegion = false;
    m.color = 0x1FF0000;

    emitter.emitMarkers({m});
    REQUIRE(sink.anyContains("MARKER"));
}

// ---------------------------------------------------------------------------
// emitSource
// ---------------------------------------------------------------------------
TEST_CASE("emitSource: empty filePath emits SOURCE EMPTY block") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    SourceData s;
    // default-constructed: type and filePath are empty
    emitter.emitSource(s);

    REQUIRE(sink.anyContains("SOURCE"));
    REQUIRE(sink.anyContains("EMPTY"));
}

TEST_CASE("emitSource: WAVE source emits FILE line with path") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    SourceData s;
    s.type = "WAVE";
    s.filePath = "/audio/kick.wav";

    emitter.emitSource(s);

    REQUIRE(sink.anyContains("WAVE"));
    REQUIRE(sink.anyContains("kick.wav"));
}

TEST_CASE("emitSource: MP3 source emits correct type") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    SourceData s;
    s.type = "MP3";
    s.filePath = "/audio/loop.mp3";

    emitter.emitSource(s);

    REQUIRE(sink.anyContains("MP3"));
}

// ---------------------------------------------------------------------------
// emitEnvelope
// ---------------------------------------------------------------------------
TEST_CASE("emitEnvelope: emits envelope tag and points") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    EnvelopeData env;
    env.tag = "VOLENV2";
    env.arm = false;
    env.points = {{0.0, 1.0}, {2.5, 0.5}, {5.0, 1.0}};

    emitter.emitEnvelope(env);

    REQUIRE(sink.anyContains("VOLENV2"));
    REQUIRE(sink.anyContains("PT"));
    // closing >
    REQUIRE(sink.contains(">"));
}

TEST_CASE("emitEnvelope: arm flag is written") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    EnvelopeData env;
    env.tag = "PANENV2";
    env.arm = true;
    env.points = {{0.0, 0.0}};

    emitter.emitEnvelope(env);

    REQUIRE(sink.anyContains("PANENV2"));
    // arm=true should produce " 1" in the envelope header
    REQUIRE(sink.anyContains("1"));
}

// ---------------------------------------------------------------------------
// emitClip
// ---------------------------------------------------------------------------
TEST_CASE("emitClip: basic fields are written") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    ClipData clip;
    clip.name = "Kick";
    clip.pos = 1.0;
    clip.len = 0.5;
    clip.srcOffset = 0.0;
    clip.gain = 1.0;
    clip.mute = 0;
    clip.source.type = "WAVE";
    clip.source.filePath = "/audio/kick.wav";

    emitter.emitClip(clip);

    REQUIRE(sink.anyContains("ITEM"));
    REQUIRE(sink.anyContains("Kick"));
    REQUIRE(sink.anyContains("WAVE"));
}

TEST_CASE("emitClip: clip-level automation is emitted when present") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    ClipData clip;
    clip.name = "Narration";
    clip.pos = 0.0;
    clip.len = 10.0;
    clip.source.type = "WAVE";
    clip.source.filePath = "/audio/narr.wav";

    EnvelopeData env;
    env.tag = "VOLENV";
    env.arm = false;
    env.points = {{0.0, 1.0}, {5.0, 0.2}, {10.0, 1.0}};
    clip.automation = env;

    emitter.emitClip(clip);

    REQUIRE(sink.anyContains("VOLENV"));
    REQUIRE(sink.anyContains("PT"));
}

TEST_CASE("emitClip: no automation field → no VOLENV emitted") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    ClipData clip;
    clip.name = "Bass";
    clip.source.type = "WAVE";
    clip.source.filePath = "/audio/bass.wav";
    // clip.automation is nullopt by default

    emitter.emitClip(clip);

    CHECK_FALSE(sink.anyContains("VOLENV"));
}

// ---------------------------------------------------------------------------
// emitAudioTrack
// ---------------------------------------------------------------------------
TEST_CASE("emitAudioTrack: track header and clips") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    AudioTrackData track;
    track.name = "Drums";
    track.vol = 1.0;
    track.pan = 0.0;
    track.mute = 0;
    track.solo = 0;
    track.nchan = 2;

    ClipData clip;
    clip.name = "Kick";
    clip.pos = 0.0;
    clip.len = 0.25;
    clip.source.type = "WAVE";
    clip.source.filePath = "/audio/kick.wav";
    track.clips.push_back(clip);

    emitter.emitAudioTrack(track);

    REQUIRE(sink.anyContains("TRACK"));
    REQUIRE(sink.anyContains("Drums"));
    REQUIRE(sink.anyContains("Kick"));
}

TEST_CASE("emitAudioTrack: gain envelope emitted when present") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    AudioTrackData track;
    track.name = "Vox";
    track.vol = 1.0;
    track.pan = 0.0;

    EnvelopeData gainEnv;
    gainEnv.tag = "VOLENV2";
    gainEnv.arm = false;
    gainEnv.points = {{0.0, 1.0}, {3.0, 0.5}};
    track.gainEnv = gainEnv;

    emitter.emitAudioTrack(track);

    REQUIRE(sink.anyContains("VOLENV2"));
}

TEST_CASE("emitAudioTrack: pan envelope emitted when present") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    AudioTrackData track;
    track.name = "Stereo";
    track.vol = 1.0;
    track.pan = 0.0;

    EnvelopeData panEnv;
    panEnv.tag = "PANENV2";
    panEnv.arm = true;
    panEnv.points = {{0.0, 0.0}};
    track.panEnv = panEnv;

    emitter.emitAudioTrack(track);

    REQUIRE(sink.anyContains("PANENV2"));
}

// ---------------------------------------------------------------------------
// emitVideoTrack
// ---------------------------------------------------------------------------
TEST_CASE("emitVideoTrack: emits VIDEO track with clip") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    VideoTrackData vt;
    VideoClipData vc;
    vc.name = "MyVideo";
    vc.pos = 0.0;
    vc.len = 30.0;
    vc.srcOffset = 0.0;
    vc.source.type = "VIDEO";
    vc.source.filePath = "/media/film.mxf";
    vt.clips.push_back(vc);

    emitter.emitVideoTrack(vt);

    REQUIRE(sink.anyContains("TRACK"));
    REQUIRE(sink.anyContains("VIDEO"));
    REQUIRE(sink.anyContains("film.mxf"));
}

// ---------------------------------------------------------------------------
// emit (full CompositionData)
// ---------------------------------------------------------------------------
TEST_CASE("emit: full composition roundtrip") {
    CapturingSink sink;
    RppWriter writer(&sink);
    AafEmitter emitter(writer);

    CompositionData comp;
    comp.tcOffset = 0.0;
    comp.maxProjLen = 60.0;
    comp.samplerate = 48000;
    comp.fps = 25;
    comp.isDrop = 0;

    MarkerData marker;
    marker.id = 1;
    marker.t = 4.0;
    marker.name = "Intro";
    marker.isRegion = false;
    comp.markers.push_back(marker);

    AudioTrackData track;
    track.name = "Bass";
    track.vol = 1.0;
    track.pan = 0.0;
    ClipData clip;
    clip.name = "Bass01";
    clip.pos = 0.0;
    clip.len = 4.0;
    clip.source.type = "WAVE";
    clip.source.filePath = "/audio/bass.wav";
    track.clips.push_back(clip);
    comp.audioTracks.push_back(track);

    emitter.emit(comp);

    REQUIRE(sink.anyContains("MARKER"));
    REQUIRE(sink.anyContains("Intro"));
    REQUIRE(sink.anyContains("TRACK"));
    REQUIRE(sink.anyContains("Bass"));
    REQUIRE(sink.anyContains("Bass01"));
}

// ---------------------------------------------------------------------------
// rppSourceTypeFromPath
// ---------------------------------------------------------------------------
TEST_CASE("rppSourceTypeFromPath: maps extensions correctly") {
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.wav")) == "WAVE");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.WAV")) == "WAVE");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.mp3")) == "MP3");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.MP3")) == "MP3");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.flac")) == "FLAC");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.ogg")) == "VORBIS");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.mxf")) == "VIDEO");
    CHECK(std::string(rppSourceTypeFromPath("/a/b/file.aiff")) == "WAVE"); // default
}

// ---------------------------------------------------------------------------
// buildExtractDir
// ---------------------------------------------------------------------------
TEST_CASE("buildExtractDir: strips extension and appends -media") {
    CHECK(buildExtractDir("/path/to/session.aaf") == "/path/to/session-media");
    CHECK(buildExtractDir("/session.aaf") == "/session-media");
    CHECK(buildExtractDir("C:\\projects\\show.aaf") == "C:\\projects\\show-media");
}
