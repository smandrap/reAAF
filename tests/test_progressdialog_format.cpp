// tests/test_progressdialog_format.cpp
// Standalone unit tests: formatEntry() formatting + drainNew() drain semantics.
// No REAPER SDK / SWELL required.

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "LogBuffer.h"

// ---------------------------------------------------------------------------
// formatEntry — local copy matching the implementation in ProgressDialog.cpp.
// Keep in sync when ProgressDialog.cpp is written (Plan 04).
// ---------------------------------------------------------------------------

static std::string formatEntry(const LogEntry& e)
{
    const char* prefix = "";
    switch (e.severity) {
        case LogEntry::ERROR: prefix = "[ERROR]"; break;
        case LogEntry::WARN:  prefix = "[WARN]";  break;
        case LogEntry::INFO:  prefix = "[INFO]";  break;
        case LogEntry::CLIP:  prefix = "[CLIP]";  break;
    }
    if (e.severity == LogEntry::CLIP && !e.clipName.empty())
        return std::string(prefix) + " " + e.clipName + ": " + e.text;
    return std::string(prefix) + " " + e.text;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void CHECK(bool cond, const char* msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        assert(false);
    }
}

// ---------------------------------------------------------------------------
// Test: formatEntry()
// ---------------------------------------------------------------------------

static void test_format_error()
{
    LogEntry e; e.severity = LogEntry::ERROR; e.text = "disk error"; e.clipName = "";
    CHECK(formatEntry(e) == "[ERROR] disk error", "ERROR prefix");
}

static void test_format_warn()
{
    LogEntry e; e.severity = LogEntry::WARN; e.text = "missing file"; e.clipName = "";
    CHECK(formatEntry(e) == "[WARN] missing file", "WARN prefix");
}

static void test_format_info()
{
    LogEntry e; e.severity = LogEntry::INFO; e.text = "Starting import..."; e.clipName = "";
    CHECK(formatEntry(e) == "[INFO] Starting import...", "INFO prefix");
}

static void test_format_clip_no_name()
{
    LogEntry e; e.severity = LogEntry::CLIP; e.text = "OK"; e.clipName = "";
    CHECK(formatEntry(e) == "[CLIP] OK", "CLIP no clipName");
}

static void test_format_clip_with_name()
{
    LogEntry e; e.severity = LogEntry::CLIP; e.text = "OK"; e.clipName = "Kick_01";
    CHECK(formatEntry(e) == "[CLIP] Kick_01: OK", "CLIP with clipName");
}

static void test_format_clip_warn_with_name()
{
    LogEntry e; e.severity = LogEntry::CLIP; e.text = "missing media file"; e.clipName = "Snare_02";
    CHECK(formatEntry(e) == "[CLIP] Snare_02: missing media file", "CLIP Snare_02");
}

// ---------------------------------------------------------------------------
// Test: drainNew()
// ---------------------------------------------------------------------------

static void test_drain_sequential()
{
    LogBuffer buf;
    buf.setVerbosity(2);
    buf.push(LogEntry::INFO,  "a", nullptr);
    buf.push(LogEntry::INFO,  "b", nullptr);
    buf.push(LogEntry::INFO,  "c", nullptr);
    buf.push(LogEntry::INFO,  "d", nullptr);
    buf.push(LogEntry::INFO,  "e", nullptr);

    std::vector<LogEntry> out;
    size_t pos = buf.drainNew(0, out);
    CHECK(pos == 5, "drainNew returns 5 after 5 pushes");
    CHECK(out.size() == 5, "drainNew returns 5 entries");

    std::vector<LogEntry> out2;
    size_t pos2 = buf.drainNew(pos, out2);
    CHECK(pos2 == 5, "drainNew returns same pos when no new entries");
    CHECK(out2.size() == 0, "drainNew returns 0 entries when nothing new");

    buf.push(LogEntry::WARN, "f", nullptr);
    buf.push(LogEntry::WARN, "g", nullptr);

    std::vector<LogEntry> out3;
    size_t pos3 = buf.drainNew(pos2, out3);
    CHECK(pos3 == 7, "drainNew returns 7 after 2 more pushes");
    CHECK(out3.size() == 2, "drainNew returns only 2 new entries");
}

static void test_drain_overflow_clamping()
{
    LogBuffer buf;
    buf.setVerbosity(2);
    // Fill to capacity
    for (int i = 0; i < LogBuffer::kCapacity; ++i)
        buf.push(LogEntry::INFO, "fill", nullptr);
    // Push one more to trigger overflow
    buf.push(LogEntry::WARN, "overflow", nullptr);

    // readPos=0 is stale (entries were evicted). drainNew must clamp it.
    std::vector<LogEntry> out;
    size_t pos = buf.drainNew(0, out);
    // Must not crash. out.size() must be <= kCapacity.
    CHECK(out.size() <= (size_t)LogBuffer::kCapacity, "overflow: out.size() <= kCapacity");
    // Returned pos must be >= kCapacity (it is the new total count)
    CHECK(pos >= (size_t)LogBuffer::kCapacity, "overflow: returned pos >= kCapacity");
    fprintf(stderr, "overflow clamping: pos=%zu out.size=%zu (ok)\n", pos, out.size());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_format_error();
    test_format_warn();
    test_format_info();
    test_format_clip_no_name();
    test_format_clip_with_name();
    test_format_clip_warn_with_name();
    test_drain_sequential();
    test_drain_overflow_clamping();
    fprintf(stderr, "All tests passed.\n");
    return 0;
}