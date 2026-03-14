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

#include "FadeResolver.h"
#include <libaaf.h>
#include <sys/stat.h>

#include "helpers.h"

XFadeMap buildXFadeMap(const aafiAudioTrack *track) {
    XFadeMap m;
    aafiTimelineItem *ti = nullptr;
    AAFI_foreachTrackItem(track, ti) {
        const aafiTransition *xf = aafi_timelineItemToCrossFade(ti);
        if (!xf) continue;
        if (!(xf->flags & AAFI_TRANS_XFADE)) continue;

        // Use separate fields so a clip between two xfades keeps both.
        if (ti->prev) m[ti->prev].fadeOut = xf;
        if (ti->next) m[ti->next].fadeIn = xf;
    }
    return m;
}

static ResolvedFade resolveFromTransition(const aafiTransition *t, const aafRational_t *editRate) {
    return {pos_to_seconds(t->len, editRate), interpol_to_reaper_shape(t->flags)};
}

ResolvedFade resolveFadeIn(aafiAudioClip *clip, const aafiTimelineItem *ti, const XFadeMap &xFadeMap, const aafRational_t *editRate) {
    if (const aafiTransition *t = aafi_getFadeIn(clip))
        return resolveFromTransition(t, editRate);

    if (const auto it = xFadeMap.find(ti); it != xFadeMap.end() && it->second.fadeIn)
        return resolveFromTransition(it->second.fadeIn, editRate);

    return {};
}

ResolvedFade resolveFadeOut(aafiAudioClip *clip, const aafiTimelineItem *ti, const XFadeMap &xFadeMap,
    const aafRational_t *editRate) {
    if (const aafiTransition *t = aafi_getFadeOut(clip))
        return resolveFromTransition(t, editRate);
    if (const auto it = xFadeMap.find(ti); it != xFadeMap.end() && it->second.fadeOut)
        return resolveFromTransition(it->second.fadeOut, editRate);
    return {};
}
