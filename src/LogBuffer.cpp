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

#include "LogBuffer.h"

#include <cstdio>   // snprintf, vsnprintf
#include <cstdarg>  // va_list

// ---------------------------------------------------------------------------
// push() — the core operation;
//
// Verbosity thresholds (enforced at write time, before any storage):
//   0 = None:    drop all (increment m_dropped, return)
//   1 = Normal:  keep ERROR and WARN only; drop INFO
//   2 = Verbose: keep everything
//
// Overflow path (m_count == kCapacity):
//   First overflow (m_overflowing == false):
//     1. Evict the oldest entry; write a sentinel WARN at that slot:
//        "<m_dropped+1> earlier entries were dropped (buffer full)".
//     2. Advance m_head; reset m_dropped = 0; set m_overflowing = true.
//     3. Fall through to the pure-ring write below.
//   Subsequent overflows (m_overflowing == true):
//     Just evict the oldest entry and store the new one. No sentinel spam.
//   m_count never exceeds kCapacity.
// ---------------------------------------------------------------------------

bool LogBuffer::shouldLogEntry(const LogEntry &entry) const {
    switch (m_verbosity) {
        case 0: return false;
        case 1: return entry.severity == LogEntry::ERROR || entry.severity == LogEntry::WARN;
        default: return true;
    }
}

void LogBuffer::log(const LogEntry::Severity sev, const char *msg) {
    push(LogEntry(sev, msg));
}

void LogBuffer::logf(const LogEntry::Severity sev, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    push(LogEntry(sev, buf));
}

void LogBuffer::push(const LogEntry &entry) {
    if (!shouldLogEntry(entry)) {
        ++m_dropped;
        return;
    }

    if (m_count == kCapacity) {
        if (!m_overflowing) {
            char buf[80];
            snprintf(buf, sizeof(buf), "%d earlier entries were dropped (buffer full)", m_dropped + 1);
            m_entries[m_head] = LogEntry(LogEntry::WARN, buf);
            m_head = (m_head + 1) % kCapacity;
            m_dropped = 0;
            m_overflowing = true;
        }
        // Pure ring: evict oldest, store new entry. m_count stays at kCapacity.
        m_entries[m_head] = entry;
        m_head = (m_head + 1) % kCapacity;
        return;
    }

    m_entries[m_head] = entry;
    m_head = (m_head + 1) % kCapacity;
    ++m_count;
}

// ---------------------------------------------------------------------------
// setVerbosity / getVerbosity
// ---------------------------------------------------------------------------

void LogBuffer::setVerbosity(const int v) {
    m_verbosity = v;
}

int LogBuffer::getVerbosity() const {
    return m_verbosity;
}

// ---------------------------------------------------------------------------
// Test-only accessors
// ---------------------------------------------------------------------------

int LogBuffer::size() const {
    return m_count;
}

LogEntry LogBuffer::at(const int idx) const {
    // idx 0 = oldest, idx m_count-1 = newest.
    // The oldest entry lives at (m_head - m_count + kCapacity) % kCapacity.
    const int oldest = (m_head - m_count + kCapacity) % kCapacity;
    const int ring = (oldest + idx) % kCapacity;
    return m_entries[ring];
}

