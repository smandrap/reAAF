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

#ifndef REAPER_AAF_LOGBUFFER_H
#define REAPER_AAF_LOGBUFFER_H

#include <cstdarg>
#include <string>
#include <vector>


// ---------------------------------------------------------------------------
// LogEntry — a single diagnostic message stored in the ring buffer.
// ---------------------------------------------------------------------------

struct LogEntry {
    enum Severity { ERR, WARN, INFO, DEBUG } severity = INFO;

    std::string text;

    LogEntry() = default;

    LogEntry(const Severity sev, const char *msg) : severity(sev), text(msg ? msg : "") {}
};

// ---------------------------------------------------------------------------
// LogBuffer -- fixed-capacity ring buffer of LogEntry items.
//
// Overflow behaviour: on the first overflow push() evicts the oldest entry
// and inserts a one-time sentinel [WARN] "1 earlier entry was dropped
// (buffer full)", then stores the new entry. All subsequent overflows silently
// evict the oldest entry and store the new one (pure ring-buffer behaviour).
// Count never exceeds kCapacity.
// ---------------------------------------------------------------------------

class LogBuffer {
  public:
    static constexpr size_t kCapacity = 2000;

    explicit LogBuffer(const LogEntry::Severity minSeverity = LogEntry::INFO) {
        m_entries.reserve(kCapacity);
        m_minSeverity = minSeverity;
    }

    void log(LogEntry::Severity sev, const char *msg);

    void logf(LogEntry::Severity sev, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 3, 4)))
#endif
        ;

    [[nodiscard]] bool hasErrorsOrWarnings() const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] const LogEntry &at(int idx) const;

  private:
    std::vector<LogEntry> m_entries;
    LogEntry::Severity m_minSeverity;
    size_t m_head = 0; // next write position (ring index)
    bool m_overflowing = false; // true after the first capacity overflow
    bool m_hasErrorsOrWarnings = false;

    void push(const LogEntry &entry);
};

#endif // REAPER_AAF_LOGBUFFER_H
