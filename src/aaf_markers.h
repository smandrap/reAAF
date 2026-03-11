#ifndef REAPER_AAF_AAF_MARKERS_H
#define REAPER_AAF_AAF_MARKERS_H

// Forward declarations
struct AAF_Iface;
struct RppWriter;

void write_markers(const RppWriter &w, const AAF_Iface *aafi);

#endif //REAPER_AAF_AAF_MARKERS_H