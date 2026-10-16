// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: inFAMOUS draw distance and clip planes (src/view_hook.h). The probe verifies the eboot,
// installs the two code hooks and calls gow3_view_attach; a thread then keeps the game's data
// (LOD table, band distances, default clip planes) equal to the menu's values, so changes are
// live and survive the game rewriting them (debug LOD modes, script resets).
#include "gow3_settings.h"
#include "gow3_view.h"

#include "../../src/view_hook.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

unsigned char* image = nullptr;
// Read by the hook stubs: {LOD_ADJUST weight, 1.0f} and 1 / FADE_DISTANCE scale.
alignas(16) float region_params[2] = {1.0f, 1.0f};
alignas(16) float fade_inverse = 1.0f;

void Put(uint32_t at, float value) {
    float current;
    std::memcpy(&current, image + at, 4);
    if (current != value) {
        std::memcpy(image + at, &value, 4);
    }
}

void Apply() {
    const auto& s = Gow3Settings::Get();
    const float scale = Gow3View::ClampLodScale(s.lod_scale);
    for (unsigned r = 0; r < GOW3_VIEW_LOD_ROWS; ++r) {
        Put(GOW3_VIEW_LOD_TABLE + r * GOW3_VIEW_LOD_STRIDE, gow3_view_lod_original[r] * scale);
    }
    for (unsigned b = 0; b < GOW3_VIEW_BAND_COUNT; ++b) {
        Put(GOW3_VIEW_BANDS + 4 * b, Gow3View::ClampBand(s.lod_bands[b]));
    }
    Put(GOW3_VIEW_NEAR_CLIP, Gow3View::ClampNearClip(s.near_clip));
    Put(GOW3_VIEW_FAR_CLIP, Gow3View::ClampFarClip(s.far_clip));
    region_params[0] = Gow3View::ClampRegionLod(s.region_lod);
    fade_inverse = 1.0f / Gow3View::ClampRegionFade(s.region_fade);
}

} // namespace

extern "C" uintptr_t gow3_view_region_params(void) {
    return reinterpret_cast<uintptr_t>(region_params);
}

extern "C" uintptr_t gow3_view_fade_inverse(void) {
    return reinterpret_cast<uintptr_t>(&fade_inverse);
}

extern "C" void gow3_view_attach(unsigned char* guest_image) {
    image = guest_image;
    Apply(); // before the game's first frame
    const auto& s = Gow3Settings::Get();
    std::printf("Draw distance: LOD scale %.2f, bands %.0f/%.0f/%.0f/%.0f/%.0f/%.0f, region LOD_ADJUST weight %.2f, "
                "FADE_DISTANCE x%.2f, near %.1f, far %.0f, texture LOD bias %+.2f\n",
                s.lod_scale.load(), s.lod_bands[0].load(), s.lod_bands[1].load(), s.lod_bands[2].load(),
                s.lod_bands[3].load(), s.lod_bands[4].load(), s.lod_bands[5].load(), s.region_lod.load(),
                s.region_fade.load(), s.near_clip.load(), s.far_clip.load(), s.mip_lod_bias.load());
    Gow3Settings::Get().view_supported = true;
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            Apply();
        }
    }).detach();
}
