#ifndef REAPER_AAF_RPPWRITER_H
#define REAPER_AAF_RPPWRITER_H

class ProjectStateContext;

struct RppWriter {
    ProjectStateContext *ctx;
    void line(const char *fmt, ...) const;
};

#endif //REAPER_AAF_RPPWRITER_H
