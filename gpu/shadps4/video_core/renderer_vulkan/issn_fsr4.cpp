// SPDX-License-Identifier: GPL-2.0-or-later
// issn-axy: FSR 4 Native AA in the T2x resolve's place (issn_fsr4.h). Ported from the port's
// previous core (vk_fsr4.cpp of branch issn), without its 4.1.1 replay.
#include "video_core/renderer_vulkan/issn_fsr4.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <utility>
#include <vector>

#include "common/logging/log.h"
#include "ffx_vk_fsr4_v07.h"
#include "ffx_vk_fsr4_v07_assets.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/image.h"

namespace Vulkan::Fsr4Aa {

namespace {

std::atomic<bool> g_device_supported{false};

std::string AssetDir() {
    const char* dir = std::getenv("ISSN_FSR4_DIR");
    return dir && dir[0] ? dir : "fsr4_shaders";
}

bool ReadFile(const std::string& path, std::vector<u8>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    data.resize(size_t(file.tellg()));
    file.seekg(0);
    return bool(file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size())));
}

constexpr vk::ImageSubresourceRange ColorRange{
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .levelCount = 1,
    .layerCount = 1,
};

} // namespace

bool Wanted() {
    static const bool wanted = [] {
        const char* v = std::getenv("ISSN_FSR4");
        return v && v[0] == '1';
    }();
    return wanted;
}

bool DeviceSupported() {
    return g_device_supported.load(std::memory_order_relaxed);
}

void SetDeviceSupported(bool supported) {
    g_device_supported.store(supported, std::memory_order_relaxed);
}

struct Upscaler::Impl {
    const Instance& instance;
    Scheduler& scheduler;
    std::vector<u8> scratch;
    FfxInterface backend{};
    bool backend_ok = false;
    ffxContext context{};
    bool context_ok = false;
    u32 out_width = 0, out_height = 0;
    std::string problem;
    bool fatal = false;
    u64 next_frame_id = 1;
    std::deque<std::pair<u64, u64>> in_flight; ///< provider frame id, scheduler tick
    // FSR writes RGBA16F (the v07 post pass declares it); blitted into the game's RGBA8 target.
    VideoCore::UniqueImage target;
    vk::UniqueImageView target_view;
    bool target_fresh = true;
    std::deque<std::pair<u64, VideoCore::UniqueImage>> retired_images;
    std::deque<std::pair<u64, vk::UniqueImageView>> retired_views;

    Impl(const Instance& instance_, Scheduler& scheduler_)
        : instance{instance_}, scheduler{scheduler_} {}

    ~Impl() {
        // The device is idle when the rasterizer goes (shutdown).
        if (context_ok) {
            ffxFsr4V07DestroyContext(&context, nullptr);
        }
        if (backend_ok) {
            ffxFsr4VkDestroyContext(reinterpret_cast<FfxFsr4VkContext*>(scratch.data()));
        }
    }

    void Fail(std::string reason, bool permanent) {
        if (problem != reason) {
            LOG_WARNING(Render_Vulkan, "FSR 4 unavailable: {} - the game's own anti-aliasing stays",
                        reason);
            std::printf("FSR 4: %s\n", reason.c_str());
            std::fflush(stdout);
        }
        problem = std::move(reason);
        fatal |= permanent;
    }

