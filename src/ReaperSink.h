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

#ifndef REAPER_AAF_REAPERSINK_H
#define REAPER_AAF_REAPERSINK_H

#include "IRppSink.h"

class ProjectStateContext;

struct ReaperSink : IRppSink {
    explicit ReaperSink(ProjectStateContext *ctx) : m_ctx(ctx) {}
    void writeLine(const char *line) override;
private:
    ProjectStateContext *m_ctx;
};

#endif // REAPER_AAF_REAPERSINK_H
