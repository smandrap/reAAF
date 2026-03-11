#ifndef REAPER_AAF_AAFIMPORTER_H
#define REAPER_AAF_AAFIMPORTER_H

#include <functional>
#include <string>

#include "RppWriter.h"
#include "FadeResolver.h"
#include "libaaf/AAFTypes.h"

// Forward declarations
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

    void writeMarkers() const;

    void writeTrack(const aafiAudioTrack *track,
                    int trackIdx, int &itemCounter);

    void writeItem(aafiAudioClip *clip,
                   const aafiTimelineItem *ti,
                   const aafRational_t *trackEditRate,
                   int itemIdx,
                   const XFadeMap &xFadeMap);

    void writeSource(const aafiAudioClip *clip);

    // Unified envelope emitter.
    // `tag`       : RPP tag (e.g. "VOLENV2", "PANENV2")
    // `transform` : maps raw AAF value → RPP value
    // `arm`       : emit "ARM 1" inside the envelope block (needed for pan)
    void writeEnvelope(const aafiAudioGain *gain,
                       double segStartSec,
                       double segLenSec,
                       const char *tag,
                       const std::function<double(double)> &transform,
                       bool arm = false);
};

#endif // REAPER_AAF_AAFIMPORTER_H
