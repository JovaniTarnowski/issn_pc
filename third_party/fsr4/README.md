# FSR 4 v07 INT8 model, Native AA, 1080 tier

The asset set the game's FSR 4 Native AA uses (ISSN_FSR4=1, gpu/shadps4/video_core/renderer_vulkan/issn_fsr4.cpp):
SPIR-V passes, model initializers and pre-pass weights of AMD's FidelityFX FSR 4 (MIT, see
LICENSE-FSR4-v07.txt), as built by FireBurn/Q2RTX (commit ae8d628, baseq2/fsr4_shaders; the same
files tools/fetch_fsr4_assets.sh downloads). opt/ holds the post pass through shared memory and pass 11
without its out-of-bounds writes (tools/fsr4_optimize.sh), which the upscaler prefers.

packaging/windows/package.sh ships this folder as fsr4_shaders/. No game data is in it.
