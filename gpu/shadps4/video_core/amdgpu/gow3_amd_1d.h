// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: diagnostics and switches for 1D images on AMD's Windows driver (iss_pc #16, #31, #49,
// #53). With BC decoding (gow3_bc_tiny.h) the RX 580 still writes through a null pointer at
// amdvlk64.dll+0xebbbb right after the 512x1, 32-layer, 10-mip R8G8B8A8Srgb 1D image is made,
// so the cause is the 1D image itself, not the compression.
//   GOW3_IMG_TRACE=1/0: print (and flush) every step for 1D images: create, view, upload record,
//                      upload execution. Default: on for AMD's proprietary driver.
//   GOW3_1D_NO_STORAGE=1: create 1D images without storage usage.
//   GOW3_1D_NO_MUTABLE=1: create 1D images without the mutable-format/extended-usage flags.
#pragma once

#include <atomic>
#include <cstdio>

namespace AmdGpu {

inline std::atomic<bool> img_trace{false};
inline std::atomic<bool> img_1d_no_storage{false};
inline std::atomic<bool> img_1d_no_mutable{false};

inline bool ImgTrace() {
    return img_trace.load(std::memory_order_relaxed);
}

} // namespace AmdGpu

// Printed and flushed at once: the driver may crash on the very next call.
#define GOW3_IMG_TRACE(...)                                                                         \
    do {                                                                                           \
        if (::AmdGpu::ImgTrace()) {                                                                \
            std::printf("GPU trace: " __VA_ARGS__);                                                \
            std::fflush(stdout);                                                                   \
        }                                                                                          \
    } while (0)
