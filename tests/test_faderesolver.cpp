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

// Standalone tests for FadeResolver.h/.cpp.
// Constructs LibAAF C structs directly — no AAF_Iface needed.

#include "FadeResolver.h"

#include <catch2/catch_all.hpp>
#include <libaaf/AAFIface.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a minimal linked list: [clip_ti] with no neighbours.
static void link_solo(aafiTimelineItem &clip_ti, aafiAudioClip &clip) {
    clip_ti.type = AAFI_AUDIO_CLIP;
    clip_ti.data = &clip;
    clip.timelineItem = &clip_ti;
}

// Insert a transition item between two clip items and set all back/forward links.
static void link_trans(aafiTimelineItem &prev_ti, aafiTimelineItem &trans_ti, aafiTransition &trans,
                       aafiTimelineItem &next_ti) {
    trans_ti.type = AAFI_TRANS;
    trans_ti.data = &trans;

    prev_ti.next = &trans_ti;
    trans_ti.prev = &prev_ti;
    trans_ti.next = &next_ti;
    next_ti.prev = &trans_ti;
}

// ---------------------------------------------------------------------------
// resolveFadeIn
// ---------------------------------------------------------------------------

TEST_CASE(

    "resolveFadeIn: no fade returns zero length and default shape") {
    aafiAudioClip clip{};
    aafiTimelineItem ti{};
    link_solo(ti, clip);

    XFadeMap empty;
    aafRational_t rate{25, 1};

    auto [len, shape] = resolveFadeIn(&clip, &ti, empty, &rate);
    REQUIRE(len == Catch::Approx(0.0));
    REQUIRE(shape == 1);
}

TEST_CASE(

    "resolveFadeIn: clip fadein transition is used") {
    // prev item is a FADE_IN transition
    aafiTransition trans{};
    trans.len = 100; // 100 frames @ 25fps = 4 s
    trans.flags = AAFI_TRANS_FADE_IN | AAFI_INTERPOL_LINEAR;

    aafiTimelineItem trans_ti{};
    aafiTimelineItem clip_ti{};
    aafiAudioClip clip{};
    link_solo(clip_ti, clip);
    trans_ti.type = AAFI_TRANS;
    trans_ti.data = &trans;
    clip_ti.prev = &trans_ti;

    XFadeMap empty;
    aafRational_t rate{25, 1};

    auto [len, shape] = resolveFadeIn(&clip, &clip_ti, empty, &rate);
    REQUIRE(len == Catch::Approx(4.0));
    REQUIRE(shape == 0); // linear
}

TEST_CASE(

    "resolveFadeIn: xfade in map is used when no clip fadein") {
    aafiTransition xf{};
    xf.len = 50; // 50 frames @ 25fps = 2 s
    xf.flags = AAFI_TRANS_XFADE | AAFI_INTERPOL_POWER;

    aafiTimelineItem clip_ti{};
    aafiAudioClip clip{};
    link_solo(clip_ti, clip);

    XFadeMap xFadeMap;
    xFadeMap[&clip_ti].fadeIn = &xf;

    aafRational_t rate{25, 1};

    auto [len, shape] = resolveFadeIn(&clip, &clip_ti, xFadeMap, &rate);
    REQUIRE(len == Catch::Approx(2.0));
    REQUIRE(shape == 4); // power/fast-start
}

TEST_CASE(

    "resolveFadeIn: clip fadein takes priority over xfade in map") {
    aafiTransition fade_trans{};
    fade_trans.len = 100; // 4 s
    fade_trans.flags = AAFI_TRANS_FADE_IN | AAFI_INTERPOL_LINEAR;

    aafiTransition xf{};
    xf.len = 200; // 8 s — should NOT win
    xf.flags = AAFI_TRANS_XFADE | AAFI_INTERPOL_POWER;

    aafiTimelineItem trans_ti{};
    trans_ti.type = AAFI_TRANS;
    trans_ti.data = &fade_trans;

    aafiTimelineItem clip_ti{};
    aafiAudioClip clip{};
    link_solo(clip_ti, clip);
    clip_ti.prev = &trans_ti;

    XFadeMap xFadeMap;
    xFadeMap[&clip_ti].fadeIn = &xf;

    aafRational_t rate{25, 1};

    auto [len, shape] = resolveFadeIn(&clip, &clip_ti, xFadeMap, &rate);
    REQUIRE(len == Catch::Approx(4.0)); // from clip fadein, not xfade
    REQUIRE(shape == 0); // linear, not power
}

// ---------------------------------------------------------------------------
// resolveFadeOut
// ---------------------------------------------------------------------------

