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

#include <string>
#include <vector>

#include "mutex.h"

// ---------------------------------------------------------------------------
// LogEntry — a single diagnostic message stored in the ring buffer.
// ---------------------------------------------------------------------------

struct LogEntry {
    enum Severity { ERROR, WARN, INFO, CLIP } severity;
    std::string text;
    std::string clipName; // empty if not clip-specific
};

// ---------------------------------------------------------------------------
// LogBuffer — fixed-capacity, mutex-protected ring buffer of LogEntry items.
//
// Thread safety: push(), setVerbosity() and getVerbosity() are all safe to
// call from any thread. The mutex is acquired for the full duration of push()
// so verbosity check and buffer write are atomic as a unit.
//
// Verbosity levels:
//   0 = None    — drops everything; push() stores nothing
//   1 = Normal  — stores ERROR and WARN only; drops INFO and CLIP
//   2 = Verbose — stores all severities
//
// Overflow behaviour: when the buffer is at capacity (kCapacity entries),
// push() evicts the oldest entry, inserts a sentinel [WARN] entry whose text
// is "N earlier entries were dropped (buffer full)" (N = m_dropped + 1 where
// m_dropped counts verbosity-filtered drops since the last sentinel), then
// stores the new entry. Count never exceeds kCapacity.
//
// Phase 2 will add: size_t drainNew(size_t readPos, std::vector<LogEntry>& out)
// ---------------------------------------------------------------------------

class LogBuffer {
public:
    static constexpr int kCapacity = 2000;

    // Push an entry into the buffer. Acquires the mutex for the full operation.
    // clipName may be nullptr (stored as empty string).
    void push(LogEntry::Severity sev, const char* msg, const char* clipName = nullptr);

    void setVerbosity(int v);
    int  getVerbosity() const;

    // --- Test-only accessors (used by TestableLogBuffer in tests/) ----------
    // Returns the number of entries currently stored in the buffer.
    int size() const;
    // Returns the entry at logical index idx (0 = oldest, size()-1 = newest).
    // Behaviour is undefined if idx is out of range.
    LogEntry at(int idx) const;

    // Incremental drain: copies entries from logical index readPos to end into out.
    // Returns the new read position (== size() after drain).
    // Clamps stale readPos when buffer overflow has evicted old entries.
    // Thread-safe (acquires mutex internally). Do NOT call at() from the callback.
    size_t drainNew(size_t readPos, std::vector<LogEntry>& out) const;

private:
    mutable WDL_Mutex m_mutex;
    LogEntry m_entries[kCapacity] = {};
    int m_head     = 0; // next write position (ring index)
    int m_count    = 0; // entries currently stored (max kCapacity)
    int m_verbosity = 1; // 0=None, 1=Normal, 2=Verbose
    int m_dropped  = 0; // verbosity-filtered drops since last sentinel

    // No-lock variant — caller MUST hold m_mutex. Used by drainNew() to avoid
    // deadlock (WDL_Mutex is non-reentrant; at() acquires the lock internally).
    LogEntry at_nolock(int idx) const;
};

#endif // REAPER_AAF_LOGBUFFER_H
