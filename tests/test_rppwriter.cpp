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

// Standalone tests for RppWriter using a CapturingSink.
// Does NOT link ReaperSink.cpp or any REAPER SDK source.

#include "RppWriter.h"
#include "IRppSink.h"

#include <catch2/catch_all.hpp>
#include <vector>
#include <string>
#include <algorithm>

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
        return std::any_of(lines.begin(), lines.end(),
                           [&](const std::string &ln) { return ln.find(sub) != std::string::npos; });
    }
};

// ---------------------------------------------------------------------------
// Chunk RAII
// ---------------------------------------------------------------------------

TEST_CASE("Chunk: destructor emits closing >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto t = w.track("X", 1.0, 0.0, 0, 0, 2); }
    REQUIRE(sink.lines.back() == ">");
}

TEST_CASE("Chunk: explicit close() emits > and destructor does not double-close") {
    CapturingSink sink;
    RppWriter w(&sink);
    {
        auto t = w.track("X", 1.0, 0.0, 0, 0, 2);
        t.close();
        // destructor runs here — must not emit a second >
    }
    long closeCount = std::count(sink.lines.begin(), sink.lines.end(), ">");
    REQUIRE(closeCount == 1);
}

TEST_CASE("Chunk: move — moved-from does not double-close") {
    CapturingSink sink;
    RppWriter w(&sink);
    {
        auto t1 = w.track("X", 1.0, 0.0, 0, 0, 2);
        auto t2 = std::move(t1); // t1 is now moved-from
        // t1 destructs here — should NOT emit >
        // t2 destructs at end of block — should emit exactly one >
    }
    long closeCount = std::count(sink.lines.begin(), sink.lines.end(), ">");
    REQUIRE(closeCount == 1);
}

// ---------------------------------------------------------------------------
// track()
// ---------------------------------------------------------------------------

TEST_CASE("track: emits <TRACK header, NAME, VOLPAN, MUTESOLO, NCHAN, >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto t = w.track("Drums", 1.0, 0.0, 0, 0, 2); }

    REQUIRE(sink.lines.at(0) == "<TRACK");
    REQUIRE(sink.contains("NAME \"Drums\""));
    REQUIRE(sink.anyContains("VOLPAN"));
    REQUIRE(sink.anyContains("MUTESOLO"));
    REQUIRE(sink.anyContains("NCHAN 2"));
    REQUIRE(sink.lines.back() == ">");
}

TEST_CASE("track: null name is emitted as empty string") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto t = w.track(nullptr, 1.0, 0.0, 0, 0, 1); }
    REQUIRE(sink.contains("NAME \"\""));
}

TEST_CASE("track: name with double-quotes is escaped") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto t = w.track("say \"hi\"", 1.0, 0.0, 0, 0, 1); }
    REQUIRE(sink.contains("NAME \"say \\\"hi\\\"\""));
}

// ---------------------------------------------------------------------------
// item()
// ---------------------------------------------------------------------------

TEST_CASE("item: emits <ITEM header with POSITION, LENGTH, FADEIN, FADEOUT, >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto i = w.item("clip", 1.0, 2.0, 0.5, 0, 0.25, 1, 1.0, 0.0, 0); }

    REQUIRE(sink.lines.at(0) == "<ITEM");
    REQUIRE(sink.anyContains("NAME \"clip\""));
    REQUIRE(sink.anyContains("POSITION 1.0"));
    REQUIRE(sink.anyContains("LENGTH 2.0"));
    REQUIRE(sink.anyContains("FADEIN 0"));
    REQUIRE(sink.anyContains("FADEOUT 1"));
    REQUIRE(sink.lines.back() == ">");
}

TEST_CASE("item: nullptr for name results in empty name") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto i = w.item(nullptr, 0, 1, 0, 0, 0, 0, 0, 0, 0); }
    REQUIRE(sink.anyContains("NAME \"\""));
}

TEST_CASE("item: mute flag is emitted") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto i = w.item("c", 0.0, 1.0, 0.0, 0, 0.0, 0, 1.0, 0.0, 1); }
    REQUIRE(sink.anyContains("MUTE 1"));
}

// ---------------------------------------------------------------------------
// source()
// ---------------------------------------------------------------------------

