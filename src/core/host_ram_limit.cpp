// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdlib>

#include "common/literals.h"
#include "common/logging.h"
#include "core/host_ram_limit.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Core {

#ifdef _WIN32
namespace {

using namespace Common::Literals;

constexpr u64 MarioKartWorldProgramId = 0x0400C3F00006E000ULL;
// Measured on its attract loop with limits of 3, 4, 5 and 7 GiB: same frame times as without one.
constexpr u64 MarioKartWorldLimit = 4_GiB;
constexpr SIZE_T MinimumWorkingSet = 200_MiB;

u64 LimitFor(u64 program_id) {
    if (const char* const value = std::getenv("EDEN_NX2_RAM_LIMIT_MIB")) {
        return program_id == MarioKartWorldProgramId ? std::strtoull(value, nullptr, 10) << 20 : 0;
    }
    return program_id == MarioKartWorldProgramId ? MarioKartWorldLimit : 0;
}

} // Anonymous namespace

void ApplyHostRamLimit(u64 program_id) {
    static bool limit_active = false;
    const u64 limit = LimitFor(program_id);
    if (limit == 0 && !limit_active) {
        return;
    }
    const DWORD flags = limit != 0
                            ? QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_ENABLE
                            : QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE;
    const SIZE_T maximum = limit != 0 ? static_cast<SIZE_T>(limit) : static_cast<SIZE_T>(1_GiB);
    if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), MinimumWorkingSet, maximum, flags)) {
        LOG_WARNING(Core, "Could not set the host RAM limit: error {}", GetLastError());
        return;
    }
    limit_active = limit != 0;
    LOG_INFO(Core, "Host RAM limit: {} MiB (0 = none)", limit >> 20);
}
#else
void ApplyHostRamLimit(u64) {}
#endif

} // namespace Core
