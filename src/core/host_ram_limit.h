// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Core {

/// Caps the host RAM of the emulator for the titles that need it, and lifts the cap for the others.
/// A guest loads far more than it touches each frame, so the host pages the rest out instead of
/// keeping it resident. EDEN_NX2_RAM_LIMIT_MIB=<n> overrides the limit, 0 disables it.
void ApplyHostRamLimit(u64 program_id);

} // namespace Core