TEST_CASE("source: emits <SOURCE TYPE, FILE, >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto s = w.source("WAVE", "/audio/kick.wav"); }

    REQUIRE(sink.lines.at(0) == "<SOURCE WAVE");
    REQUIRE(sink.contains("FILE \"/audio/kick.wav\""));
    REQUIRE(sink.lines.back() == ">");
}

TEST_CASE("source: path with backslashes is escaped") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto s = w.source("WAVE", "C:\\audio\\kick.wav"); }
    REQUIRE(sink.contains("FILE \"C:\\\\audio\\\\kick.wav\""));
}

// ---------------------------------------------------------------------------
// emptySource()
// ---------------------------------------------------------------------------

TEST_CASE("emptySource: emits <SOURCE EMPTY and >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto s = w.emptySource(); }
    REQUIRE(sink.lines.at(0) == "<SOURCE EMPTY");
    REQUIRE(sink.lines.back() == ">");
}

// ---------------------------------------------------------------------------
// envelope()
// ---------------------------------------------------------------------------

TEST_CASE("envelope: emits tag line, VIS, >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto e = w.envelope("VOLENV"); }
    REQUIRE(sink.lines.at(0) == "<VOLENV");
    REQUIRE(sink.contains("VIS 1 1 1"));
    REQUIRE(sink.lines.back() == ">");
}

TEST_CASE("envelope: arm=true emits ARM 1") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto e = w.envelope("VOLENV", true); }
    REQUIRE(sink.contains("ARM 1"));
}

TEST_CASE("envelope: arm=false does not emit ARM 1") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto e = w.envelope("VOLENV", false); }
    REQUIRE(!sink.contains("ARM 1"));
}

// ---------------------------------------------------------------------------
// writeMarker() / writeEnvPoint()
// ---------------------------------------------------------------------------

TEST_CASE("writeMarker: emits MARKER line with all fields, isRegionBoundary=false") {
    CapturingSink sink;
    RppWriter w(&sink);
    w.writeMarker(1, 3.5, "Intro", false, 0);
    REQUIRE(sink.lines.size() == 1);
    REQUIRE(sink.lines.at(0).rfind("MARKER 1", 0) == 0);
    REQUIRE(sink.anyContains("\"Intro\" 0"));
}

TEST_CASE("writeMarker: isRegionBoundary=true sets flag to 1") {
    CapturingSink sink;
    RppWriter w(&sink);
    w.writeMarker(1, 3.5, "Intro", true, 0);
    REQUIRE(sink.lines.size() == 1);
    REQUIRE(sink.anyContains("\"Intro\" 1"));
}

TEST_CASE("writeMarker: null name emits empty quoted string") {
    CapturingSink sink;
    RppWriter w(&sink);
    w.writeMarker(2, 0.0, nullptr, false, 0);
    REQUIRE(sink.lines.size() == 1);
    REQUIRE(sink.anyContains("\"\""));
}

TEST_CASE("writeEnvPoint: emits PT line") {
    CapturingSink sink;
    RppWriter w(&sink);
    w.writeEnvPoint(1.0, 0.5);
    REQUIRE(sink.lines.size() == 1);
    REQUIRE(sink.lines.at(0).rfind("PT", 0) == 0);
}

// ---------------------------------------------------------------------------
// project()
// ---------------------------------------------------------------------------

TEST_CASE("project: emits <REAPER_PROJECT header, key lines, >") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto p = w.project(0.0, 120.0, 25, 0, 48000); }

    REQUIRE(sink.lines.at(0) == "<REAPER_PROJECT 0.1");
    REQUIRE(sink.anyContains("PROJOFFS"));
    REQUIRE(sink.anyContains("MAXPROJLEN"));
    REQUIRE(sink.anyContains("TIMEMODE"));
    REQUIRE(sink.anyContains("SMPTESYNC"));
    REQUIRE(sink.anyContains("SAMPLERATE 48000"));
    REQUIRE(sink.lines.back() == ">");
}

TEST_CASE("project: maxProjLen is padded by 60 seconds") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto p = w.project(0.0, 100.0, 25, 0, 44100); }
    // MAXPROJLEN should contain 160.0 (100 + 60)
    REQUIRE(sink.anyContains("MAXPROJLEN 0 160."));
}

