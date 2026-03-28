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
    if ( entry.severity > m_minSeverity ) {
        return;
    }

    if ( m_entries.size() >= kCapacity ) {
        if ( m_droppedCount == 0 ) {
            m_entries[m_entries.size() - 1].severity = LogEntry::WARN;
            m_entries[m_entries.size() - 1].text = "--- Too many entries ---";
        }
        ++m_droppedCount;
        return;
    }

    if ( entry.severity == LogEntry::ERR || entry.severity == LogEntry::WARN ) {
        m_hasErrorsOrWarnings = true;
    }
    m_entries.push_back(entry);
}


size_t LogBuffer::size() const { return m_entries.size(); }
size_t LogBuffer::droppedCount() const { return m_droppedCount; }

const LogEntry &LogBuffer::at(const int idx) const { return m_entries[idx]; }
