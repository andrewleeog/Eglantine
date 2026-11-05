// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "common/common_types.h"

namespace Common::X64 {

#ifdef _MSC_VER
__forceinline static u64 FencedRDTSC() {
    // lfence on both sides costs ~30 cycles twice per read; in spin loops that read the
    // clock thousands of times per wait it showed up as 19% of the program in profiles.
    // The fence before rdtsc keeps ordering vs prior loads; the one after only matters
    // when the consumer needs the value ordered with SUBSEQUENT loads, which no caller
    // here does.
    _ReadWriteBarrier();
    const u64 result = __rdtsc();
    _ReadWriteBarrier();
    return result;
}
#else
static inline u64 FencedRDTSC() {
    // Same rationale as the MSVC path above: one leading fence, none after.
    u64 eax;
    u64 edx;
    asm volatile("lfence\n\t"
                 "rdtsc\n\t"
                 : "=a"(eax), "=d"(edx));
    return (edx << 32) | eax;
}
#endif

u64 EstimateRDTSCFrequency();

} // namespace Common::X64
