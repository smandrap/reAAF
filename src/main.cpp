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
#include "PrefsPage.h"
#include "LogDialog.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "version.h"

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include "reaper_plugin.h"


#ifdef _WIN32
#  define strcasecmp _stricmp
#endif

REAPER_PLUGIN_HINSTANCE g_hInst = nullptr;

// ---------------------------------------------------------------------------
// projectimport callbacks
// ---------------------------------------------------------------------------

static bool aaf_WantProjectFile(const char *fn) {
    const char *ext = strrchr(fn, '.');
    return ext && strcasecmp(ext, ".aaf") == 0;
}

static const char *aaf_EnumFileExtensions(const int i, char **descptr) {
    if (i == 0) {
        static char kAafFileDesc[] = "Advanced Authoring Format (*.aaf)";
        if (descptr) *descptr = kAafFileDesc;
        return "aaf";
    }
    return nullptr;
}

static int aaf_ImportProject(const char *fn, ProjectStateContext *ctx) {
    if (!fn || !ctx) return -1;
    LogBuffer logBuffer;
    const int ok = AafImporter(ctx, fn, &logBuffer).run();

    const auto mode = PrefsPage::getVerbosity();
    if (mode == PrefsPage::LogVerbosity::NONE) return ok;

    if (mode == PrefsPage::LogVerbosity::ERR) {
        if (!logBuffer.hasErrorsOrWarnings()) return ok;
        LogDialog::open(std::move(logBuffer), LogEntry::WARN);
    } else {
        LogDialog::open(std::move(logBuffer), LogEntry::INFO);
    }
    return ok;
}

// project_import_register_t is defined in reaper_plugin.h.
// Three function pointers in this exact order:
//   bool        (*WantProjectFile)(const char* fn);
//   const char* (*EnumFileExtensions)(int i, char** descptr);
//   int         (*LoadProject)(const char* fn, ProjectStateContext* ctx);
static project_import_register_t g_import_reg = {
    aaf_WantProjectFile,
    aaf_EnumFileExtensions,
    aaf_ImportProject
};

// ---------------------------------------------------------------------------
// Plugin entry point
// ---------------------------------------------------------------------------
extern "C" {
REAPER_PLUGIN_DLL_EXPORT
int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                             const reaper_plugin_info_t *rec) {
    g_hInst = hInstance;
    if (!rec) return 0;

    if (rec->caller_version != REAPER_PLUGIN_VERSION) return 0;
    if (REAPERAPI_LoadAPI(rec->GetFunc) != 0) return 0;

    plugin_register("projectimport", &g_import_reg);

    // Register the AAF Import preferences page
    PrefsPage::registerPage();

    plugin_register("atexit", reinterpret_cast<void *>(+[] {
        PrefsPage::unregisterPage();
    }));

    return 1;
}
}