TEST_CASE("project: fps and isDrop are forwarded") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto p = w.project(0.0, 60.0, 30, 1, 48000); }
    REQUIRE(sink.anyContains("TIMEMODE 1 5 -1 30 1"));
    REQUIRE(sink.anyContains("SMPTESYNC 0 30"));
}

TEST_CASE("project: tcOffsetSec is forwarded to PROJOFFS") {
    CapturingSink sink;
    RppWriter w(&sink);
    { auto p = w.project(3.5, 60.0, 25, 0, 48000); }
    REQUIRE(sink.anyContains("PROJOFFS 3.5"));
}

// ---------------------------------------------------------------------------
// line() large-string path (buf overflow → heap allocation)
// ---------------------------------------------------------------------------

TEST_CASE("line: string exceeding 8192-byte stack buffer is emitted in full") {
    // NAME \"<name>\" must exceed 8192 bytes.
    // Format is: NAME "<escaped_name>" — overhead is 8 bytes, so name of 8192 chars overflows.
    const std::string big(8200, 'A');
    CapturingSink sink;
    RppWriter w(&sink);
    { auto t = w.track(big.c_str(), 1.0, 0.0, 0, 0, 1); }

    // Find the NAME line and verify the full name was written
    const std::string expected = "NAME \"" + big + "\"";
    REQUIRE(sink.contains(expected));
}

// ---------------------------------------------------------------------------
// setErrorHandler
// ---------------------------------------------------------------------------

TEST_CASE("setErrorHandler: handler is not called during normal writes") {
    CapturingSink sink;
    RppWriter w(&sink);
    bool called = false;
    w.setErrorHandler([&](RppWriter::ErrorKind, const char *) { called = true; });
    { auto t = w.track("Name", 1.0, 0.0, 0, 0, 2); }
    REQUIRE(!called);
}

TEST_CASE("setErrorHandler: handler is not called when large-string heap path is taken") {
    // The heap path (RppWriter.cpp:43-48) handles oversized lines successfully —
    // it is NOT an error condition, so the handler must not fire.
    CapturingSink sink;
    RppWriter w(&sink);
    bool called = false;
    w.setErrorHandler([&](RppWriter::ErrorKind, const char *) { called = true; });
    const std::string big(8200, 'B');
    { auto t = w.track(big.c_str(), 1.0, 0.0, 0, 0, 1); }
    REQUIRE(!called);
    REQUIRE(sink.contains("NAME \"" + big + "\""));
}

TEST_CASE("setErrorHandler: handler can be replaced mid-use") {
    CapturingSink sink;
    RppWriter w(&sink);
    int calls_a = 0, calls_b = 0;
    w.setErrorHandler([&](RppWriter::ErrorKind, const char *) { ++calls_a; });
    { auto t = w.track("A", 1.0, 0.0, 0, 0, 1); }
    w.setErrorHandler([&](RppWriter::ErrorKind, const char *) { ++calls_b; });
    { auto t = w.track("B", 1.0, 0.0, 0, 0, 1); }
    REQUIRE(calls_a == 0);
    REQUIRE(calls_b == 0);
}

// NOTE: the m_onError invocation path (RppWriter.cpp:52-55) fires only when
// vsnprintf returns negative. On POSIX this requires an encoding error and is not
// triggerable through the public API without mocking vsnprintf.
// Clearing the handler via setErrorHandler({}) cannot be meaningfully tested
// for crash safety: normal writes never reach the if (m_onError) guard at all.

// ---------------------------------------------------------------------------
// Nesting — track > item > source
// ---------------------------------------------------------------------------

TEST_CASE("nesting: track > item > source emits correct open/close order") {
    CapturingSink sink;
    RppWriter w(&sink);
    {
        auto t = w.track("T", 1.0, 0.0, 0, 0, 2);
        {
            auto i = w.item("C", 0.0, 1.0, 0.0, 0, 0.0, 0, 1.0, 0.0, 0);
            { auto s = w.source("WAVE", "/f.wav"); }
        }
    }
    // Must start with <TRACK and end with > (track close)
    REQUIRE(sink.lines.front() == "<TRACK");
    REQUIRE(sink.lines.back() == ">");
    // Count three closing >
    long closeCount = std::count(sink.lines.begin(), sink.lines.end(), ">");
    REQUIRE(closeCount == 3);
}
