// tests/test_progressdialog_format.cpp
// Unit tests: formatEntry() formatting.

#include "LogBuffer.h"

#include <catch2/catch_all.hpp>

// ---------------------------------------------------------------------------
// formatEntry — formats a LogEntry as a displayable string with severity prefix.
// ---------------------------------------------------------------------------

static std::string formatEntry(const LogEntry& e)
{
    const char* prefix = "";
    switch (e.severity) {
        case LogEntry::ERR:  prefix = "[ERROR]"; break;
        case LogEntry::WARN: prefix = "[WARN]";  break;
        case LogEntry::INFO: prefix = "[INFO]";  break;
    }
    return std::string(prefix) + " " + e.text;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("formatEntry prefixes") {
    SECTION("ERROR prefix") {
        LogEntry e; e.severity = LogEntry::ERR; e.text = "disk error";
        REQUIRE(formatEntry(e) == "[ERROR] disk error");
    }
    SECTION("WARN prefix") {
        LogEntry e; e.severity = LogEntry::WARN; e.text = "missing file";
        REQUIRE(formatEntry(e) == "[WARN] missing file");
    }
    SECTION("INFO prefix") {
        LogEntry e; e.severity = LogEntry::INFO; e.text = "Starting import...";
        REQUIRE(formatEntry(e) == "[INFO] Starting import...");
    }
}
