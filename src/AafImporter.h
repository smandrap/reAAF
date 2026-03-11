#ifndef REAPER_AAF_AAFIMPORTER_H
#define REAPER_AAF_AAFIMPORTER_H

#include <string>
#include "RppWriter.h"
#include "FadeResolver.h"
#include "libaaf/AAFTypes.h"

//Forward declarations
struct AAF_Iface;
struct aafiAudioTrack;
struct aafiAudioClip;
struct aafiAudioGain;
struct aafiTimelineItem;
class ProjectStateContext;

class AafImporter {
public:
    AafImporter(ProjectStateContext *ctx, const char *filepath);

    int run();

private:
    RppWriter m_writer;
    AAF_Iface *m_aafi;
    std::string m_extractDir;
    std::string m_filePath;

    void writeTrack(
        const aafiAudioTrack *track,
        int trackIdx,
        int &itemCounter) const;

    void writeItem(
        aafiAudioClip *clip,
        const aafiTimelineItem *ti,
        const aafRational_t *trackEditRate,
        int itemIdx,
        const XFadeMap &xFadeMap) const;

    void writeSource(const aafiAudioClip *clip) const;


    void writeVolEnvelope(const aafiAudioGain *gain,
                          double seg_start_sec,
                          double seg_len_sec,
                          const char *envTag) const;

    void writePanEnvelope(const aafiAudioGain *pan,
                          double seg_start_sec,
                          double seg_len_sec) const;

    void writeMarkers() const;
};


#endif //REAPER_AAF_AAFIMPORTER_H