TEST_CASE(

    "resolveFadeOut: no fade returns zero length and default shape") {
    aafiAudioClip clip{};
    aafiTimelineItem ti{};
    link_solo(ti, clip);

    XFadeMap empty;
    aafRational_t rate{25, 1};

    auto [len, shape] = resolveFadeOut(&clip, &ti, empty, &rate);
    REQUIRE(len == Catch::Approx(0.0));
    REQUIRE(shape == 1);
}

TEST_CASE(

    "resolveFadeOut: clip fadeout transition is used") {
    aafiTransition trans{};
    trans.len = 48000; // 48000 samples @ 48kHz = 1 s
    trans.flags = AAFI_TRANS_FADE_OUT | AAFI_INTERPOL_LOG;

    aafiTimelineItem trans_ti{};
    aafiTimelineItem clip_ti{};
    aafiAudioClip clip{};
    link_solo(clip_ti, clip);
    trans_ti.type = AAFI_TRANS;
    trans_ti.data = &trans;
    clip_ti.next = &trans_ti;

    XFadeMap empty;
    aafRational_t rate{48000, 1};

    auto [len, shape] = resolveFadeOut(&clip, &clip_ti, empty, &rate);
    REQUIRE(len == Catch::Approx(1.0));
    REQUIRE(shape == 3); // log/slow-start
}

TEST_CASE(

    "resolveFadeOut: xfade in map is used when no clip fadeout") {
    aafiTransition xf{};
    xf.len = 25; // 25 frames @ 25fps = 1 s
    xf.flags = AAFI_TRANS_XFADE | AAFI_INTERPOL_BSPLINE;

    aafiTimelineItem clip_ti{};
    aafiAudioClip clip{};
    link_solo(clip_ti, clip);

    XFadeMap xFadeMap;
    xFadeMap[&clip_ti].fadeOut = &xf;

    aafRational_t rate{25, 1};

    auto [len, shape] = resolveFadeOut(&clip, &clip_ti, xFadeMap, &rate);
    REQUIRE(len == Catch::Approx(1.0));
    REQUIRE(shape == 5); // bspline/bezier
}

// ---------------------------------------------------------------------------
// buildXFadeMap
// ---------------------------------------------------------------------------

TEST_CASE(

    "buildXFadeMap: empty track returns empty map") {
    aafiAudioTrack track{};
    track.timelineItems = nullptr;

    auto map = buildXFadeMap(&track);
    REQUIRE(map.empty());
}

TEST_CASE(

    "buildXFadeMap: track with no transitions returns empty map") {
    aafiAudioClip clip{};
    aafiTimelineItem clip_ti{};
    link_solo(clip_ti, clip);
    clip_ti.next = nullptr;

    aafiAudioTrack track{};
    track.timelineItems = &clip_ti;

    auto map = buildXFadeMap(&track);
    REQUIRE(map.empty());
}

TEST_CASE(

    "buildXFadeMap: xfade between two clips maps both neighbours") {
    aafiTransition xf{};
    xf.flags = AAFI_TRANS_XFADE;
    xf.len = 50;

    aafiAudioClip clip1{}, clip2{};
    aafiTimelineItem clip1_ti{}, xf_ti{}, clip2_ti{};
    link_solo(clip1_ti, clip1);
    link_solo(clip2_ti, clip2);
    link_trans(clip1_ti, xf_ti, xf, clip2_ti);

    aafiAudioTrack track{};
    track.timelineItems = &clip1_ti;

    auto map = buildXFadeMap(&track);

    REQUIRE(map.count(&clip1_ti) == 1);
    REQUIRE(map.at(&clip1_ti).fadeOut == &xf);
    REQUIRE(map.at(&clip1_ti).fadeIn == nullptr);

    REQUIRE(map.count(&clip2_ti) == 1);
    REQUIRE(map.at(&clip2_ti).fadeIn == &xf);
    REQUIRE(map.at(&clip2_ti).fadeOut == nullptr);
}

TEST_CASE(

    "buildXFadeMap: non-xfade transition is ignored") {
    // A FADE_IN transition should not be treated as an xfade
    aafiTransition fade{};
    fade.flags = AAFI_TRANS_FADE_IN;
    fade.len = 50;

    aafiAudioClip clip1{}, clip2{};
    aafiTimelineItem clip1_ti{}, fade_ti{}, clip2_ti{};
    link_solo(clip1_ti, clip1);
    link_solo(clip2_ti, clip2);
    link_trans(clip1_ti, fade_ti, fade, clip2_ti);

    aafiAudioTrack track{};
    track.timelineItems = &clip1_ti;

    auto map = buildXFadeMap(&track);
    REQUIRE(map.empty());
}
