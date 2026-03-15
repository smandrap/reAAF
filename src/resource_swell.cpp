// Dialog resources for macOS/Linux via SWELL.
// Not compiled on Windows (see CMakeLists.txt).

// reaper_plugin.h pulls in swell.h (and swell-types.h) which defines
// Win32 constants like CBS_DROPDOWNLIST, WS_TABSTOP, WS_VSCROLL, etc.
// swell-dlggen.h must be included after these types are available.
#include "reaper_plugin.h"
#include "swell/swell-dlggen.h"
#include "resource.h"

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_AAF_PREFS,
    SWELL_DLG_WS_CHILD | SWELL_DLG_WS_FLIPPED,
    "AAF Import Prefs", 300, 42, 1.8)
BEGIN
    GROUPBOX "AAF Import",     IDC_STATIC,          4,  4, 292, 32
    LTEXT    "Log verbosity:", IDC_STATIC,          13, 20,  68, 10
    COMBOBOX                   IDC_COMBO_VERBOSITY, 60, 19, 160, 50, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP
END
SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_AAF_PREFS)
