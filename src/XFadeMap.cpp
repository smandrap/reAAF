#include "XFadeMap.h"
#include <libaaf.h>

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

