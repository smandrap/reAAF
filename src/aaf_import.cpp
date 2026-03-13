#include "AafImporter.h"
#include "aaf_import.h"


int ImportAAF(const char* filepath, ProjectStateContext* ctx) {
    if (!filepath || !ctx) return -1;
    return AafImporter(ctx, filepath).run();
}