    bool CreateBackend(u32 ow, u32 oh) {
        FfxFsr4V07AssetSet assets{};
        if (!ffxFsr4V07BuildAssetSet(FFX_FSR4_MODEL_PRESET_NATIVE_AA, ow, oh, &assets)) {
            Fail("output size is not supported by the v07 model", true);
            return false;
        }
        const std::string dir = AssetDir() + "/";
        std::array<std::vector<u8>, FFX_FSR4_VK_PASS_COUNT> code;
        std::vector<u8> initializer, weights;
        const auto load = [&](const char* name, std::vector<u8>& data) {
            if (ReadFile(dir + "opt/" + name, data) || ReadFile(dir + name, data)) {
                return true;
            }
            Fail("missing " + dir + name + " (launcher: Graphics > Download FSR 4)", true);
            return false;
        };
        if (!load(assets.pre, code[0])) {
            return false;
        }
        for (u32 pass = 0; pass < FFX_FSR4_MODEL_PASS_COUNT; ++pass) {
            if (!load(assets.model[pass], code[1 + pass])) {
                return false;
            }
        }
        if (!load(assets.post, code[13]) || !load(assets.rcas, code[14]) ||
            !load(assets.spdAutoExposure, code[15]) || !load(assets.initializer, initializer) ||
            !load(assets.prePassWeights, weights)) {
            return false;
        }
        if (initializer.size() != FFX_FSR4_V07_INITIALIZER_BYTES ||
            weights.size() != FFX_FSR4_V07_PRE_PASS_WEIGHTS_BYTES) {
            Fail("model weights have an unexpected size", true);
            return false;
        }
        static const std::array<std::string, FFX_FSR4_VK_PASS_COUNT> entries = [] {
            std::array<std::string, FFX_FSR4_VK_PASS_COUNT> names;
            names.fill("main");
            for (u32 pass = 1; pass <= FFX_FSR4_MODEL_PASS_COUNT; ++pass) {
                names[pass] = "fsr4_model_v07_i8_pass" + std::to_string(pass);
            }
            return names;
        }();
        FfxFsr4VkCreateInfo ci{};
        ci.device = instance.GetDevice();
        ci.physicalDevice = instance.GetPhysicalDevice();
        for (u32 i = 0; i < FFX_FSR4_VK_PASS_COUNT; ++i) {
            if (code[i].size() % 4) {
                Fail("invalid SPIR-V asset", true);
                return false;
            }
            ci.shaders[i] = {reinterpret_cast<const uint32_t*>(code[i].data()), code[i].size(),
                             entries[i].c_str()};
        }
        ci.modelInitializer = initializer.data();
        ci.modelInitializerSize = initializer.size();
        ci.prePassWeights = weights.data();
        ci.prePassWeightsSize = weights.size();
        scratch.assign(ffxFsr4VkGetScratchMemorySize(), 0);
        ci.scratchBuffer = scratch.data();
        ci.scratchBufferSize = scratch.size();
        if (const VkResult result = ffxFsr4VkCreateContext(&ci, &backend); result != VK_SUCCESS) {
            Fail("Vulkan backend creation failed (" + std::to_string(int(result)) + ")", true);
            backend = {};
            return false;
        }
        backend_ok = true;

        ffxCreateContextDescUpscale desc{};
        desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
        // The input is the game's tonemapped RGBA8 frame (no HDR, no exposure) and its depth is
        // reversed (GreaterEqual).
        desc.flags = FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
        desc.maxRenderSize = {(ow + 7) & ~7u, (oh + 7) & ~7u};
        desc.maxUpscaleSize = {(ow + 7) & ~7u, (oh + 7) & ~7u};
        ffxFsr4V07SetBackendInterface(&backend);
        const auto created = ffxFsr4V07CreateContext(&context, &desc.header, nullptr);
        ffxFsr4V07SetBackendInterface(nullptr);
        if (created != FFX_API_RETURN_OK) {
            Fail("provider context creation failed (" + std::to_string(created) + ")", true);
            return false;
        }
        context_ok = true;
        out_width = ow;
        out_height = oh;
        LOG_WARNING(Render_Vulkan, "FSR 4: v07 INT8 Native AA running, {} tier, {}x{}", assets.tier, ow,
                    oh);
        std::printf("FSR 4: v07 INT8 Native AA, %s tier, %ux%u\n", assets.tier, ow, oh);
        std::fflush(stdout);
        return true;
    }

    void Retire() {
        while (!in_flight.empty() && scheduler.IsFree(in_flight.front().second)) {
            ffxFsr4VkRetireFrame(&backend, in_flight.front().first);
            in_flight.pop_front();
        }
        while (!retired_images.empty() && scheduler.IsFree(retired_images.front().first)) {
            retired_images.pop_front();
        }
        while (!retired_views.empty() && scheduler.IsFree(retired_views.front().first)) {
            retired_views.pop_front();
        }
    }

