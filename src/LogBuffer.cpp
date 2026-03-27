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

#include <cstdarg> // va_list
#include <cstdio> // snprintf, vsnprintf
#include <string>

// ---------------------------------------------------------------------------
// push() — the core operation.
//
// Overflow path (m_count == kCapacity):
//   First overflow (m_overflowing == false):
//     1. Evict the oldest entry; write a sentinel WARN at that slot:
//        "1 earlier entry was dropped (buffer full)".
//     2. Advance m_head; set m_overflowing = true.
//     3. Fall through to the pure-ring write below.
//   Subsequent overflows (m_overflowing == true):
//     Just evict the oldest entry and store the new one. No sentinel spam.
//   m_count never exceeds kCapacity.
// ---------------------------------------------------------------------------

void LogBuffer::log(const LogEntry::Severity sev, const char *msg) { push(LogEntry(sev, msg)); }

void LogBuffer::logf(const LogEntry::Severity sev, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char buf[512];
    va_list ap2;
    va_copy(ap2, ap);
    const int written = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if ( written < static_cast<int>(sizeof(buf)) ) {
        va_end(ap2);
        push(LogEntry(sev, buf));
        return;
    }

    std::string large(static_cast<size_t>(written), '\0');
    vsnprintf(large.data(), static_cast<size_t>(written) + 1, fmt, ap2);
    va_end(ap2);
    push(LogEntry(sev, large.c_str()));
}

bool LogBuffer::hasErrorsOrWarnings() const { return m_hasErrorsOrWarnings; }

void LogBuffer::push(const LogEntry &entry) {
    if ( entry.severity == LogEntry::ERR || entry.severity == LogEntry::WARN ) {
        m_hasErrorsOrWarnings = true;
    }

    if ( m_entries.size() < kCapacity ) {
        m_entries.push_back(entry);
        return;
    }

    if ( !m_overflowing ) {
        static constexpr char kSentinel[] = "1 earlier entry was dropped (buffer full)";
        m_entries[m_head] = LogEntry(LogEntry::WARN, kSentinel);
        m_head = (m_head + 1) % kCapacity;
        m_overflowing = true;
    }
    m_entries[m_head] = entry;
    m_head = (m_head + 1) % kCapacity;
}


size_t LogBuffer::size() const { return m_entries.size(); }

const LogEntry &LogBuffer::at(const int idx) const { return m_entries[idx]; }
