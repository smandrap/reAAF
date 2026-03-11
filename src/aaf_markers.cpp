#include "aaf_markers.h"
#include "helpers.h"
#include "RppWriter.h"
#include "libaaf/AAFIface.h"

// ---------------------------------------------------------------------------
// Markers
//
// aafiMarker (from AAFIface.h):
//   aafPosition_t  start
//   aafPosition_t  length     — > 0 means region
//   aafRational_t *edit_rate  — POINTER
//   char          *name, *comment
//   uint16_t       RGBColor[3]
// ---------------------------------------------------------------------------

void write_markers(const RppWriter &w, const AAF_Iface *aafi) {
    int id = 1;
    const aafiMarker *m = nullptr;
    AAFI_foreachMarker(aafi, m) {
        // edit_rate is a pointer
        const double t = pos_to_seconds(m->start, m->edit_rate);

        if (const bool isRegion = (m->length > 0); !isRegion) {
            w.line("MARKER %d %.10f \"%s\" 0 0 1",
                   id++, t,
                   m->name ? escape_rpp_string(m->name).c_str() : "");
        } else {
            const double end = pos_to_seconds(m->start + m->length, m->edit_rate);
            w.line("MARKER %d %.10f \"%s\" 0 1 1",
                   id, t, m->name ? escape_rpp_string(m->name).c_str() : "");
            w.line("MARKER %d %.10f \"%s\" 0 1 1",
                   id + 1, end, m->name ? escape_rpp_string(m->name).c_str() : "");
            id += 2;
        }
    }
}
