// SPDX-License-Identifier: GPL-2.0-or-later
// issn: the inFAMOUS Second Son shader fixes, on GCN code translated by the recompiler.
//   ninja -C out/gpu issn-shader-fixes-test && out/gpu/issn-shader-fixes-test
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>
#include "shader_recompiler/info.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/runtime_info.h"

// libissngpu reports the runtime counters when its window closes; no runtime here.
extern "C" void runtime_report(void) {}

namespace {
namespace IR = Shader::IR;

int failures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

constexpr u32 V1 = 257, V2 = 258, V3 = 259; // VGPR operands in the 9-bit source field
constexpr u32 SEndPgm = 0xBF810000u;

// VOP2: vdst = op(src0, vsrc1).
constexpr u32 Vop2(u32 op, u32 vdst, u32 src0, u32 vsrc1) {
    return (op << 25) | (vdst << 17) | (vsrc1 << 9) | src0;
}
// VOP3a: vdst = op(src0, src1, src2).
constexpr std::array<u32, 2> Vop3(u32 op, u32 vdst, u32 src0, u32 src1, u32 src2) {
    return {(0x34u << 26) | (op << 17) | vdst, src0 | (src1 << 9) | (src2 << 18)};
}
// ds_write_b32 [vaddr], vdata: keeps the result alive (an LDS store is a side effect).
constexpr std::array<u32, 2> DsWriteB32(u32 vaddr, u32 vdata) {
    return {(0x36u << 26) | (13u << 18), (vdata << 8) | vaddr};
}

struct Counts {
    int fp_equal{};
    int fp_add{};
    int fp_fma{};
    int barriers{};
};

Counts Translate(const std::vector<u32>& code, const Shader::Profile& profile, u32 workgroup = 64) {
    static Shader::Pools pools;
    pools.ReleaseContents();
    static const std::array<u32, Shader::ShaderParams::NumShaderUserData> user_data{};
    const Shader::ShaderParams params{.user_data = user_data, .code = code, .hash = 0x155u};
    Shader::Info info{Shader::HwStage::Compute, Shader::SwStage::Compute, params};
    Shader::RuntimeInfo runtime_info{};
    runtime_info.Initialize(Shader::HwStage::Compute, Shader::SwStage::Compute);
    runtime_info.hw.cs.shared_memory_size = 1024;
    runtime_info.hw.cs.workgroup_size = {workgroup, 1, 1};
    runtime_info.props.num_input_vgprs = 1;
    runtime_info.props.num_allocated_vgprs = 8;
    const auto program = Shader::TranslateProgram(code, pools, info, runtime_info, profile);
    Counts counts;
    for (const IR::Block* block : program.blocks) {
        for (const IR::Inst& inst : block->Instructions()) {
            switch (inst.GetOpcode()) {
            case IR::Opcode::FPOrdEqual32:
                ++counts.fp_equal;
                break;
            case IR::Opcode::FPAdd32:
                ++counts.fp_add;
                break;
            case IR::Opcode::FPFma32:
                ++counts.fp_fma;
                break;
            case IR::Opcode::Barrier:
                ++counts.barriers;
                break;
            default:
                break;
            }
        }
    }
    return counts;
}

std::vector<u32> Program(std::initializer_list<u32> words) {
    return std::vector<u32>(words);
}

} // namespace

int main() {
    Shader::Profile profile{};
    profile.subgroup_size = 64;

    // v_mac_legacy_f32 v0, v1, v2: D = S0 * S1 + D with 0 * x = +0 (two zero tests, an add).
    {
        const auto ds = DsWriteB32(3, 0);
        const auto counts = Translate(Program({Vop2(6, 0, V1, 2), ds[0], ds[1], SEndPgm}), profile);
        Check(counts.fp_equal == 2 && counts.fp_add == 1 && counts.fp_fma == 0,
              "V_MAC_LEGACY_F32 uses the legacy multiply");
    }
    // v_mac_f32 v0, v1, v2 (IEEE): no zero tests.
    {
        const auto ds = DsWriteB32(3, 0);
        const auto counts = Translate(Program({Vop2(31, 0, V1, 2), ds[0], ds[1], SEndPgm}), profile);
        Check(counts.fp_equal == 0, "V_MAC_F32 keeps the IEEE multiply-add");
    }
    // v_mad_legacy_f32 v0, v1, v2, v3.
    {
        const auto mad = Vop3(320, 0, V1, V2, V3);
        const auto ds = DsWriteB32(4, 0);
        const auto counts = Translate(Program({mad[0], mad[1], ds[0], ds[1], SEndPgm}), profile);
        Check(counts.fp_equal == 2 && counts.fp_add == 1 && counts.fp_fma == 0,
              "V_MAD_LEGACY_F32 uses the legacy multiply");
    }
    // The wave barrier pass runs on a workgroup wider than the host subgroup without breaking a
    // shader that has nothing for it to synchronize.
    {
        Shader::Profile wave = profile;
        wave.subgroup_size = 32;
        wave.wave_memory_barriers = true;
        const auto ds = DsWriteB32(3, 0);
        const auto counts =
            Translate(Program({Vop2(6, 0, V1, 2), ds[0], ds[1], SEndPgm}), wave, 256);
        Check(counts.fp_equal == 2 && counts.barriers == 0,
              "WaveMemoryBarrierPass leaves straight-line LDS code alone");
    }
    std::printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
