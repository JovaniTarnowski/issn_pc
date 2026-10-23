// SPDX-License-Identifier: GPL-2.0-or-later
// issn-axy: FSR 4 v07 upscaling presets on any Vulkan 1.3 device with INT8 dot products and
// compute derivatives (e.g. Mesa's lavapipe on a PC without such a GPU):
//   fsr4-upscale-test <assets dir> [render_w render_h out_w out_h] ...
// For each size pair: the preset issn_fsr4.cpp picks for that ratio is loaded (opt/ first, as
// the game does), a smooth test picture is upscaled for a few frames with the game's +-0.25 px
// jitter and zero motion, and the result is read back. It must be finite and keep the picture:
// the mean colour within 2% and the mean absolute difference from the ideal picture within 4%.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "ffx_vk_fsr4_v07.h"
#include "ffx_vk_fsr4_v07_assets.h"

namespace {

VkInstance instance;
VkPhysicalDevice physical;
VkDevice device;
VkQueue queue;
uint32_t queue_family;
VkCommandPool pool;

#define CHECK_VK(x)                                                                                \
    do {                                                                                           \
        const VkResult r_ = (x);                                                                   \
        if (r_ != VK_SUCCESS) {                                                                    \
            std::fprintf(stderr, "%s failed: %d (line %d)\n", #x, int(r_), __LINE__);            \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

uint32_t MemoryType(uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    std::fprintf(stderr, "no memory type\n");
    std::exit(1);
}

struct Image {
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView view{};
    VkFormat format{};
    uint32_t w{}, h{};
};

Image MakeImage(VkFormat format, uint32_t w, uint32_t h) {
    Image img{.format = format, .w = w, .h = h};
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    CHECK_VK(vkCreateImage(device, &ci, nullptr, &img.image));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, img.image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_VK(vkAllocateMemory(device, &ai, nullptr, &img.memory));
    CHECK_VK(vkBindImageMemory(device, img.image, img.memory, 0));
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = img.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CHECK_VK(vkCreateImageView(device, &vi, nullptr, &img.view));
    return img;
}

struct Buffer {
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void* data{};
};

Buffer MakeBuffer(VkDeviceSize size) {
    Buffer b;
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size;
    ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    CHECK_VK(vkCreateBuffer(device, &ci, nullptr, &b.buffer));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, b.buffer, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK(vkAllocateMemory(device, &ai, nullptr, &b.memory));
    CHECK_VK(vkBindBufferMemory(device, b.buffer, b.memory, 0));
    CHECK_VK(vkMapMemory(device, b.memory, 0, size, 0, &b.data));
    return b;
}

VkCommandBuffer Begin() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    CHECK_VK(vkAllocateCommandBuffers(device, &ai, &cb));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK_VK(vkBeginCommandBuffer(cb, &bi));
    return cb;
}

void Submit(VkCommandBuffer cb) {
    CHECK_VK(vkEndCommandBuffer(cb));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    CHECK_VK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
    CHECK_VK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(device, pool, 1, &cb);
}

void Barrier(VkCommandBuffer cb, const Image& img, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img.image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);
}

uint16_t Half(float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    const uint32_t sign = (x >> 16) & 0x8000;
    int exp = int((x >> 23) & 0xff) - 127 + 15;
    uint32_t mant = x & 0x7fffff;
    if (exp <= 0) {
        return uint16_t(sign);
    }
    if (exp >= 31) {
        return uint16_t(sign | 0x7c00);
    }
    return uint16_t(sign | (exp << 10) | (mant >> 13));
}

float FromHalf(uint16_t h) {
    const uint32_t sign = uint32_t(h & 0x8000) << 16;
    const int exp = (h >> 10) & 0x1f;
    const uint32_t mant = h & 0x3ff;
    uint32_t x;
    if (exp == 0) {
        x = sign; // flush denormals
    } else if (exp == 31) {
        x = sign | 0x7f800000 | (mant << 13);
    } else {
        x = sign | uint32_t(exp - 15 + 127) << 23 | (mant << 13);
    }
    float f;
    std::memcpy(&f, &x, 4);
    return f;
}

/// The test picture in [0, 1] at normalized (u, v): smooth gradients and a soft ring.
std::array<float, 3> Picture(double u, double v) {
    const double r = std::hypot(u - 0.5, v - 0.5);
    return {float(0.2 + 0.6 * u), float(0.2 + 0.6 * v),
            float(0.5 + 0.3 * std::cos(r * 12.0))};
}

void Upload(const Image& img, const std::vector<uint8_t>& bytes) {
    Buffer staging = MakeBuffer(bytes.size());
    std::memcpy(staging.data, bytes.data(), bytes.size());
    VkCommandBuffer cb = Begin();
    Barrier(cb, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {img.w, img.h, 1};
    vkCmdCopyBufferToImage(cb, staging.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);
    Barrier(cb, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    Submit(cb);
    vkDestroyBuffer(device, staging.buffer, nullptr);
    vkFreeMemory(device, staging.memory, nullptr);
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    data.resize(size_t(f.tellg()));
    f.seekg(0);
    return bool(f.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size())));
}

// As issn_fsr4.cpp PresetFor.
FfxFsr4ModelPreset PresetFor(uint32_t rw, uint32_t rh, uint32_t ow, uint32_t oh) {
    const double ratio = std::max(double(ow) / rw, double(oh) / rh);
    return ratio <= 1.05   ? FFX_FSR4_MODEL_PRESET_NATIVE_AA
           : ratio <= 1.6  ? FFX_FSR4_MODEL_PRESET_QUALITY
           : ratio <= 1.85 ? FFX_FSR4_MODEL_PRESET_BALANCED
           : ratio <= 2.5  ? FFX_FSR4_MODEL_PRESET_PERFORMANCE
                           : FFX_FSR4_MODEL_PRESET_ULTRA_PERFORMANCE;
}

int RunCase(const std::string& dir, uint32_t rw, uint32_t rh, uint32_t ow, uint32_t oh) {
    const FfxFsr4ModelPreset preset = PresetFor(rw, rh, ow, oh);
    FfxFsr4V07AssetSet assets{};
    if (!ffxFsr4V07BuildAssetSet(preset, ow, oh, &assets)) {
        std::printf("FAIL %ux%u -> %ux%u: no asset set\n", rw, rh, ow, oh);
        return 1;
    }
    std::array<std::vector<uint8_t>, FFX_FSR4_VK_PASS_COUNT> code;
    std::vector<uint8_t> initializer, weights;
    const auto load = [&](const char* name, std::vector<uint8_t>& out) {
        if (ReadFile(dir + "/opt/" + name, out) || ReadFile(dir + "/" + name, out)) {
            return true;
        }
        std::printf("FAIL: missing %s\n", name);
        return false;
    };
    bool ok = load(assets.pre, code[0]);
    for (uint32_t p = 0; ok && p < FFX_FSR4_MODEL_PASS_COUNT; ++p) {
        ok = load(assets.model[p], code[1 + p]);
    }
    ok = ok && load(assets.post, code[13]) && load(assets.rcas, code[14]) &&
         load(assets.spdAutoExposure, code[15]) && load(assets.initializer, initializer) &&
         load(assets.prePassWeights, weights);
    if (!ok) {
        return 1;
    }
    std::array<std::string, FFX_FSR4_VK_PASS_COUNT> entries;
    entries.fill("main");
    for (uint32_t p = 1; p <= FFX_FSR4_MODEL_PASS_COUNT; ++p) {
        entries[p] = "fsr4_model_v07_i8_pass" + std::to_string(p);
    }
    FfxFsr4VkCreateInfo ci{};
    ci.device = device;
    ci.physicalDevice = physical;
    for (uint32_t i = 0; i < FFX_FSR4_VK_PASS_COUNT; ++i) {
        ci.shaders[i] = {reinterpret_cast<const uint32_t*>(code[i].data()), code[i].size(),
                         entries[i].c_str()};
    }
    ci.modelInitializer = initializer.data();
    ci.modelInitializerSize = initializer.size();
    ci.prePassWeights = weights.data();
    ci.prePassWeightsSize = weights.size();
    std::vector<uint8_t> scratch(ffxFsr4VkGetScratchMemorySize());
    ci.scratchBuffer = scratch.data();
    ci.scratchBufferSize = scratch.size();
    FfxInterface backend{};
    CHECK_VK(ffxFsr4VkCreateContext(&ci, &backend));
    ffxCreateContextDescUpscale desc{};
    desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    desc.flags = FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
    desc.maxRenderSize = {(rw + 7) & ~7u, (rh + 7) & ~7u};
    desc.maxUpscaleSize = {(ow + 7) & ~7u, (oh + 7) & ~7u};
    ffxContext context{};
    ffxFsr4V07SetBackendInterface(&backend);
    const auto created = ffxFsr4V07CreateContext(&context, &desc.header, nullptr);
    ffxFsr4V07SetBackendInterface(nullptr);
    if (created != FFX_API_RETURN_OK) {
        std::printf("FAIL: context %d\n", int(created));
        return 1;
    }

    Image color = MakeImage(VK_FORMAT_R16G16B16A16_SFLOAT, rw, rh);
    Image depth = MakeImage(VK_FORMAT_R32_SFLOAT, rw, rh);
    Image motion = MakeImage(VK_FORMAT_R16G16_SFLOAT, rw, rh);
    Image output = MakeImage(VK_FORMAT_R16G16B16A16_SFLOAT, ow, oh);
    {
        std::vector<uint8_t> bytes(size_t(rw) * rh * 8);
        auto* px = reinterpret_cast<uint16_t*>(bytes.data());
        for (uint32_t y = 0; y < rh; ++y) {
            for (uint32_t x = 0; x < rw; ++x) {
                const auto c = Picture((x + 0.5) / rw, (y + 0.5) / rh);
                uint16_t* p = px + (size_t(y) * rw + x) * 4;
                p[0] = Half(c[0]);
                p[1] = Half(c[1]);
                p[2] = Half(c[2]);
                p[3] = Half(1.0f);
            }
        }
        Upload(color, bytes);
        std::vector<uint8_t> zero(size_t(rw) * rh * 4, 0);
        Upload(motion, zero);
        std::vector<uint8_t> d(size_t(rw) * rh * 4);
        const float half = 0.5f;
        for (size_t i = 0; i < size_t(rw) * rh; ++i) {
            std::memcpy(d.data() + i * 4, &half, 4);
        }
        Upload(depth, d);
        VkCommandBuffer cb = Begin();
        Barrier(cb, output, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        Submit(cb);
    }
    const auto reg = [&](const Image& img, VkAccessFlags access) {
        const FfxFsr4VkExternalImageState state{
            .structSize = sizeof(FfxFsr4VkExternalImageState),
            .image = img.image,
            .view = img.view,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
            .stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .accessMask = access,
            .restoreLayout = VK_IMAGE_LAYOUT_GENERAL,
            .restoreStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .restoreAccessMask = access,
        };
        CHECK_VK(ffxFsr4VkSetExternalImageState(&backend, &state));
    };
    const auto resource = [](const Image& img, uint32_t format, uint32_t state) {
        FfxApiResource r{};
        r.resource = reinterpret_cast<void*>(img.view);
        r.description.type = FFX_RESOURCE_TYPE_TEXTURE2D;
        r.description.format = format;
        r.description.width = img.w;
        r.description.height = img.h;
        r.description.depth = 1;
        r.description.mipCount = 1;
        r.state = state;
        return r;
    };
    constexpr int Frames = 4;
    for (int frame = 0; frame < Frames; ++frame) {
        CHECK_VK(ffxFsr4VkBeginFrame(&backend, uint64_t(frame + 1)));
        VkCommandBuffer cb = Begin();
        reg(color, VK_ACCESS_SHADER_READ_BIT);
        reg(depth, VK_ACCESS_SHADER_READ_BIT);
        reg(motion, VK_ACCESS_SHADER_READ_BIT);
        reg(output, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        ffxDispatchDescUpscale d{};
        d.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
        d.commandList = cb;
        d.color = resource(color, FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                           FFX_API_RESOURCE_STATE_COMPUTE_READ);
        d.depth = resource(depth, FFX_SURFACE_FORMAT_R32_FLOAT, FFX_API_RESOURCE_STATE_COMPUTE_READ);
        d.motionVectors = resource(motion, FFX_SURFACE_FORMAT_R16G16_FLOAT,
                                   FFX_API_RESOURCE_STATE_COMPUTE_READ);
        d.output = resource(output, FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                            FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        const float j = (frame & 1) ? -0.25f : 0.25f;
        d.jitterOffset = {j, j};
        d.motionVectorScale = {-float(rw), -float(rh)};
        d.renderSize = {rw, rh};
        d.upscaleSize = {ow, oh};
        d.enableSharpening = true;
        d.sharpness = 0.2f;
        d.frameTimeDelta = 16.6f;
        d.preExposure = 1.0f;
        d.reset = frame == 0;
        d.cameraNear = 0.1f;
        d.cameraFar = 10000.0f;
        d.cameraFovAngleVertical = 1.0f;
        d.viewSpaceToMetersFactor = 1.0f;
        const auto result = ffxFsr4V07Dispatch(&context, &d.header);
        if (result != FFX_API_RETURN_OK) {
            std::printf("FAIL: dispatch %d\n", int(result));
            return 1;
        }
        Submit(cb);
        ffxFsr4VkRetireFrame(&backend, uint64_t(frame + 1));
    }
    // Read the output back and compare it with the ideal picture at the output size.
    Buffer readback = MakeBuffer(size_t(ow) * oh * 8);
    {
        VkCommandBuffer cb = Begin();
        Barrier(cb, output, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {ow, oh, 1};
        vkCmdCopyImageToBuffer(cb, output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback.buffer, 1, &copy);
        Submit(cb);
    }
    const auto* px = static_cast<const uint16_t*>(readback.data);
    double sum[3]{}, ideal_sum[3]{}, diff = 0.0;
    size_t bad = 0;
    for (uint32_t y = 0; y < oh; ++y) {
        for (uint32_t x = 0; x < ow; ++x) {
            const auto c = Picture((x + 0.5) / ow, (y + 0.5) / oh);
            for (int k = 0; k < 3; ++k) {
                const float v = FromHalf(px[(size_t(y) * ow + x) * 4 + k]);
                if (!std::isfinite(v)) {
                    ++bad;
                    continue;
                }
                sum[k] += v;
                ideal_sum[k] += c[k];
                diff += std::abs(v - c[k]);
            }
        }
    }
    const double n = double(ow) * oh;
    double mean_err = 0.0;
    for (int k = 0; k < 3; ++k) {
        mean_err = std::max(mean_err, std::abs(sum[k] - ideal_sum[k]) / n);
    }
    const double mad = diff / (n * 3);
    const bool pass = bad == 0 && mean_err < 0.02 && mad < 0.04;
    std::printf("%s %ux%u -> %ux%u, preset %d, %s tier: mean RGB %.3f %.3f %.3f (ideal %.3f %.3f "
                "%.3f), mean |diff| %.4f, non-finite %zu\n",
                pass ? "PASS" : "FAIL", rw, rh, ow, oh, int(preset), assets.tier, sum[0] / n,
                sum[1] / n, sum[2] / n, ideal_sum[0] / n, ideal_sum[1] / n, ideal_sum[2] / n, mad,
                bad);
    vkDeviceWaitIdle(device);
    ffxFsr4V07DestroyContext(&context, nullptr);
    ffxFsr4VkDestroyContext(reinterpret_cast<FfxFsr4VkContext*>(scratch.data()));
    return pass ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <assets dir> [rw rh ow oh]...\n", argv[0]);
        return 2;
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    CHECK_VK(vkCreateInstance(&ici, nullptr, &instance));
    uint32_t count = 1;
    VkPhysicalDevice devices[8];
    count = 8;
    CHECK_VK(vkEnumeratePhysicalDevices(instance, &count, devices));
    physical = devices[0];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical, &props);
    std::printf("device: %s\n", props.deviceName);
    uint32_t qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &qcount, qprops.data());
    for (uint32_t i = 0; i < qcount; ++i) {
        if (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queue_family = i;
            break;
        }
    }
    // Every feature the device has (the provider asks for what it needs).
    VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR deriv{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR};
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.pNext = &deriv;
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.pNext = &f13;
    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    f11.pNext = &f12;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &f11;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    const char* extensions[] = {VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME};
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &features;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = extensions;
    CHECK_VK(vkCreateDevice(physical, &dci, nullptr, &device));
    vkGetDeviceQueue(device, queue_family, 0, &queue);
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = queue_family;
    CHECK_VK(vkCreateCommandPool(device, &pci, nullptr, &pool));
    int failures = 0;
    if (argc == 2) {
        const uint32_t cases[][4] = {{1920, 1080, 1920, 1080}, {1280, 720, 1920, 1080},
                                     {1592, 896, 1920, 1080},  {1112, 624, 1920, 1080}, {968, 544, 1920, 1080},
                                     {640, 360, 1920, 1080},   {1920, 1080, 2560, 1440}};
        for (const auto& c : cases) {
            failures += RunCase(argv[1], c[0], c[1], c[2], c[3]);
        }
    } else {
        for (int i = 2; i + 3 < argc; i += 4) {
            failures += RunCase(argv[1], uint32_t(std::atoi(argv[i])), uint32_t(std::atoi(argv[i + 1])),
                                uint32_t(std::atoi(argv[i + 2])), uint32_t(std::atoi(argv[i + 3])));
        }
    }
    return failures == 0 ? 0 : 1;
}
