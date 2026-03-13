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
struct aafiAudioEssencePointer;
class ProjectStateContext;

class AafImporter {
public:
    AafImporter(ProjectStateContext *ctx, const char *filepath);


    int run();


private:
    RppWriter m_writer;
    AAF_Iface *m_aafi;
    std::string m_filePath;
    std::string m_extractDir;
    bool m_extractDirCreated = false;

    static std::string buildExtractDir(const char *filepath);
    static const char *rppSourceTypeFromPath(const char *filePath);
    static double resolveConstantGain(const aafiAudioGain *gain, double defaultValue = 1.0);
    static const char *resolveClipName(const aafiAudioClip *clip);

    void setMediaLocation() const;

    bool loadFile();



    void processTrack_Audio(const aafiAudioTrack *track);

    void processTrack_Video(const aafiVideoTrack *track,
                            int &itemCounter);

    void processItem_Audio(aafiAudioClip *clip,
                           const aafiTimelineItem *ti,
                           const aafRational_t *trackEditRate,
                           const XFadeMap &xFadeMap, const aafiAudioEssencePointer *essPtr);

    void processItem_Video(const aafiVideoClip *clip, const aafRational_t *trackEditRate);

    void processSource_Audio(const aafiAudioEssencePointer *essPtr);

    void processSource_Video(const aafiVideoEssence *ess);

    void processMarkers() const;

    void processEnvelope(const aafiAudioGain *gain,
                       double segLenSec,
                       const char *tag,
                       const std::function<double(double)> &transform,
                       bool arm = false);
};

#endif // REAPER_AAF_AAFIMPORTER_H
