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
#include "ReaperSink.h"
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
struct aafiAudioEssenceFile;
class ProjectStateContext;

class AafImporter {
public:
    AafImporter(ProjectStateContext *ctx, const char *filepath, LogBuffer &logBuffer);

    int run();

private:
    ReaperSink m_reaperSink;
    RppWriter m_writer;
    AafiHandle m_aafi;
    std::string m_filePath;
    std::string m_extractDir;
    bool m_extractDirCreated = false;
    LogBuffer& m_logBuffer;

    static void libaafLogCallback(aafLog *log, void *userData, int lib, int type,
                                  const char *srcFile, const char *srcFunc, int line,
                                  const char *msg, void *user);

    void setMediaLocation() const;

    [[nodiscard]] bool loadFile() const;

    void processTrackAutomation(const aafiAudioTrack *track, double compLen);

    void processTrack_Audio(const aafiAudioTrack *track);

    void processTrack_Video(const aafiVideoTrack *track);

    void processItem_Audio(aafiAudioClip *clip,
                           const aafiTimelineItem *ti,
                           const aafRational_t *trackEditRate,
                           const XFadeMap &xFadeMap,
                           const aafiAudioEssencePointer *essPtr);

    void processItem_Video(const aafiVideoClip *clip, const aafRational_t *trackEditRate);

    // Returns true on success; sets ess->usable_file_path as a side effect via libaaf.
    bool extractEmbeddedEssence(aafiAudioEssenceFile *ess);

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
