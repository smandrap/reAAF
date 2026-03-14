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

#ifndef REAPER_AAF_XFADEMAP_H
#define REAPER_AAF_XFADEMAP_H

#include <unordered_map>

#include "libaaf/AAFTypes.h"

// Forward declaration

struct aafiAudioClip;
struct aafiTransition;
struct aafiTimelineItem;
struct aafiAudioTrack;

struct ClipXfades {
    const aafiTransition *fadeIn = nullptr; // xfade where this clip is the incoming side
    const aafiTransition *fadeOut = nullptr; // xfade where this clip is the outgoing side
};

using XFadeMap = std::unordered_map<const aafiTimelineItem *, ClipXfades>;

XFadeMap buildXFadeMap(const aafiAudioTrack *track);

struct ResolvedFade {
    double len = 0.0;
    int shape = 1; // default: sine
};

ResolvedFade resolveFadeIn(aafiAudioClip *clip, const aafiTimelineItem *ti, const XFadeMap &xFadeMap,
                           const aafRational_t *editRate);

ResolvedFade resolveFadeOut(aafiAudioClip *clip, const aafiTimelineItem *ti, const XFadeMap &xFadeMap,
                            const aafRational_t *editRate);

#endif //REAPER_AAF_XFADEMAP_H
