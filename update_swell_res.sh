#!/usr/bin/env bash
set -e

# Paths
SWELL_RESGEN="extern/reaper-sdk/WDL/swell/swell_resgen.php"
RESOURCE_RC="src/resource.rc"
OUT_DIR="src/"

# Make sure output dir exists
#mkdir -p "$OUT_DIR"

# Generate the files
php "$SWELL_RESGEN" "$RESOURCE_RC"

# Move generated files to src/swell_generated
#mv resource.rc_mac_dlg "$OUT_DIR/"
#mv resource.rc_mac_menu "$OUT_DIR/"

echo "SWELL resources generated in $OUT_DIR"