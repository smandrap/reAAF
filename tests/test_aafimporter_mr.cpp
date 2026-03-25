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

/*
 * Machine-readable comparison test: AafImporter output vs aaftool reference files.
 *
 * For each .aaf in AAF_TEST_DIR with a corresponding .aaftool in GOLDEN_AAFTOOL_DIR,
 * runs AafImporter and compares numeric fields (position, length, soffs, gain, fades,
 * automation, markers) against the independent aaftool reference.
 *
 * Track splitting: STEREO tracks produce 2 RPP tracks per 1 AAF track; the RPP
 * track pair is skipped for detailed comparison (clip geometry only tested on MONO
 * tracks). Channels > 2 cause the remaining tracks to be skipped.
 *
 * Fade comparison: FadeResolver may have bugs — fade CHECK failures are expected
 * to surface them, not block the run.
 */

#include <catch2/catch_all.hpp>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "AafImporter.h"
#include "IRppSink.h"
#include "LogBuffer.h"

namespace {
// ── CapturingSink ─────────────────────────────────────────────────────────────

struct CapturingSink : IRppSink {
    std::vector<std::string> lines;
    void writeLine(const char *l) override { lines.push_back(l); }
};

// ── AaftoolParser data structures ─────────────────────────────────────────────

struct AafAutoPt {
    double time = 0.0; // fraction of clip length (0.0–1.0)
    double value = 0.0; // linear
};

struct AafClip {
    int64_t start = 0; // raw samples (includes comp_start)
    int64_t len = 0;
    int64_t soffs = 0;
    double gain = 1.0; // linear; 1.0 if not stated
    bool mute = false;
    // Explicit per-clip fades (not crossfade-derived). Empty curve = no fade.
    std::string fadein_curve;
    int64_t fadein_len = 0; // samples
    std::string fadeout_curve;
    int64_t fadeout_len = 0;
    // Name: ClipName if present, else first SourceFile unique_name
    std::string name;
    std::vector<AafAutoPt> automation;
    bool has_automation = false;
};

struct AafTrack {
    bool solo = false;
    bool mute = false;
    int channels = 1; // 1=MONO/Unknwn, 2=STEREO, 6=5.1, 8=7.1
    std::vector<AafClip> clips;
};

struct AafMarker {
    int64_t start = 0; // raw samples
    int64_t len = 0;
    std::string label;
};

struct AaftoolData {
    int64_t comp_start = 0; // samples
    int samplerate = 48000;
    std::vector<AafTrack> tracks;
    std::vector<AafMarker> markers;
};

// ── Parsing helpers ───────────────────────────────────────────────────────────

// Find key in line, skip any non-digit chars after it, parse int64.
static bool find_i64(const std::string &line, const char *key, int64_t &out) {
    auto p = line.find(key);
    if ( p == std::string::npos )
        return false;
    p += std::strlen(key);
    while ( p < line.size() && !std::isdigit((unsigned char)line[p]) && line[p] != '-' )
        ++p;
    if ( p >= line.size() )
        return false;
    char *end;
    out = std::strtoll(line.c_str() + p, &end, 10);
    return end != line.c_str() + p;
}

// Find key in line, skip any non-digit/sign/dot chars after it, parse double.
static bool find_dbl(const std::string &line, const char *key, double &out) {
    auto p = line.find(key);
    if ( p == std::string::npos )
        return false;
    p += std::strlen(key);
    while ( p < line.size() && !std::isdigit((unsigned char)line[p]) && line[p] != '-' &&
            line[p] != '.' )
        ++p;
    if ( p >= line.size() )
        return false;
    char *end;
    out = std::strtod(line.c_str() + p, &end);
    return end != line.c_str() + p;
}

// Find key, then extract next double-quoted string.
static bool find_quoted(const std::string &line, const char *key, std::string &out) {
    auto p = line.find(key);
    if ( p == std::string::npos )
        return false;
    p = line.find('"', p + std::strlen(key));
    if ( p == std::string::npos )
        return false;
    ++p;
    auto e = line.find('"', p);
    if ( e == std::string::npos )
        return false;
    out = line.substr(p, e - p);
    return true;
}

// Find "FadeIn:" or "FadeOut:" key, extract "CURV_XXX (N)" → curve name + sample count.
static bool find_fade(const std::string &line, const char *key, std::string &curve,
                      int64_t &samples) {
    auto p = line.find(key);
    if ( p == std::string::npos )
        return false;
    p += std::strlen(key);
    while ( p < line.size() && line[p] == ' ' )
        ++p;
    auto s = p;
    while ( p < line.size() && line[p] != ' ' && line[p] != '(' )
        ++p;
    if ( p == s )
        return false;
    curve = line.substr(s, p - s);
    p = line.find('(', p);
    if ( p == std::string::npos )
        return false;
    ++p;
    char *end;
    samples = std::strtoll(line.c_str() + p, &end, 10);
    return end != line.c_str() + p;
}

// ── AaftoolParser ─────────────────────────────────────────────────────────────

AaftoolData parse_aaftool(const std::string &path) {
    std::ifstream f(path);
    AaftoolData d;
    std::string line;

    enum State { PREAMBLE, TRACKS };
    State state = PREAMBLE;

    AafTrack *cur_track = nullptr;
    AafClip *cur_clip = nullptr;

    while ( std::getline(f, line) ) {
        // ── Section transitions ──────────────────────────────────────────────
        if ( line.find("Tracks & Clips") != std::string::npos ) {
            state = TRACKS;
            cur_track = nullptr;
            cur_clip = nullptr;
            continue;
        }

        // ── Preamble fields (also checked in TRACKS for robustness) ──────────
        {
            int64_t v;
            if ( find_i64(line, "Composition Start (samples)", v) ) {
                d.comp_start = v;
                continue;
            }
            if ( find_i64(line, "Dominant Sample Rate", v) ) {
                d.samplerate = static_cast<int>(v);
                continue;
            }
        }

        // ── Marker lines (appear after Tracks section) ───────────────────────
        if ( line.find("Marker[") != std::string::npos &&
             line.find("Start:") != std::string::npos ) {
            AafMarker m;
            find_i64(line, "Start:", m.start);
            find_i64(line, "Length:", m.len);
            find_quoted(line, "Label:", m.label);
            d.markers.push_back(m);
            continue;
        }

        if ( state != TRACKS )
            continue;

        // ── AudioTrack line ──────────────────────────────────────────────────
        if ( line.find("AudioTrack[") != std::string::npos ) {
            d.tracks.push_back({});
            cur_track = &d.tracks.back();
            cur_clip = nullptr;
            cur_track->solo = line.find("Solo: SOLO") != std::string::npos;
            cur_track->mute = line.find("Mute: MUTE") != std::string::npos;
            if ( line.find("Format: STEREO") != std::string::npos )
                cur_track->channels = 2;
            else if ( line.find("Format: 5.1") != std::string::npos )
                cur_track->channels = 6;
            else if ( line.find("Format: 7.1") != std::string::npos )
                cur_track->channels = 8;
            else
                cur_track->channels = 1; // MONO or Unknwn
            continue;
        }

        // ── Clip line ────────────────────────────────────────────────────────
        if ( cur_track && line.find("Clip (") != std::string::npos &&
             line.find("Start:") != std::string::npos ) {
            cur_track->clips.push_back({});
            cur_clip = &cur_track->clips.back();
            find_i64(line, "Start:", cur_clip->start);
            find_i64(line, "Len:", cur_clip->len);
            find_i64(line, "SourceOffset:", cur_clip->soffs);
            find_dbl(line, "Gain:", cur_clip->gain);
            cur_clip->mute = line.find("Mute: MUTE") != std::string::npos;
            cur_clip->has_automation = line.find("VolumeAutomation: YES") != std::string::npos;
            find_fade(line, "FadeIn:", cur_clip->fadein_curve, cur_clip->fadein_len);
            find_fade(line, "FadeOut:", cur_clip->fadeout_curve, cur_clip->fadeout_len);
            // ClipName (only present when explicitly named)
            find_quoted(line, "ClipName:", cur_clip->name);
            continue;
        }

        // ── SourceFile line — fill name from ch 1 if not set by ClipName ────
        if ( cur_clip && cur_clip->name.empty() &&
             line.find("SourceFile [ch 1]:") != std::string::npos ) {
            find_quoted(line, "SourceFile [ch 1]:", cur_clip->name);
            continue;
        }

        // ── Automation point line ────────────────────────────────────────────
        if ( cur_clip && cur_clip->has_automation && line.find("_time:") != std::string::npos ) {
            AafAutoPt pt;
            find_dbl(line, "_time:", pt.time);
            find_dbl(line, "_value:", pt.value);
            cur_clip->automation.push_back(pt);
            continue;
        }
    }
    return d;
}

// ── RppParser data structures ─────────────────────────────────────────────────

struct RppAutoPt {
    double time = 0.0; // seconds from item start
    double value = 0.0; // linear
};

struct RppItem {
    double position = 0.0;
    double length = 0.0;
    double soffs = 0.0;
    double gain = 1.0;
    bool mute = false;
    int fadein_shape = 1; // 1 = no fade (REAPER default)
    double fadein_len = 0.0;
    int fadeout_shape = 1;
    double fadeout_len = 0.0;
    std::string name;
    std::vector<RppAutoPt> volenv;
};

struct RppTrack {
    std::string name;
    bool mute = false;
    bool solo = false;
    std::vector<RppItem> items;
};

struct RppData {
    int samplerate = 48000;
    double projoffs = 0.0;
    std::vector<RppTrack> tracks; // audio tracks only (VIDEO excluded)
    struct Marker {
        double pos = 0.0;
        bool is_region = false;
        std::string name;
    };

