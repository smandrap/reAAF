//
// Created by Federico Manuppella on 11/03/26.
//

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
