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

// Standalone tests for helpers.h inline/constexpr functions.
// Does NOT link helpers.cpp — all tested functions are inline or constexpr.

#include "helpers.h"

#include <catch2/catch_all.hpp>

// ---------------------------------------------------------------------------
// rational_to_double
// ---------------------------------------------------------------------------

TEST_CASE("rational_to_double") {
    SECTION("zero denominator returns 0") {
        REQUIRE(rational_to_double({1, 0}) == 0.0);
    }
    SECTION("integer fraction") {
        REQUIRE(rational_to_double({25, 1}) == Catch::Approx(25.0));
    }
    SECTION("proper fraction") {
        REQUIRE(rational_to_double({1, 4}) == Catch::Approx(0.25));
    }
    SECTION("negative numerator") {
        REQUIRE(rational_to_double({-3, 4}) == Catch::Approx(-0.75));
    }
    SECTION("zero numerator") {
        REQUIRE(rational_to_double({0, 48000}) == 0.0);
    }
}

// ---------------------------------------------------------------------------
// pos_to_seconds
// ---------------------------------------------------------------------------

TEST_CASE("pos_to_seconds") {
    SECTION("null editRate returns 0") {
        REQUIRE(pos_to_seconds(100, nullptr) == 0.0);
    }
    SECTION("zero editRate returns 0") {
        aafRational_t zero{0, 1};
        REQUIRE(pos_to_seconds(100, &zero) == 0.0);
    }
    SECTION("25fps: 100 frames = 4 seconds") {
        aafRational_t fps25{25, 1};
        REQUIRE(pos_to_seconds(100, &fps25) == Catch::Approx(4.0));
    }
    SECTION("48000Hz sample rate: 48000 samples = 1 second") {
        aafRational_t sr{48000, 1};
        REQUIRE(pos_to_seconds(48000, &sr) == Catch::Approx(1.0));
    }
    SECTION("fractional edit rate (29.97)") {
        aafRational_t fps2997{30000, 1001};
        REQUIRE(pos_to_seconds(30000, &fps2997) == Catch::Approx(1001.0));
    }
    SECTION("zero position") {
        aafRational_t fps25{25, 1};
        REQUIRE(pos_to_seconds(0, &fps25) == 0.0);
    }
}

// ---------------------------------------------------------------------------
// escape_rpp_string
// ---------------------------------------------------------------------------

TEST_CASE("escape_rpp_string") {
    SECTION("null input returns empty string") {
        REQUIRE(escape_rpp_string(nullptr).empty());
    }
    SECTION("clean string passes through") {
        REQUIRE(escape_rpp_string("hello") == "hello");
    }
    SECTION("backslash is doubled") {
        REQUIRE(escape_rpp_string("a\\b") == "a\\\\b");
    }
    SECTION("double-quote is escaped") {
        REQUIRE(escape_rpp_string("say \"hi\"") == "say \\\"hi\\\"");
    }
    SECTION("newline is escaped") {
        REQUIRE(escape_rpp_string("line1\nline2") == "line1\\nline2");
    }
    SECTION("carriage return is escaped") {
        REQUIRE(escape_rpp_string("a\rb") == "a\\rb");
    }
    SECTION("control character below 0x20 becomes space") {
        REQUIRE(escape_rpp_string("\x01\x1F") == "  ");
    }
    SECTION("path with backslashes (Windows-style)") {
        REQUIRE(escape_rpp_string("C:\\audio\\kick.wav") == "C:\\\\audio\\\\kick.wav");
    }
}

// ---------------------------------------------------------------------------
// clamp_volume
// ---------------------------------------------------------------------------

TEST_CASE("clamp_volume") {
    SECTION("below 0 clamped to 0") {
        REQUIRE(clamp_volume(-1.0) == 0.0);
    }
    SECTION("above 4 clamped to 4") {
        REQUIRE(clamp_volume(5.0) == 4.0);
    }
    SECTION("value in range passes through") {
        REQUIRE(clamp_volume(1.5) == Catch::Approx(1.5));
    }
    SECTION("boundary 0 is inclusive") {
        REQUIRE(clamp_volume(0.0) == 0.0);
    }
    SECTION("boundary 4 is inclusive") {
        REQUIRE(clamp_volume(4.0) == 4.0);
    }
}

// ---------------------------------------------------------------------------
// clamp_pan
// ---------------------------------------------------------------------------

TEST_CASE("clamp_pan") {
    SECTION("below -1 clamped to -1") {
        REQUIRE(clamp_pan(-2.0) == -1.0);
    }
    SECTION("above 1 clamped to 1") {
        REQUIRE(clamp_pan(2.0) == 1.0);
    }
    SECTION("value in range passes through") {
        REQUIRE(clamp_pan(0.5) == Catch::Approx(0.5));
    }
    SECTION("center (0) passes through") {
        REQUIRE(clamp_pan(0.0) == 0.0);
    }
}

// ---------------------------------------------------------------------------
// aafiColorToReaper
// ---------------------------------------------------------------------------

TEST_CASE("aafiColorToReaper") {
    SECTION("black (0,0,0) produces only the marker flag bit") {
        const uint16_t black[3] = {0, 0, 0};
        REQUIRE(aafiColorToReaper(black) == 0x1000000);
    }
    SECTION("pure red (255,0,0)") {
        const uint16_t red[3] = {255, 0, 0};
        REQUIRE(aafiColorToReaper(red) == (0x1000000 | 0xFF0000));
    }
    SECTION("pure green (0,255,0)") {
        const uint16_t green[3] = {0, 255, 0};
        REQUIRE(aafiColorToReaper(green) == (0x1000000 | 0x00FF00));
    }
    SECTION("pure blue (0,0,255)") {
        const uint16_t blue[3] = {0, 0, 255};
        REQUIRE(aafiColorToReaper(blue) == (0x1000000 | 0x0000FF));
    }
    SECTION("white (255,255,255)") {
        const uint16_t white[3] = {255, 255, 255};
        REQUIRE(aafiColorToReaper(white) == 0x1FFFFFF);
    }
}

// ---------------------------------------------------------------------------
// interpol_to_reaper_shape
// ---------------------------------------------------------------------------

TEST_CASE("interpol_to_reaper_shape") {
    SECTION("linear flag -> shape 0") {
        REQUIRE(interpol_to_reaper_shape(AAFI_INTERPOL_LINEAR) == 0);
    }
    SECTION("power flag -> shape 4 (fast start)") {
        REQUIRE(interpol_to_reaper_shape(AAFI_INTERPOL_POWER) == 4);
    }
    SECTION("log flag -> shape 3 (slow start)") {
        REQUIRE(interpol_to_reaper_shape(AAFI_INTERPOL_LOG) == 3);
    }
    SECTION("bspline flag -> shape 5 (bezier)") {
        REQUIRE(interpol_to_reaper_shape(AAFI_INTERPOL_BSPLINE) == 5);
    }
    SECTION("unknown/zero flags -> shape 1 (quarter-sine default)") {
        REQUIRE(interpol_to_reaper_shape(0) == 1);
    }
    SECTION("linear takes priority when multiple flags set") {
        REQUIRE(interpol_to_reaper_shape(AAFI_INTERPOL_LINEAR | AAFI_INTERPOL_POWER) == 0);
    }
}
