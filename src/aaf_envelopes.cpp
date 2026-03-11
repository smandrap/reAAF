//
// Created by Federico Manuppella on 11/03/26.
//

#include "aaf_envelopes.h"

#include "helpers.h"
#include "RppWriter.h"
#include "libaaf/AAFIface.h"

// ---------------------------------------------------------------------------
// Automation envelopes
//
// aafiAudioGain (from AAFIface.h):
//   uint32_t       flags;    — AAFI_AUDIO_GAIN_CONSTANT or AAFI_AUDIO_GAIN_VARIABLE
//   unsigned int   pts_cnt;  — number of control points
//   aafRational_t *time;     — array[pts_cnt]: 0.0–1.0 fraction of segment duration
//   aafRational_t *value;    — array[pts_cnt]: amplitude multipliers
//
// gain->time[i] is a 0.0–1.0 fraction of the enclosing segment's duration.
// Absolute seconds = seg_start_sec + time[i] * seg_len_sec
// ---------------------------------------------------------------------------

void write_volume_envelope(const RppWriter &w,
                                  const aafiAudioGain *gain,
                                  const double seg_start_sec,
                                  const double seg_len_sec,
                                  const char *envTag) {
    if (!gain) return;
    if (!(gain->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (gain->pts_cnt == 0 || !gain->time || !gain->value) return;

    w.line("<%s", envTag);
    // w.line("ACT 0 -1");
    w.line("VIS 1 1 1");

    for (unsigned int i = 0; i < gain->pts_cnt; ++i) {
        const double frac = rational_to_double(gain->time[i]); // 0.0 .. 1.0
        const double t = seg_start_sec + frac * seg_len_sec;
        const double val = clamp_volume(rational_to_double(gain->value[i]));

        w.line("PT %.10f %.10f 0", t, val);
    }

    w.line(">");
}

void write_pan_envelope(const RppWriter &w,
                               const aafiAudioGain *pan,
                               const double seg_start_sec,
                               const double seg_len_sec) {
    if (!(pan->flags & AAFI_AUDIO_GAIN_VARIABLE)) return;
    if (pan->pts_cnt == 0 || !pan->time || !pan->value) return;

    // AAF pan: 0=left, 0.5=center, 1=right → REAPER: -1=left, 0=centre, +1=right
    w.line("<PANENV2");
    // w.line("ACT 1 -1"); //Uncomment to always active
    w.line("VIS 1 1 1");
    w.line("ARM 1");

    for (unsigned int i = 0; i < pan->pts_cnt; ++i) {
        const double frac = rational_to_double(pan->time[i]);
        const double t = seg_start_sec + frac * seg_len_sec;
        const double aafPan = rational_to_double(pan->value[i]);
        const double rPan = clamp_pan((aafPan - 0.5) * -2.0); // multiply negative otherwise panning is reversed
        w.line("PT %.10f %.10f 0", t, rPan);
    }

    w.line(">");
}