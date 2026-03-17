// tests/test_progressdialog_format.cpp
// Standalone unit tests: formatEntry() formatting.

#include <cassert>
#include <cstdio>
#include <string>
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
    }
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
    LogEntry e; e.severity = LogEntry::ERROR; e.text = "disk error";
    CHECK(formatEntry(e) == "[ERROR] disk error", "ERROR prefix");
}

static void test_format_warn()
{
    LogEntry e; e.severity = LogEntry::WARN; e.text = "missing file";
    CHECK(formatEntry(e) == "[WARN] missing file", "WARN prefix");
}

static void test_format_info()
{
    LogEntry e; e.severity = LogEntry::INFO; e.text = "Starting import...";
    CHECK(formatEntry(e) == "[INFO] Starting import...", "INFO prefix");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_format_error();
    test_format_warn();
    test_format_info();
    fprintf(stderr, "All tests passed.\n");
    return 0;
}