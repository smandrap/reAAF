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

#ifndef REAPER_AAF_AAFIHANDLE_H
#define REAPER_AAF_AAFIHANDLE_H

extern "C" {
#include "libaaf/AAFIface.h"
}

// Why bother? To prevent a very unlikely memory leak in case AafImporter constructor fails after
// calling aafi_alloc(). In that (incredibly unlikely) case, aafi_release is never called. RAII it is.

struct AafiHandle {
    AAF_Iface *ptr = nullptr;
    explicit AafiHandle(AAF_Iface *p) : ptr(p) {} // no raw pointers allowed sir
    ~AafiHandle() { if (ptr) aafi_release(&ptr); } // autorelease on scope end

    // prevent copy
    AafiHandle(const AafiHandle &) = delete;
    AafiHandle &operator=(const AafiHandle &) = delete;

    // move constructor
    AafiHandle(AafiHandle &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }

    AAF_Iface *operator->() const { return ptr; }
    [[nodiscard]] AAF_Iface *get() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};


#endif //REAPER_AAF_AAFIHANDLE_H