    void MakeTarget(u32 width, u32 height) {
        const u64 tick = scheduler.CurrentTick();
        if (target_view) {
            retired_views.emplace_back(tick, std::move(target_view));
        }
        if (target) {
            retired_images.emplace_back(tick, std::move(target));
        }
        const vk::Device device = instance.GetDevice();
        target = VideoCore::UniqueImage(device, instance.GetAllocator());
        target.Create(vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent{width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
            .initialLayout = vk::ImageLayout::eUndefined,
        });
        auto [result, view] = device.createImageViewUnique({
            .image = target,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .subresourceRange = ColorRange,
        });
        target_view = std::move(view);
        target_fresh = true;
    }

    VkResult Register(vk::Image image, vk::ImageView view, VkAccessFlags access) {
        const FfxFsr4VkExternalImageState state{
            .structSize = sizeof(FfxFsr4VkExternalImageState),
            .image = image,
            .view = view,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
            .stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .accessMask = access,
            .restoreLayout = VK_IMAGE_LAYOUT_GENERAL,
            .restoreStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .restoreAccessMask = access,
        };
        return ffxFsr4VkSetExternalImageState(&backend, &state);
    }

    bool Record(vk::CommandBuffer cmdbuf, const Input& color, const Input& motion,
                const Input& depth, vk::Image output, u32 ow, u32 oh, float jx, float jy,
                bool reset) {
        if (fatal) {
            return false;
        }
        if (!DeviceSupported()) {
            Fail("the GPU lacks INT8 dot products / compute derivatives (FSR 4 needs a Radeon "
                 "RX 7000 / 9000 or similar)",
                 true);
            return false;
        }
        if (!context_ok || ow != out_width || oh != out_height) {
            if (context_ok || backend_ok) {
                // A size change (rare: the game renders 1920x1080) - the old context's frames
                // must be done first.
                scheduler.Finish();
                if (context_ok) {
                    ffxFsr4V07DestroyContext(&context, nullptr);
                    context_ok = false;
                }
                if (backend_ok) {
                    ffxFsr4VkDestroyContext(reinterpret_cast<FfxFsr4VkContext*>(scratch.data()));
                    backend_ok = false;
                }
                in_flight.clear();
            }
            if (!CreateBackend(ow, oh)) {
                return false;
            }
            MakeTarget(ow, oh);
            reset = true;
        }
        Retire();
        const u64 frame_id = next_frame_id++;
        VkResult begin = ffxFsr4VkBeginFrame(&backend, frame_id);
        while (begin == VK_NOT_READY && !in_flight.empty()) {
            scheduler.Wait(in_flight.front().second);
            Retire();
            begin = ffxFsr4VkBeginFrame(&backend, frame_id);
        }
        if (begin != VK_SUCCESS) {
            Fail("no free provider frame (" + std::to_string(int(begin)) + ")", false);
            return false;
        }
        // A frame begun must reach RetireFrame even when it records nothing.
        const auto abandon = [&] { in_flight.emplace_back(frame_id, scheduler.CurrentTick()); };

        // FSR's output image: General for the compute passes.
        cmdbuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader, {}, {}, {},
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                .dstAccessMask =
                    vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                .oldLayout = target_fresh ? vk::ImageLayout::eUndefined
                                          : vk::ImageLayout::eTransferSrcOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = target,
                .subresourceRange = ColorRange,
            });
        target_fresh = false;

        constexpr VkAccessFlags read = VK_ACCESS_SHADER_READ_BIT;
        const std::array<std::tuple<vk::Image, vk::ImageView, VkAccessFlags, const char*>, 4>
            images{{
                {color.image, color.view, read, "color"},
                {depth.image, depth.view, read, "depth"},
                {motion.image, motion.view, read, "motion"},
                {target, *target_view, read | VK_ACCESS_SHADER_WRITE_BIT, "output"},
            }};
        for (const auto& [image, view, access, name] : images) {
            if (const VkResult result = Register(image, view, access); result != VK_SUCCESS) {
                Fail(std::string{"external image registration failed ("} + name + ", " +
                         std::to_string(int(result)) + ")",
                     false);
                abandon();
                return false;
            }
        }
        const auto resource = [](vk::ImageView view, u32 w, u32 h, u32 format, u32 state) {
            FfxApiResource r{};
            r.resource = reinterpret_cast<void*>(VkImageView(view));
            r.description.type = FFX_RESOURCE_TYPE_TEXTURE2D;
            r.description.format = format;
            r.description.width = w;
            r.description.height = h;
            r.description.depth = 1;
            r.description.mipCount = 1;
            r.state = state;
            return r;
        };
        ffxDispatchDescUpscale d{};
        d.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
        d.commandList = reinterpret_cast<void*>(VkCommandBuffer(cmdbuf));
        d.color = resource(color.view, color.width, color.height,
                           FFX_SURFACE_FORMAT_R8G8B8A8_UNORM, FFX_API_RESOURCE_STATE_COMPUTE_READ);
        d.depth = resource(depth.view, depth.width, depth.height, FFX_SURFACE_FORMAT_R32_FLOAT,
                           FFX_API_RESOURCE_STATE_COMPUTE_READ);
        d.motionVectors = resource(motion.view, motion.width, motion.height,
                                   FFX_SURFACE_FORMAT_R16G16_FLOAT,
                                   FFX_API_RESOURCE_STATE_COMPUTE_READ);
        d.output = resource(*target_view, ow, oh, FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                            FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        d.jitterOffset = {jx, jy};
        // The game's vectors are screen UV, current minus previous; FSR wants render pixels
        // towards the previous position. ISSN_FSR4_MV=+ flips the sign (calibration).
        static const float mv_sign = [] {
            const char* v = std::getenv("ISSN_FSR4_MV");
            return v && v[0] == '+' ? 1.0f : -1.0f;
        }();
        d.motionVectorScale = {mv_sign * float(motion.width), mv_sign * float(motion.height)};
        d.renderSize = {color.width, color.height};
        d.upscaleSize = {ow, oh};
        // ISSN_FSR4_SHARPNESS (0..1, default 0.2): RCAS after the model.
        static const float sharpness = [] {
            const char* v = std::getenv("ISSN_FSR4_SHARPNESS");
            const float s = v && *v ? float(std::atof(v)) : 0.2f;
            return s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        }();
        d.enableSharpening = sharpness > 0.0f;
        d.sharpness = sharpness;
        d.enableAutoExposure = false;
        d.frameTimeDelta = 16.6f;
        d.preExposure = 1.0f;
        d.reset = reset;
        d.cameraNear = 0.1f;
        d.cameraFar = 10000.0f;
        d.cameraFovAngleVertical = 1.0f;
        d.viewSpaceToMetersFactor = 1.0f;
        if (const auto result = ffxFsr4V07Dispatch(&context, &d.header);
            result != FFX_API_RETURN_OK) {
            Fail("dispatch failed (" + std::to_string(result) + ")", false);
            abandon();
            return false;
        }
        in_flight.emplace_back(frame_id, scheduler.CurrentTick());

        // FSR's RGBA16F result into the resolve's target (layout TransferDst, set by the caller).
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                               vk::ImageMemoryBarrier{
                                   .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                                   .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                                   .oldLayout = vk::ImageLayout::eGeneral,
                                   .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .image = target,
                                   .subresourceRange = ColorRange,
                               });
        const vk::ImageSubresourceLayers layers{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        const vk::ImageBlit blit{
            .srcSubresource = layers,
            .srcOffsets = std::array{vk::Offset3D{0, 0, 0}, vk::Offset3D{s32(ow), s32(oh), 1}},
            .dstSubresource = layers,
            .dstOffsets = std::array{vk::Offset3D{0, 0, 0}, vk::Offset3D{s32(ow), s32(oh), 1}},
        };
        cmdbuf.blitImage(target, vk::ImageLayout::eTransferSrcOptimal, output,
                         vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);
        problem.clear();
        return true;
    }
};

Upscaler::Upscaler(const Instance& instance, Scheduler& scheduler)
    : impl{std::make_unique<Impl>(instance, scheduler)} {}

Upscaler::~Upscaler() = default;

bool Upscaler::Record(vk::CommandBuffer cmdbuf, const Input& color, const Input& motion,
                      const Input& depth, vk::Image output, u32 out_width, u32 out_height,
                      float jitter_x, float jitter_y, bool reset) {
    return impl->Record(cmdbuf, color, motion, depth, output, out_width, out_height, jitter_x,
                        jitter_y, reset);
}

const std::string& Upscaler::Problem() const noexcept {
    return impl->problem;
}

} // namespace Vulkan::Fsr4Aa
