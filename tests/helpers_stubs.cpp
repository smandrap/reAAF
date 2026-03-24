/*
 * Stub implementations of helpers.cpp symbols for tests.
 * rlog() is a no-op (calls REAPER's ShowConsoleMsg in production).
 * ensure_dir() uses std::filesystem instead of platform APIs.
 */

#include <cstdarg>
#include <filesystem>
#include <string>

void rlog(const char * /*fmt*/, ...) {}

bool ensure_dir(const std::string &path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
}
