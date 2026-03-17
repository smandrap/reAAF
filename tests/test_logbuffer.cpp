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

static void test_verbosity_normal_passes_warn()
{
    // push(WARN, "msg") with verbosity=1 (Normal): entry stored
    TestableLogBuffer buf;
    buf.setVerbosity(1);
    buf.log(LogEntry::WARN, "warn message");
    check(buf.count() == 1, "Normal verbosity: WARN is stored");
}

static void test_verbosity_normal_drops_info()
{
    // push(INFO, "msg") with verbosity=1 (Normal): entry NOT stored
    TestableLogBuffer buf;
    buf.setVerbosity(1);
    buf.log(LogEntry::INFO, "info message");
    check(buf.count() == 0, "Normal verbosity: INFO is dropped");
}

static void test_verbosity_normal_drops_clip()
{
    // push(CLIP, "msg") with verbosity=1 (Normal): entry NOT stored
    TestableLogBuffer buf;
    buf.setVerbosity(1);
    buf.log(LogEntry::CLIP, "clip message");
    check(buf.count() == 0, "Normal verbosity: CLIP is dropped");
}

static void test_verbosity_none_drops_error()
{
    // push(ERROR, "msg") with verbosity=0 (None): entry NOT stored
    TestableLogBuffer buf;
    buf.setVerbosity(0);
    buf.log(LogEntry::ERROR, "error message");
    check(buf.count() == 0, "None verbosity: ERROR is dropped");
}

static void test_verbosity_verbose_stores_all()
{
    // push(any, "msg") with verbosity=2 (Verbose): entry stored regardless
    TestableLogBuffer buf;
    buf.setVerbosity(2);
    buf.log(LogEntry::INFO,  "info");
    buf.log(LogEntry::CLIP,  "clip");
    buf.log(LogEntry::WARN,  "warn");
    buf.log(LogEntry::ERROR, "error");
    check(buf.count() == 4, "Verbose verbosity: all severities stored");
}

static void test_clip_name_stored()
{
    // push() with clipName="Kick_01": stored entry has clipName=="Kick_01"
    TestableLogBuffer buf;
    buf.setVerbosity(2);
    buf.log(LogEntry::WARN, "kick warn", "Kick_01");
    check(buf.count() == 1, "clipName: entry stored");
    check(buf.entryAt(0).clipName == "Kick_01", "clipName: value is Kick_01");
}

static void test_clip_name_empty_when_not_provided()
{
    // push() with no clipName arg: stored entry has clipName==""
    TestableLogBuffer buf;
    buf.setVerbosity(2);
    buf.log(LogEntry::WARN, "no clip");
    check(buf.count() == 1, "no clipName: entry stored");
    check(buf.entryAt(0).clipName.empty(), "no clipName: clipName is empty string");
}

static void test_buffer_full_at_capacity()
{
    // After 2000 pushes (all pass verbosity filter): count == 2000
    TestableLogBuffer buf;
    buf.setVerbosity(2);
    for (int i = 0; i < LogBuffer::kCapacity; ++i) {
        buf.log(LogEntry::INFO, "fill");
    }
    check(buf.count() == LogBuffer::kCapacity, "Buffer: count == kCapacity after kCapacity pushes");
}

