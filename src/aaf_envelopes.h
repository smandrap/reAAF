//
// Created by Federico Manuppella on 11/03/26.
//

#ifndef REAPER_AAF_AAF_VOLENV_H
#define REAPER_AAF_AAF_VOLENV_H

struct RppWriter;
struct aafiAudioGain;

void write_volume_envelope(const RppWriter &w,
                           const aafiAudioGain *gain,
                           double seg_start_sec,
                           double seg_len_sec,
                           const char *envTag);

void write_pan_envelope(const RppWriter &w,
                               const aafiAudioGain *pan,
                               const double seg_start_sec,
                               const double seg_len_sec);


#endif //REAPER_AAF_AAF_VOLENV_H
