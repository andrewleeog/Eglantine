// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdlib>

#include "common/common_types.h"

/// Experimental draw-cost knobs for draw-heavy titles (all off by default):
///
/// EDEN_NX2_DRAW_BUDGET=<max_draws>:<min_vertices> - past max_draws draws in one frame,
/// skip draws with fewer than min_vertices vertices. Particle systems and small props are
/// thousands of tiny draws; their per-draw resolve cost is what drowns the GPU thread in
/// draw-heavy views. Skipped draws vanish (particles thin out); terrain and characters
/// (large vertex counts) always draw. "0:0" = off.
///
/// EDEN_NX2_FRAME_SKIP=<n> - run the game's graphics at 60/n: on the skipped frames the
/// GPU thread does no draw work at all and the last composited image stays up. The game's
/// simulation keeps its own pace, so the world still moves at real speed; input and
/// camera respond at the reduced rate. n=1 (default) = off.
namespace VideoCommon::Nx2DrawBudget {

struct Knobs {
    u32 max_draws{};      ///< 0 = unlimited
    u32 min_vertices{};   ///< draws under this many vertices are skippable
    u32 frame_skip{};     ///< render every Nth frame; 1 = every frame

    constexpr bool BudgetActive() const noexcept {
        return max_draws != 0;
    }
    constexpr bool SkipActive() const noexcept {
        return frame_skip > 1;
    }
};

inline const Knobs g_knobs = [] {
    Knobs knobs{};
    if (const char* const value = std::getenv("EDEN_NX2_DRAW_BUDGET")) {
        // "<max_draws>:<min_vertices>"; the min_vertices part is optional (default 64)
        char* end{};
        const long long max_draws = std::strtoll(value, &end, 10);
        if (max_draws > 0) {
            knobs.max_draws = static_cast<u32>(max_draws);
            long long min_vertices = 64;
            if (end && *end == ':') {
                min_vertices = std::strtoll(end + 1, nullptr, 10);
                if (min_vertices < 0) {
                    min_vertices = 0;
                }
            }
            knobs.min_vertices = static_cast<u32>(min_vertices);
        }
    }
    if (const char* const value = std::getenv("EDEN_NX2_FRAME_SKIP")) {
        const long long n = std::strtoll(value, nullptr, 10);
        knobs.frame_skip = n > 1 ? static_cast<u32>(n) : 1;
    }
    return knobs;
}();

/// Per-GPU-thread-frame draw accounting. Reset in TickFrame.
inline std::atomic<u32> g_draws_this_frame{0};
inline std::atomic<u64> g_frame_counter{0};
/// The last frame's skip decision (the present path reads it too).
inline std::atomic<bool> g_frame_skipped{false};

/// Call at the start of each GPU-thread frame. Returns true when this frame's draws are
/// skipped entirely (frame-skip mode).
inline bool BeginFrame() {
    g_draws_this_frame.store(0, std::memory_order_relaxed);
    const u64 frame = g_frame_counter.fetch_add(1, std::memory_order_relaxed);
    const bool skipped = g_knobs.SkipActive() && (frame % g_knobs.frame_skip) != 0;
    g_frame_skipped.store(skipped, std::memory_order_relaxed);
    return skipped;
}

/// Call before recording a draw. Returns false when the draw must be skipped (over the
/// budget and small enough to be a particle/fragment draw).
inline bool AllowDraw(u32 vertex_count) {
    const Knobs& knobs = g_knobs;
    if (!knobs.BudgetActive() || g_frame_skipped.load(std::memory_order_relaxed)) {
        return true;
    }
    const u32 draws = g_draws_this_frame.fetch_add(1, std::memory_order_relaxed);
    return draws < knobs.max_draws || vertex_count >= knobs.min_vertices;
}

} // namespace VideoCommon::Nx2DrawBudget
