// SPDX-License-Identifier: GPL-2.0-or-later
// issn-axy: FSR 4 (AMD's v07 INT8 model through the FSR-Vulkan provider, MIT) in the slot axylpup's
// DLSS uses: the game's SMAA T2x temporal resolve is replaced by FSR 4 Native AA, from the same
// inputs (tonemap output 0x2f9e10000, motion vectors 0x3059b0000, reversed-Z depth 0x26b200000,
// the game's +-0.25 px jitter). ISSN_FSR4=1 turns it on (vk_dlss.h Mode() == 3); the assets come
// from the launcher's download (ISSN_FSR4_DIR, else fsr4_shaders next to the working directory).
// Anything missing (assets, INT8 dot products, compute derivatives) leaves the game's own AA.

#pragma once

#include <memory>
#include <string>
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Scheduler;

namespace Fsr4Aa {

/// ISSN_FSR4=1.
bool Wanted();

struct Input {
    vk::Image image;
    vk::ImageView view;
    u32 width, height;
};

class Upscaler {
public:
    Upscaler(const Instance& instance, Scheduler& scheduler);
    ~Upscaler();

    /// Records FSR 4 Native AA into `cmdbuf`: color / motion / depth in layout General, `output`
    /// (the resolve's target, layout TransferDst afterwards is the caller's) receives the result
    /// through a blit from FSR's RGBA16F image. `motion_uv`: vectors in screen UV, current minus
    /// previous (the game's). False = not run; the caller falls back to the game's picture.
    bool Record(vk::CommandBuffer cmdbuf, const Input& color, const Input& motion,
                const Input& depth, vk::Image output, u32 out_width, u32 out_height, float jitter_x,
                float jitter_y, bool reset);

    /// The last failure, or empty; permanent ones stop further attempts.
    [[nodiscard]] const std::string& Problem() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// The device has what the v07 INT8 shaders need (set by vk_instance at device creation).
bool DeviceSupported();
void SetDeviceSupported(bool supported);

} // namespace Fsr4Aa
} // namespace Vulkan
