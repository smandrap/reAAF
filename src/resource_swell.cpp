// Dialog resources for macOS/Linux via SWELL.
// resource.rc_mac_dlg is auto-generated from resource.rc by swell_resgen.php
// at build time — resource.rc is the single source of truth.
// Not compiled on Windows (see CMakeLists.txt).

// reaper_plugin.h pulls in swell.h which defines the Win32 constants
// (CBS_DROPDOWNLIST, WS_TABSTOP, etc.) needed by the generated file.
// swell-dlggen.h must come after swell.h and before the generated include.
#include "reaper_plugin.h"
#include "resource.h"
#include "swell-dlggen.h"

// WS_SIZEBOX in resource.rc is not auto-mapped to SWELL_DLG_WS_RESIZABLE by
// swell_resgen.php, so we override the generated style before including it.
#define SET_IDD_AAF_PROGRESS_STYLE (SWELL_DLG_FLAGS_AUTOGEN | SWELL_DLG_WS_RESIZABLE)

#include "resource.rc_mac_dlg"
