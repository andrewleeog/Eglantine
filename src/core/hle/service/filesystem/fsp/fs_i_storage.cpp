// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <thread>

#include "core/file_sys/errors.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/filesystem/fsp/fs_i_storage.h"

#include "common/scratch_buffer.h"
#include "core/core.h"
#include "core/device_memory.h"

#include "video_core/gpu.h"
#include "video_core/host1x/host1x.h"
#include <cstdlib>
#include <atomic>

namespace Service::FileSystem {

IStorage::IStorage(Core::System& system_, FileSys::VirtualFile backend_)
    : ServiceFramework{system_, "IStorage"}, backend(std::move(backend_)) {
    static const FunctionInfo functions[] = {
        {0, D<&IStorage::Read>, "Read"},
        {1, nullptr, "Write"},
        {2, nullptr, "Flush"},
        {3, nullptr, "SetSize"},
        {4, D<&IStorage::GetSize>, "GetSize"},
        {5, nullptr, "OperateRange"},
    };
    RegisterHandlers(functions);
}

Result IStorage::Read(
    OutBuffer<BufferAttr_HipcMapAlias | BufferAttr_HipcMapTransferAllowsNonSecure> out_bytes,
    s64 offset, s64 length) {
    LOG_DEBUG(Service_FS, "called, offset={:#x}, length={}", offset, length);
    if (std::getenv("EDEN_NX2_FS_PROBE") != nullptr && length >= 8) {
        static std::atomic<u32> logged{};
        if (logged.fetch_add(1) < 5000) {
            LOG_INFO(Service_FS, "STORPROBE dest={:#x} off={:#x} len={}",
                     static_cast<u64>(reinterpret_cast<uintptr_t>(out_bytes.data())), offset, length);
        }
    }

    R_UNLESS(length >= 0, FileSys::ResultInvalidSize);
    R_UNLESS(offset >= 0, FileSys::ResultInvalidOffset);

    const auto now = std::chrono::steady_clock::now();
    if (now < next_read_tick) {
        std::this_thread::sleep_for(next_read_tick - now);
    }
    next_read_tick = std::chrono::steady_clock::now() +
                     std::chrono::nanoseconds{std::min(length, MaxChargedReadSize) *
                                              1'000'000'000 / ReadBytesPerSecond};

    // Read the data from the Storage backend
    backend->Read(out_bytes.data(), length, offset);

    // The read lands in guest memory behind the GPU's back: without this the texture and buffer
    // caches keep serving their stale (often zero-filled) copies of these pages, which renders as
    // black, untextured objects once streamed content arrives after the image was first sampled.
    // The read wrote through a host pointer: the caches are told about the page the same way the
    // VI writer does, by translating the pointer to its device address. Before the first frame the
    // caches are still being built, so nothing is cached yet.
    if (length > 0 && Core::g_nx2_present_count.load() > 0) {
        // The buffer may be a temporary outside the device memory arena, where the pointer to
        // device address translation does not apply: only invalidate when it is inside it.
        const auto* const arena = system.DeviceMemory().GetPointer<u8>(Core::DramMemoryMap::Base);
        const auto begin = reinterpret_cast<uintptr_t>(arena);
        const auto at = reinterpret_cast<uintptr_t>(out_bytes.data());
        constexpr uintptr_t ArenaLimit = 16ull << 30;
        if (at >= begin && at - begin < ArenaLimit) {
            static thread_local Common::ScratchBuffer<u32> scratch;
            system.Host1x().MemoryManager().ApplyOpOnPAddr(
                static_cast<PAddr>(at - begin), scratch,
                [&](DAddr addr) { system.GPU().InvalidateRegion(addr, length); });
        }
    }

    R_SUCCEED();
}

Result IStorage::GetSize(Out<u64> out_size) {
    *out_size = backend->GetSize();

    LOG_DEBUG(Service_FS, "called, size={}", *out_size);

    R_SUCCEED();
}

} // namespace Service::FileSystem
