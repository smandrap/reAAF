#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include "reaper_plugin.h"
#include "aaf_import.h"

#ifdef _WIN32
#  define strcasecmp _stricmp
#endif

// ---------------------------------------------------------------------------
// projectimport callbacks
// ---------------------------------------------------------------------------

static bool aaf_WantProjectFile(const char* fn)
{
    const char* ext = strrchr(fn, '.');
    return ext && strcasecmp(ext, ".aaf") == 0;
}

static const char* aaf_EnumFileExtensions(const int i, char** descptr)
{
    if (i == 0) {
        if (descptr) *descptr = const_cast<char *>("Advanced Authoring Format (*.aaf)");
        return "aaf";
    }
    return nullptr;
}

static int aaf_ImportProject(const char* fn, ProjectStateContext* ctx)
{
    return ImportAAF(fn, ctx);
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
extern "C"
{
    REAPER_PLUGIN_DLL_EXPORT
    int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE /*hInstance*/,
                                 const reaper_plugin_info_t* rec)
    {
        if (!rec) return 0;

        if (rec->caller_version != REAPER_PLUGIN_VERSION) return 0;
        if (REAPERAPI_LoadAPI(rec->GetFunc) != 0) return 0;

        rec->Register("projectimport", &g_import_reg);

        return 1;
    }
}