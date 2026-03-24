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
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("LogBuffer stores all severities") {
    TestableLogBuffer buf;
    buf.log(LogEntry::INFO,  "info");
    buf.log(LogEntry::WARN,  "warn");
    buf.log(LogEntry::ERR, "error");
    REQUIRE(buf.count() == 3);
}

TEST_CASE("LogBuffer fills to capacity") {
    TestableLogBuffer buf;
    for (int i = 0; i < LogBuffer::kCapacity; ++i)
        buf.log(LogEntry::INFO, "fill");
    REQUIRE(buf.count() == LogBuffer::kCapacity);
}

TEST_CASE("LogBuffer ring overflow: sentinel inserted, count stays at capacity") {
    // On kCapacity+1 push: oldest entry evicted, a sentinel WARN is inserted,
    // new entry stored — count stays at kCapacity.
    TestableLogBuffer buf;
    for (int i = 0; i < LogBuffer::kCapacity; ++i)
        buf.log(LogEntry::INFO, "fill");

    buf.log(LogEntry::ERR, "overflow entry");

    REQUIRE(buf.count() == LogBuffer::kCapacity);

    const LogEntry sentinel = buf.entryAt(buf.count() - 2);
    const LogEntry newest   = buf.entryAt(buf.count() - 1);

    SECTION("sentinel severity is WARN") {
        REQUIRE(sentinel.severity == LogEntry::WARN);
    }
    SECTION("sentinel text matches exact format") {
        REQUIRE(sentinel.text == "1 earlier entry was dropped (buffer full)");
    }
    SECTION("newest entry is the overflow entry") {
        REQUIRE(newest.text == "overflow entry");
    }
}
