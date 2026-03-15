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

#include <cstdio>   // snprintf
#include <cstring>  // strlen

// ---------------------------------------------------------------------------
// push() — the core operation;
//
// Verbosity thresholds (enforced at write time, before any storage):
//   0 = None:    drop all (increment m_dropped, return)
//   1 = Normal:  keep ERROR (0) and WARN (1) only; drop INFO (2) and CLIP (3)
//   2 = Verbose: keep everything
//
// Overflow path (m_count == kCapacity):
//   1. Evict the oldest entry by advancing m_head (overwrites it).
//   2. Build sentinel with severity=WARN and text="<m_dropped+1> earlier
//      entries were dropped (buffer full)".  clipName is empty.
//   3. Write sentinel at m_head; advance m_head; reset m_dropped = 0;
//      do NOT change m_count (we just freed then immediately re-used a slot).
//   4. Now one slot remains free; write the new entry normally.
// ---------------------------------------------------------------------------

void LogBuffer::push(const LogEntry::Severity sev, const char *msg, const char *clipName) {
    // --- verbosity gate ---------------------------------------------------
    bool pass = false;
    switch (m_verbosity) {
        case 0: // None — drop everything
            pass = false;
            break;
        case 1: // Normal — keep ERROR and WARN only
            pass = sev == LogEntry::ERROR || sev == LogEntry::WARN;
            break;
        case 2: // Verbose — keep all
        default:
            pass = true;
            break;
    }

    if (!pass) {
        ++m_dropped;
        return;
    }

    // --- overflow handling ------------------------------------------------
    if (m_count == kCapacity) {
        // Evict oldest: advance head past the oldest entry (it gets overwritten).
        // After eviction we have one free slot at m_head — we'll use it for
        // the sentinel, then write the real entry in the next slot.

        // Build sentinel text: "N earlier entries were dropped (buffer full)"
        // N = m_dropped + 1 (the +1 accounts for the entry about to be evicted)
        char sentinelText[80];
        snprintf(sentinelText, sizeof(sentinelText),
                 "%d earlier entries were dropped (buffer full)",
                 m_dropped + 1);

        // Write sentinel at m_head (the evicted slot).
        LogEntry &sentinel = m_entries[m_head];
        sentinel.severity = LogEntry::WARN;
        sentinel.text = sentinelText;
        sentinel.clipName = "";
        m_head = (m_head + 1) % kCapacity;
        // m_count stays at kCapacity (evicted one, wrote sentinel → net zero)
        m_dropped = 0;

        // Now write the new entry at the next position (evicts another oldest).
        // m_count is still kCapacity so we advance m_head past the next oldest.
        LogEntry &entry = m_entries[m_head];
        entry.severity = sev;
        entry.text = msg ? msg : "";
        entry.clipName = (clipName && *clipName) ? clipName : "";
        m_head = (m_head + 1) % kCapacity;
        // m_count unchanged — still kCapacity.
        return;
    }

    // --- normal path (buffer not full) ------------------------------------
    LogEntry &entry = m_entries[m_head];
    entry.severity = sev;
    entry.text = msg ? msg : "";
    entry.clipName = (clipName && *clipName) ? clipName : "";
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

// ---------------------------------------------------------------------------
// drainNew — incremental drain used by ProgressDialog's timer callback.
//
// readPos: caller's logical read cursor (0 = oldest entry ever seen).
//   If readPos < (total - kCapacity) the cursor is stale due to overflow —
//   clamp it to max(0, total - kCapacity) before reading to avoid returning
//   garbage indices. An overflow is only possible if >kCapacity entries were
//   pushed since the last drain, which at 100 ms tick rate means >2000 entries
//   per 100 ms — extremely unlikely, but handled for correctness.
//
// Returns the new read position (always == total after drain completes).
// ---------------------------------------------------------------------------

size_t LogBuffer::drainNew(size_t readPos, std::vector<LogEntry> &out) const {
    const int total = m_count;
    // Clamp stale cursor: if overflow evicted entries before readPos, skip to
    // the oldest surviving entry.
    if (const int minPos = (total > kCapacity) ? (total - kCapacity) : 0; static_cast<int>(readPos) < minPos)
        readPos = static_cast<size_t>(minPos);
    for (int i = static_cast<int>(readPos); i < total; ++i)
        out.push_back(at(i));
    return static_cast<size_t>(total);
}
