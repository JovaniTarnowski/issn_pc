#!/usr/bin/env bash
# tools/fsr4_upscale_check.sh [rw rh ow oh]...: builds tests/fsr4_upscale_test.cpp against the
# FSR-Vulkan provider of the last build (out/gpu) and runs every FSR 4 preset the game can pick
# (Native AA, Quality, Balanced, Performance, Ultra Performance; 1080 and 2160 tiers) on a test
# picture. Without a GPU that has INT8 dot products, Mesa's lavapipe runs it (apt install
# mesa-vulkan-drivers; about 3 minutes per case on 8 threads).
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
F=gpu/third_party/fsr-vulkan
clang++ -std=c++20 -O1 -o out/fsr4-upscale-test tests/fsr4_upscale_test.cpp -I"$F/include" \
    -I"$F/upstream/ffx-2.3.0/Kits/FidelityFX/api/include" \
    -I"$F/upstream/ffx-2.3.0/Kits/FidelityFX/api/internal" \
    $(ls -d "$F"/upstream/ffx-2.3.0/Kits/FidelityFX/upscalers/*/include | sed 's/^/-I/') \
    out/gpu/third_party/fsr-vulkan/libffx_vulkan_fsr4_v07_vulkan.a \
    out/gpu/third_party/fsr-vulkan/libffx_vulkan_fsr4_v07_assets.a -lvulkan
if [[ -z ${VK_ICD_FILENAMES:-} ]] && ls /usr/share/vulkan/icd.d/lvp_icd*.json >/dev/null 2>&1; then
    VK_ICD_FILENAMES=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
    export VK_ICD_FILENAMES
fi
out/fsr4-upscale-test third_party/fsr4 "$@"
