# FSR 4 v07 INT8 models: Native AA and the upscaling presets

The asset sets the game's FSR 4 uses (Native AA at the 1080 and 2160 tiers; Quality,
Balanced, Performance and Ultra Performance at the 1080 and 2160 tiers for upscaling a lower
render size to the window) (ISSN_FSR4=1, gpu/shadps4/video_core/renderer_vulkan/issn_fsr4.cpp):
SPIR-V passes, model initializers and pre-pass weights of AMD's FidelityFX FSR 4 (MIT, see
LICENSE-FSR4-v07.txt), as built by FireBurn/Q2RTX (commit ae8d628, baseq2/fsr4_shaders; the same
files tools/fetch_fsr4_assets.sh downloads). opt/ holds the post pass through shared memory and pass 11
without its out-of-bounds writes (tools/fsr4_optimize.sh), which the upscaler prefers.

packaging/windows/package.sh ships this folder as fsr4_shaders/. No game data is in it.

The upscaling presets were added on 2026-10-23 from the same commit; every file was checked
against the sha256 in its preset's shader manifest. opt/ was rebuilt with SPIRV-Cross main
(the Debian 2023 package cannot decompile these passes) and reproduces the native set's
previous opt/ files byte for byte.
