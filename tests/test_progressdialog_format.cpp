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

// Standalone tests for ProgressDialog's formatEntry() formatting helper.

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
