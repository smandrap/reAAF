#include "aaf_import.h"
#include "AafImporter.h"


int ImportAAF(const char* filepath, ProjectStateContext* ctx) {
    if (!filepath || !ctx) return -1;
    return AafImporter(ctx, filepath).run();
}