static void test_ring_overflow_eviction_and_sentinel()
{
    // On 2001st push (passes filter): oldest entry evicted; a sentinel [WARN]
    // entry with text containing "1 earlier entries were dropped (buffer full)"
    // is inserted; new entry also stored (count stays at 2000)
    TestableLogBuffer buf;
    buf.setVerbosity(2);

    // Fill to capacity
    for (int i = 0; i < LogBuffer::kCapacity; ++i) {
        buf.log(LogEntry::INFO, "fill");
    }

    // Push one more — triggers overflow sentinel
    buf.log(LogEntry::ERROR, "overflow entry");

    check(buf.count() == LogBuffer::kCapacity, "Overflow: count stays at kCapacity");

    // The second-to-last entry should be the sentinel (WARN, "1 earlier entries were dropped (buffer full)")
    // The last entry should be "overflow entry"
    // Sentinel is at index count-2, new entry at count-1
    LogEntry sentinel = buf.entryAt(buf.count() - 2);
    LogEntry newest   = buf.entryAt(buf.count() - 1);

    check(sentinel.severity == LogEntry::WARN,
          "Overflow sentinel: severity is WARN");
    check(sentinel.text == "1 earlier entries were dropped (buffer full)",
          "Overflow sentinel: text matches exact format");
    check(newest.text == "overflow entry",
          "Overflow: newest entry is the pushed entry");
}

static void test_sentinel_counts_multiple_drops()
{
    // If N entries are dropped due to verbosity before overflow, sentinel says "N earlier entries..."
    // Here we test the sentinel N count: push kCapacity verbose entries, then push one
    // that is filtered (verbosity=1, INFO → dropped, m_dropped increments),
    // then push one that passes (WARN) → triggers overflow, sentinel must say "2 earlier entries..."
    // Wait — the plan says m_dropped tracks verbosity-filtered drops (not eviction drops).
    // Let me re-read: "1 earlier entries were dropped (buffer full)" — the N is m_dropped+1.
    // Actually from the plan: "build sentinel entry with text='<m_dropped+1> earlier entries...'"
    // and "reset m_dropped to 0" after. So m_dropped accumulates verbosity-dropped entries,
    // not ring-buffer-evicted entries. Let me test that.

    TestableLogBuffer buf;
    // Fill to capacity with WARN (Normal verbosity stores WARN)
    buf.setVerbosity(1); // Normal
    for (int i = 0; i < LogBuffer::kCapacity; ++i) {
        buf.log(LogEntry::WARN, "fill");
    }
    // Now push 2 INFO entries — they are filtered by verbosity → m_dropped += 2
    buf.log(LogEntry::INFO, "dropped1");
    buf.log(LogEntry::INFO, "dropped2");
    check(buf.count() == LogBuffer::kCapacity,
          "After verbosity drops, count still kCapacity");

    // Now push a WARN (passes verbosity=1) → buffer full → evict + sentinel
    buf.log(LogEntry::WARN, "trigger overflow");
    check(buf.count() == LogBuffer::kCapacity,
          "After overflow trigger, count stays kCapacity");

    LogEntry sentinel = buf.entryAt(buf.count() - 2);
    LogEntry newest   = buf.entryAt(buf.count() - 1);

    // m_dropped was 2 when overflow occurred, sentinel text = "3 earlier entries..."
    // (m_dropped+1 = 3)
    check(sentinel.severity == LogEntry::WARN,
          "Multi-drop sentinel: severity WARN");
    check(sentinel.text == "3 earlier entries were dropped (buffer full)",
          "Multi-drop sentinel: text says 3 (2 verbosity-dropped + 1 evicted)");
    check(newest.text == "trigger overflow",
          "Multi-drop overflow: newest is the trigger entry");
}

static void test_verbosity_getset()
{
    // setVerbosity(2) then getVerbosity() returns 2
    LogBuffer buf;
    buf.setVerbosity(2);
    check(buf.getVerbosity() == 2, "setVerbosity(2)/getVerbosity() returns 2");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    printf("=== LogBuffer Tests ===\n\n");

    test_verbosity_normal_passes_warn();
    test_verbosity_normal_drops_info();
    test_verbosity_normal_drops_clip();
    test_verbosity_none_drops_error();
    test_verbosity_verbose_stores_all();
    test_clip_name_stored();
    test_clip_name_empty_when_not_provided();
    test_buffer_full_at_capacity();
    test_ring_overflow_eviction_and_sentinel();
    test_sentinel_counts_multiple_drops();
    test_verbosity_getset();

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
