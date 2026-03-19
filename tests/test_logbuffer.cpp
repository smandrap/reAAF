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

// Standalone test binary for LogBuffer — no REAPER SDK required.
// Compile: cmake --build build --target test_logbuffer
// Run:     ./build/test_logbuffer

#include "LogBuffer.h"

#include <cstdio>
#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

static void check(bool condition, const char* test_name)
{
    if (condition) {
        printf("  PASS: %s\n", test_name);
        ++g_passed;
    } else {
        printf("  FAIL: %s\n", test_name);
        ++g_failed;
    }
}

// ---------------------------------------------------------------------------
// Helper: peek at internal state via a derived class so we don't break
// encapsulation in production, but can verify ring buffer state in tests.
// ---------------------------------------------------------------------------
class TestableLogBuffer : public LogBuffer {
public:
    // Expose count for verification.
    // We add a public accessor in LogBuffer for testing purposes (size()).
    // If LogBuffer::size() doesn't exist, this will fail to compile — RED.
    int count() const { return size(); }
    // Expose entry at a logical index (0 = oldest, count-1 = newest).
    LogEntry entryAt(int idx) const { return at(idx); }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_stores_all_severities()
{
    // All severities are stored regardless — no filtering at buffer level
    TestableLogBuffer buf;
    buf.log(LogEntry::INFO,  "info");
    buf.log(LogEntry::WARN,  "warn");
    buf.log(LogEntry::ERROR, "error");
    check(buf.count() == 3, "All severities stored");
}

static void test_buffer_full_at_capacity()
{
    TestableLogBuffer buf;
    for (int i = 0; i < LogBuffer::kCapacity; ++i) {
        buf.log(LogEntry::INFO, "fill");
    }
    check(buf.count() == LogBuffer::kCapacity, "Buffer: count == kCapacity after kCapacity pushes");
}

static void test_ring_overflow_eviction_and_sentinel()
{
    // On kCapacity+1 push: oldest entry evicted; a sentinel [WARN]
    // "1 earlier entry was dropped (buffer full)" is inserted; new entry also
    // stored (count stays at kCapacity).
    TestableLogBuffer buf;

    for (int i = 0; i < LogBuffer::kCapacity; ++i) {
        buf.log(LogEntry::INFO, "fill");
    }

    buf.log(LogEntry::ERROR, "overflow entry");

    check(buf.count() == LogBuffer::kCapacity, "Overflow: count stays at kCapacity");

    LogEntry sentinel = buf.entryAt(buf.count() - 2);
    LogEntry newest   = buf.entryAt(buf.count() - 1);

    check(sentinel.severity == LogEntry::WARN,
          "Overflow sentinel: severity is WARN");
    check(sentinel.text == "1 earlier entry was dropped (buffer full)",
          "Overflow sentinel: text matches exact format");
    check(newest.text == "overflow entry",
          "Overflow: newest entry is the pushed entry");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    printf("=== LogBuffer Tests ===\n\n");

    test_stores_all_severities();
    test_buffer_full_at_capacity();
    test_ring_overflow_eviction_and_sentinel();

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
