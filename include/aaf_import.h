#pragma once

/*
 * reaper_aaf — AAF import plugin for REAPER
 * aaf_import.h  — public interface for the import layer
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "reaper_plugin.h"

// ---------------------------------------------------------------------------
// Called from main.cpp's projectimport hook.
//
// filepath  — absolute path to the .aaf file
// ctx       — REAPER's ProjectStateContext; write RPP tokens into it.
//
// Returns 0 on success, non-zero on failure.
// ---------------------------------------------------------------------------
int ImportAAF(const char* filepath, ProjectStateContext* ctx);
