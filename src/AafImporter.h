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

    void setMediaLocation() const;

    bool loadFile();

    static const char *rppSourceTypeFromPath(const char *filePath);

    static double resolveConstantGain(const aafiAudioGain *gain, double defaultValue);

    void processMarkers() const;

    void processTrack_Audio(const aafiAudioTrack *track);

    static const char *resolveClipName(const aafiAudioClip *clip);

    void processTrack_Video(const aafiVideoTrack *track,
                            int trackIdx, int &itemCounter);

    void processItem_Audio(aafiAudioClip *clip,
                           const aafiTimelineItem *ti,
                           const aafRational_t *trackEditRate,
                           const XFadeMap &xFadeMap);

    void processItem_Video(const aafiVideoClip *clip, const aafRational_t *trackEditRate, int itemIdx);

    void processSource_Audio(const aafiAudioClip *clip);

    void processSource_Video(const aafiVideoEssence *ess);

    void processEnvelope(const aafiAudioGain *gain,
                       double segLenSec,
                       const char *tag,
                       const std::function<double(double)> &transform,
                       bool arm = false);
};

#endif // REAPER_AAF_AAFIMPORTER_H
