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

#include "AafImporter.h"
#include "LogBuffer.h"
#include "LogDialog.h"
#include "PrefsPage.h"
#include "ReaperSink.h"
#include "defines.h"
#include "helpers.h"
#include <memory>
// ReSharper disable once CppUnusedIncludeDirective
#include "version.h"
#define REAPERAPI_IMPLEMENT
#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"

REAPER_PLUGIN_HINSTANCE g_hInst = nullptr;

namespace {
constexpr unsigned int kCmdId_ZoomProject = 40295;
// This stuff is needed to zoom to project at end of import
bool isAafImport = false; // prevent zooming ALL projects. Set in aaf_ImportProject

void OnProjectLoadTimer() {
    plugin_register("-timer", reinterpret_cast<void *>(OnProjectLoadTimer));
    // project is fully loaded here
    if ( isAafImport && PrefsPage::getZoomAfterImport() ) {
        Main_OnCommand(kCmdId_ZoomProject, 0);
        isAafImport = false;
    }
}

void OnBeginLoadProjectState(const bool isUndo, project_config_extension_t *) {
    if ( !isUndo && isAafImport )
        plugin_register("timer", reinterpret_cast<void *>(OnProjectLoadTimer));
}

project_config_extension_t s_projcfg = {nullptr, nullptr, OnBeginLoadProjectState, nullptr};

// project import stuff here

bool aaf_WantProjectFile(const char *fn) {
    const char *ext = strrchr(fn, '.');
    return ext && strcasecmp(ext, ".aaf") == 0;
}


const char *aaf_EnumFileExtensions(const int i, char **descptr) {
    if ( i == 0 ) {
        static char kAafFileDesc[] = "Advanced Authoring Format (*.aaf)";
        if ( descptr )
            *descptr = kAafFileDesc;
        return "aaf";
    }
    return nullptr;
}

int aaf_ImportProject(const char *fn, ProjectStateContext *ctx) {
    if ( !fn || !ctx )
        return -1;
    isAafImport = true;

    const bool isDebug = PrefsPage::getShowDebug();

    std::unique_ptr<LogBuffer> logBuffer;
    int ok;
    try {
        logBuffer = std::make_unique<LogBuffer>(isDebug ? LogEntry::DEBUG : LogEntry::INFO);
        ReaperSink sink(ctx);
        ok = AafImporter(&sink, fn, *logBuffer).run();
    } catch ( const std::bad_alloc & ) {
        rlog("reAAF ERROR: Out of memory importing AAF file!!\n");
        return -1;
    }

    const auto mode = PrefsPage::getVerbosity();
    if ( mode == PrefsPage::LogVerbosity::NONE )
        return ok;

    if ( mode == PrefsPage::LogVerbosity::ERR ) {
        if ( !logBuffer->hasErrorsOrWarnings() )
            return ok;
        LogDialog::open(std::move(logBuffer), LogEntry::WARN, isDebug);
    } else {
        LogDialog::open(std::move(logBuffer), LogEntry::INFO, isDebug);
    }
    return ok;
}

project_import_register_t g_import_reg = {aaf_WantProjectFile, aaf_EnumFileExtensions,
                                          aaf_ImportProject};
} // namespace

// ---------------------------------------------------------------------------
// Plugin entry point
// ---------------------------------------------------------------------------
extern "C" {
REAPER_PLUGIN_DLL_EXPORT
int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, const reaper_plugin_info_t *rec) {
    g_hInst = hInstance;
    if ( !rec )
        return 0;

    if ( rec->caller_version != REAPER_PLUGIN_VERSION )
        return 0;
    if ( REAPERAPI_LoadAPI(rec->GetFunc) != 0 )
        return 0;

    plugin_register("projectconfig", &s_projcfg);

    plugin_register("projectimport", &g_import_reg);

    // Register the AAF Import preferences page
    PrefsPage::registerPage();

    plugin_register("atexit", reinterpret_cast<void *>(+[] {
                        PrefsPage::unregisterPage();
                        plugin_register("-projectconfig", &s_projcfg);
                        plugin_register("-projectimport", &g_import_reg);
                    }));

    return 1;
}
}