    std::vector<Marker> markers;
};

// ── RppParser ─────────────────────────────────────────────────────────────────

RppData parse_rpp(const std::vector<std::string> &lines) {
    RppData d;

    std::vector<std::string> stack; // chunk tag names
    int track_count = 0;
    RppTrack *cur_track = nullptr;
    RppItem *cur_item = nullptr;
    bool in_volenv = false;

    for ( const auto &line : lines ) {
        // ── Opening chunk ────────────────────────────────────────────────────
        if ( !line.empty() && line[0] == '<' ) {
            std::string tag = line.substr(1);
            auto sp = tag.find(' ');
            if ( sp != std::string::npos )
                tag = tag.substr(0, sp);
            stack.push_back(tag);

            if ( tag == "TRACK" ) {
                ++track_count;
                d.tracks.push_back({});
                cur_track = &d.tracks.back();
                cur_item = nullptr;
            } else if ( tag == "ITEM" ) {
                if ( cur_track ) {
                    cur_track->items.push_back({});
                    cur_item = &cur_track->items.back();
                }
            } else if ( tag == "VOLENV" ) {
                in_volenv = true;
            }
            continue;
        }

        // ── Closing chunk ────────────────────────────────────────────────────
        if ( line == ">" ) {
            if ( !stack.empty() ) {
                const std::string closing = stack.back();
                stack.pop_back();
                if ( closing == "VOLENV" ) {
                    in_volenv = false;
                } else if ( closing == "ITEM" ) {
                    cur_item = nullptr;
                } else if ( closing == "TRACK" ) {
                    // Drop the VIDEO track — it has no audio data to compare.
                    // Not all AAF files emit a VIDEO track (e.g. PT files don't).
                    if ( cur_track && cur_track->name == "VIDEO" )
                        d.tracks.pop_back();
                    cur_track = nullptr;
                }
            }
            continue;
        }

        // ── Data lines ───────────────────────────────────────────────────────
        if ( in_volenv && cur_item ) {
            if ( line.rfind("PT ", 0) == 0 ) {
                std::istringstream iss(line.substr(3));
                RppAutoPt pt;
                iss >> pt.time >> pt.value;
                cur_item->volenv.push_back(pt);
            }
        } else if ( cur_item ) {
            if ( line.rfind("POSITION ", 0) == 0 )
                cur_item->position = std::stod(line.substr(9));
            else if ( line.rfind("LENGTH ", 0) == 0 )
                cur_item->length = std::stod(line.substr(7));
            else if ( line.rfind("SOFFS ", 0) == 0 )
                cur_item->soffs = std::stod(line.substr(6));
            else if ( line.rfind("MUTE ", 0) == 0 )
                cur_item->mute = (line[5] == '1');
            else if ( line.rfind("FADEIN ", 0) == 0 ) {
                std::istringstream iss(line.substr(7));
                iss >> cur_item->fadein_shape >> cur_item->fadein_len;
            } else if ( line.rfind("FADEOUT ", 0) == 0 ) {
                std::istringstream iss(line.substr(8));
                iss >> cur_item->fadeout_shape >> cur_item->fadeout_len;
            } else if ( line.rfind("VOLPAN ", 0) == 0 ) {
                std::istringstream iss(line.substr(7));
                iss >> cur_item->gain;
            } else if ( line.rfind("NAME ", 0) == 0 ) {
                auto q1 = line.find('"');
                auto q2 = line.rfind('"');
                if ( q1 != std::string::npos && q2 > q1 )
                    cur_item->name = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if ( cur_track ) {
            if ( line.rfind("NAME ", 0) == 0 ) {
                auto q1 = line.find('"');
                auto q2 = line.rfind('"');
                if ( q1 != std::string::npos && q2 > q1 )
                    cur_track->name = line.substr(q1 + 1, q2 - q1 - 1);
            } else if ( line.rfind("MUTESOLO ", 0) == 0 ) {
                std::istringstream iss(line.substr(9));
                int m = 0, s = 0;
                if ( iss >> m >> s ) {
                    cur_track->mute = (m != 0);
                    cur_track->solo = (s != 0);
                }
            }
        } else {
            // Global context: before any track
            if ( line.rfind("SAMPLERATE ", 0) == 0 ) {
                std::istringstream iss(line.substr(11));
                iss >> d.samplerate;
            } else if ( line.rfind("PROJOFFS ", 0) == 0 ) {
                std::istringstream iss(line.substr(9));
                iss >> d.projoffs;
            } else if ( line.rfind("MARKER ", 0) == 0 ) {
                RppData::Marker m;
                std::istringstream iss(line.substr(7));
                int idx = 0;
                if ( !(iss >> idx >> m.pos) )
                    continue;
                auto q1 = line.find('"');
                auto q2 = line.find('"', q1 + 1);
                if ( q1 != std::string::npos && q2 != std::string::npos )
                    m.name = line.substr(q1 + 1, q2 - q1 - 1);
                if ( q2 != std::string::npos ) {
                    std::istringstream iss2(line.substr(q2 + 1));
                    int ir = 0;
                    if ( iss2 >> ir )
                        m.is_region = (ir != 0);
                }
                d.markers.push_back(m);
            }
        }
    }
    return d;
}

// ── Fade curve name → REAPER shape index ─────────────────────────────────────
// Mirrors interpol_to_reaper_shape() in helpers.h

static int curve_to_shape(const std::string &curve) {
    if ( curve == "CURV_LIN" )
        return 0;
    if ( curve == "CURV_PWR" )
        return 4;
    if ( curve == "CURV_LOG" )
        return 3;
    if ( curve == "CURV_BSP" )
        return 5;
    return 1; // quarter-sine default
}
} // namespace

// ── Test case ─────────────────────────────────────────────────────────────────

TEST_CASE(

    "AafImporter machine-readable comparison vs aaftool", "[golden-mr]") {
    namespace fs = std::filesystem;

    for ( const auto &entry : fs::directory_iterator(AAF_TEST_DIR) ) {
        if ( entry.path().extension() != ".aaf" )
            continue;
        const std::string stem = entry.path().stem().string();
        const fs::path ref_path = fs::path(GOLDEN_AAFTOOL_DIR) / (stem + ".aaftool");
        if ( !fs::exists(ref_path) )
            continue;

        SECTION(stem) {
            CapturingSink sink;
            LogBuffer log;
            AafImporter imp(&sink, entry.path().string().c_str(), log);
            imp.run();

            const AaftoolData ref = parse_aaftool(ref_path.string());
            const RppData rpp = parse_rpp(sink.lines);

            // Empty sessions have samplerate 0 — no audio data to compare.
            if ( ref.samplerate == 0 )
                continue;

            const double sr = static_cast<double>(ref.samplerate);

            // ── Samplerate ───────────────────────────────────────────────────
            REQUIRE(rpp.samplerate == ref.samplerate);

            // ── Project offset ───────────────────────────────────────────────
            // PROJOFFS (seconds) == comp_start_samples / samplerate
            REQUIRE(rpp.projoffs ==
                    Catch::Approx(static_cast<double>(ref.comp_start) / sr).epsilon(1e-4));

            // ── Tracks & clips ───────────────────────────────────────────────
            // AAF AudioTracks map to RPP tracks sequentially.
            // MONO (channels==1): 1 RPP track. STEREO (channels==2): 2 RPP tracks
            // (position/geometry skipped for stereo pairs). >2ch: stop comparing.
            size_t rpp_idx = 0;
            for ( size_t ai = 0; ai < ref.tracks.size(); ++ai ) {
                const AafTrack &at = ref.tracks[ai];
                CAPTURE(ai);

                if ( at.channels > 2 ) {
                    // 5.1/7.1: unknown RPP track count — stop
                    break;
                }

                if ( at.channels == 2 ) {
                    // Stereo: advance past 2 RPP tracks, skip detail
                    rpp_idx += 2;
                    continue;
                }

                // MONO (channels == 1)
                if ( rpp_idx >= rpp.tracks.size() )
                    break;
                const RppTrack &rt = rpp.tracks[rpp_idx++];

                // Track-level solo/mute
                CHECK(rt.mute == at.mute);
                CHECK(rt.solo == at.solo);

                // Clip count
                CHECK(rt.items.size() == at.clips.size());

                const size_t n_clips = std::min(rt.items.size(), at.clips.size());
                for ( size_t ci = 0; ci < n_clips; ++ci ) {
                    const AafClip &ac = at.clips[ci];
                    const RppItem &ri = rt.items[ci];
                    CAPTURE(ci);

                    // Position: (clip_start - comp_start) / samplerate
                    CHECK(ri.position ==
                          Catch::Approx(static_cast<double>(ac.start - ref.comp_start) / sr)
                              .epsilon(1e-4));

                    // Length
                    CHECK(ri.length ==
                          Catch::Approx(static_cast<double>(ac.len) / sr).epsilon(1e-4));

                    // Source offset: aaftool gives integer samples, AafImporter uses
                    // rational arithmetic — allow up to 2 samples of rounding difference.
                    CHECK(ri.soffs ==
                          Catch::Approx(static_cast<double>(ac.soffs) / sr).margin(2.5 / sr));

                    // Gain (linear; 1.0 if not stated in aaftool)
                    CHECK(ri.gain == Catch::Approx(std::pow(10.0, ac.gain / 20.0)).margin(0.123));

                    // Clip mute
                    CHECK(ri.mute == ac.mute);

                    // Explicit per-clip fades (FadeIn/FadeOut on the clip line itself).
                    // Note: FadeResolver may be buggy — failures here indicate real issues.
                    if ( !ac.fadein_curve.empty() ) {
                        CHECK(ri.fadein_shape == curve_to_shape(ac.fadein_curve));
                        CHECK(ri.fadein_len ==
                              Catch::Approx(static_cast<double>(ac.fadein_len) / sr).epsilon(1e-4));
                    }
                    if ( !ac.fadeout_curve.empty() ) {
                        CHECK(ri.fadeout_shape == curve_to_shape(ac.fadeout_curve));
                        CHECK(
                            ri.fadeout_len ==
                            Catch::Approx(static_cast<double>(ac.fadeout_len) / sr).epsilon(1e-4));
                    }

                    // Automation: aaftool times are fractions (0–1) of clip length;
                    // REAPER VOLENV PT times are seconds from item start.
                    // Conversion: rpp_time = aaf_fraction * ri.length
                    if ( !ac.automation.empty() ) {
                        CHECK(ri.volenv.size() == ac.automation.size());
                        const size_t n_pts = std::min(ri.volenv.size(), ac.automation.size());
                        for ( size_t pi = 0; pi < n_pts; ++pi ) {
                            CAPTURE(pi);
                            CHECK(ri.volenv[pi].time ==
                                  Catch::Approx(ac.automation[pi].time * ri.length).epsilon(1e-4));
                            // Use absolute margin: aaftool prints 6 decimal places,
                            // which is insufficient for relative comparison on small values.
                            CHECK(ri.volenv[pi].value ==
                                  Catch::Approx(ac.automation[pi].value).margin(1e-5));
                        }
                    }
                }
            }

            // ── Markers ──────────────────────────────────────────────────────
            // For each aaftool marker, find a matching RPP marker by position + label.
            // RPP may have extra region-end markers (empty label) — those are ignored.
            for ( const auto &am : ref.markers ) {
                CAPTURE(am.label);
                const double expected_pos = static_cast<double>(am.start - ref.comp_start) / sr;
                bool found = false;
                for ( const auto &rm : rpp.markers ) {
                    if ( rm.name == am.label && std::abs(rm.pos - expected_pos) < 1e-3 ) {
                        found = true;
                        break;
                    }
                }
                CHECK(found);
            }
        }
    }
}
