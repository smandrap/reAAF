/*
* Copyright (C) 2026 Federico Manuppella
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef REAPER_AAF_AAFIMPORTER_H
#define REAPER_AAF_AAFIMPORTER_H

#include <functional>
#include <string>

#include "AafiHandle.h"
#include "LogBuffer.h"
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
    AafImporter(ProjectStateContext *ctx, const char *filepath, LogBuffer *logBuffer);

    int run();

private:
    RppWriter m_writer;
    AafiHandle m_aafi;
    std::string m_filePath;
    std::string m_extractDir;
    bool m_extractDirCreated = false;
    LogBuffer*  m_logBuffer = nullptr;
    std::string m_currentClipName;

    friend void libaafLogCallback(struct aafLog*, void*, int, int,
                                  const char*, const char*, int,
                                  const char*, void*);

    static std::string buildExtractDir(const char *filepath);

    static const char *rppSourceTypeFromPath(const char *filePath);

    static double resolveConstantGain(const aafiAudioGain *gain, double defaultValue = 1.0);

    static const char *resolveClipName(const aafiAudioClip *clip);

    void setMediaLocation() const;

    [[nodiscard]] bool loadFile() const;

    void processTrackAutomation(const aafiAudioTrack *track, double compLen);

    static int countRequiredTracks(const aafiAudioClip *clip, int &nchan);

    static const aafiAudioEssencePointer *getAudioEssencePtr(const aafiAudioClip *clip, int trackIdx);


    void processTrack_Audio(const aafiAudioTrack *track);

    void processTrack_Video(const aafiVideoTrack *track);

    void processItem_Audio(aafiAudioClip *clip,
                           const aafiTimelineItem *ti,
                           const aafRational_t *trackEditRate,
                           const XFadeMap &xFadeMap,
                           const aafiAudioEssencePointer *essPtr);

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
