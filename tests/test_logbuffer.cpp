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

// Standalone tests for LogBuffer and LogEntry formatting.

#include "LogBuffer.h"

#include <catch2/catch_all.hpp>

// ---------------------------------------------------------------------------
// Helper: peek at internal state via a derived class so we don't break
// encapsulation in production, but can verify ring buffer state in tests.
// ---------------------------------------------------------------------------
class TestableLogBuffer : public LogBuffer {
  public:
    int count() const { return size(); }
    LogEntry entryAt(int idx) const { return at(idx); }
};

// ---------------------------------------------------------------------------
// formatEntry — formats a LogEntry as a displayable string with severity prefix.
// ---------------------------------------------------------------------------

static std::string formatEntry(const LogEntry &e) {
    const char *prefix = "";
    switch ( e.severity ) {
    case LogEntry::ERR:
        prefix = "[ERROR]";
        break;
    case LogEntry::WARN:
        prefix = "[WARN]";
        break;
    case LogEntry::INFO:
        prefix = "[INFO]";
        break;
    case LogEntry::DEBUG:
        prefix = "[DEBUG]";
        break;
    }
    return std::string(prefix) + " " + e.text;
}

// ---------------------------------------------------------------------------
// formatEntry()
// ---------------------------------------------------------------------------

TEST_CASE("formatEntry: ERROR prefix") {
    LogEntry e;
    e.severity = LogEntry::ERR;
    e.text = "disk error";
    REQUIRE(formatEntry(e) == "[ERROR] disk error");
}

TEST_CASE("formatEntry: WARN prefix") {
    LogEntry e;
    e.severity = LogEntry::WARN;
    e.text = "missing file";
    REQUIRE(formatEntry(e) == "[WARN] missing file");
}

TEST_CASE("formatEntry: INFO prefix") {
    LogEntry e;
    e.severity = LogEntry::INFO;
    e.text = "Starting import...";
    REQUIRE(formatEntry(e) == "[INFO] Starting import...");
}

// ---------------------------------------------------------------------------
// log()
// ---------------------------------------------------------------------------

TEST_CASE("LogBuffer stores all severities") {
    TestableLogBuffer buf;
    buf.log(LogEntry::INFO, "info");
    buf.log(LogEntry::WARN, "warn");
    buf.log(LogEntry::ERR, "error");
    REQUIRE(buf.count() == 3);
}

TEST_CASE("LogBuffer fills to capacity") {
    TestableLogBuffer buf;
    for ( int i = 0; i < LogBuffer::kCapacity; ++i )
        buf.log(LogEntry::INFO, "fill");
    REQUIRE(buf.count() == LogBuffer::kCapacity);
}

// ---------------------------------------------------------------------------
// logf()
// ---------------------------------------------------------------------------

TEST_CASE("logf: formats and stores a simple message") {
    TestableLogBuffer buf;
    buf.logf(LogEntry::INFO, "track %d of %d", 3, 10);
    REQUIRE(buf.count() == 1);
    REQUIRE(buf.entryAt(0).text == "track 3 of 10");
}

TEST_CASE("logf: stores the correct severity") {
    TestableLogBuffer buf;
    buf.logf(LogEntry::ERR, "bad path: %s", "/missing/file.wav");
    REQUIRE(buf.entryAt(0).severity == LogEntry::ERR);
}

TEST_CASE("logf: string exceeding 512-byte stack buffer is stored in full via heap path") {
    // The stack buf in logf is 512 bytes. A format result > 511 chars triggers
    // the heap allocation fallback. Overhead for "x%s" is 1 byte, so 512 'A's
    // push written to 512 which equals sizeof(buf) and takes the heap path.
    const std::string big(512, 'A');
    TestableLogBuffer buf;
    buf.logf(LogEntry::WARN, "x%s", big.c_str());
    REQUIRE(buf.count() == 1);
    REQUIRE(buf.entryAt(0).text == "x" + big);
}

// ---------------------------------------------------------------------------
// hasErrorsOrWarnings()
// ---------------------------------------------------------------------------

TEST_CASE("hasErrorsOrWarnings: false on empty buffer") {
    TestableLogBuffer buf;
    REQUIRE_FALSE(buf.hasErrorsOrWarnings());
}

TEST_CASE("hasErrorsOrWarnings: false after INFO-only entries") {
    TestableLogBuffer buf;
    buf.log(LogEntry::INFO, "ok");
    REQUIRE_FALSE(buf.hasErrorsOrWarnings());
}

TEST_CASE("hasErrorsOrWarnings: true after a WARN entry") {
    TestableLogBuffer buf;
    buf.log(LogEntry::WARN, "something odd");
    REQUIRE(buf.hasErrorsOrWarnings());
}

TEST_CASE("hasErrorsOrWarnings: true after an ERR entry") {
    TestableLogBuffer buf;
    buf.log(LogEntry::ERR, "something bad");
    REQUIRE(buf.hasErrorsOrWarnings());
}

TEST_CASE("hasErrorsOrWarnings: true once set, stays true after subsequent INFO") {
    TestableLogBuffer buf;
    buf.log(LogEntry::ERR, "bad");
    buf.log(LogEntry::INFO, "recovered");
    REQUIRE(buf.hasErrorsOrWarnings());
}

// ---------------------------------------------------------------------------
// ring overflow
// ---------------------------------------------------------------------------

TEST_CASE("LogBuffer ring overflow: sentinel inserted, count stays at capacity") {
    // Fill to capacity then push one more. The ring evicts m_entries[0], writes
    // the sentinel there, then writes the new entry at m_entries[1].
    TestableLogBuffer buf;
    for ( size_t i = 0; i < LogBuffer::kCapacity; ++i )
        buf.log(LogEntry::INFO, "fill");

    buf.log(LogEntry::ERR, "overflow entry");

    REQUIRE(buf.count() == LogBuffer::kCapacity);

    const LogEntry sentinel = buf.entryAt(0);
    const LogEntry newest = buf.entryAt(1);

    SECTION("sentinel severity is WARN") { REQUIRE(sentinel.severity == LogEntry::WARN); }
    SECTION("sentinel text matches exact format") {
        REQUIRE(sentinel.text == "1 earlier entry was dropped (buffer full)");
    }
    SECTION("newest entry is the overflow entry") { REQUIRE(newest.text == "overflow entry"); }
}
