#ifndef REAPER_AAF_AAFIMPORTER_H
#define REAPER_AAF_AAFIMPORTER_H

#include <functional>
#include <string>

#include "RppWriter.h"
#include "FadeResolver.h"

// Forward declarations
struct AAF_Iface;
struct aafiAudioTrack;
struct aafiAudioClip;
struct aafiAudioGain;
struct aafiVideoTrack;
struct aafiVideoClip;
struct aafiVideoEssence;
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

    bool m_extractDirCreated = false;

    bool ensureExtractDir();

    void writeMarkers() const;

    void writeAudioTrack(const aafiAudioTrack *track,
                         int trackIdx, int &itemCounter);

    void writeAudioItem(aafiAudioClip *clip,
                        const aafiTimelineItem *ti,
                        const aafRational_t *trackEditRate,
                        int itemIdx,
                        const XFadeMap &xFadeMap);

    void writeAudioSource(const aafiAudioClip *clip);

    void writeVideoTrack(const aafiVideoTrack *track,
                         int trackIdx, int &itemCounter);

    void writeVideoItem(const aafiVideoClip *clip, const aafRational_t *trackEditRate, int itemIdx);

    void writeVideoSource(const aafiVideoEssence *ess);

    void writeEnvelope(const aafiAudioGain *gain,
                       double segLenSec,
                       const char *tag,
                       const std::function<double(double)> &transform,
                       bool arm = false);
};

#endif // REAPER_AAF_AAFIMPORTER_H
