# SPDX-License-Identifier: GPL-2.0-or-later
"""Settings that a game needs, chosen by the title ID of its param.sfo.

run.py adds them to the environment of the game unless the variable is already set (an explicit
ISSN_... value always wins), and ISSN_GAME_PROFILE=0 leaves every game on the defaults.
"""
from pathlib import Path

# inFAMOUS Second Son (US, EU). From the settings measured by the shadPS4 build made for this game
# (AYOUB1080p, github.com/weshayoub92/shadPS4-perf): compute workgroups wider than a host subgroup
# need barriers where the game relies on GCN waves of 64 running in lockstep.
INFAMOUS_SECOND_SON = ('CUSA00004', 'CUSA00223')

# issn-axy: the GPU core of axylpup's inFAMOUS Second Son build (github.com/axylpup/shadPS4,
# second-son 2212a345 = release v1.0.2), with the switches of its release (shadps4-flags.txt).
# Left out: what lives outside the vendored core (DLSS, mouse aim, button-icon textures, the pad,
# save dialog, video decoder and kernel mutex parts, its shader-cache name, its boot screen).
AXY_RELEASE_FLAGS = {
    'SHADPS4_A2': '1',
    'SHADPS4_A2_INDIRECT': '1',
    'SHADPS4_A2_MAT_MEMO': '1',
    'SHADPS4_A2_PARK': '1',
    'SHADPS4_A2_VBUF2': '1',
    'SHADPS4_A2_WORKERS': '6',
    'SHADPS4_ARENA_READ_PREFETCH': '1',
    'SHADPS4_ASC_EARLY': '1',
    'SHADPS4_ASC_YIELD': '1',
    'SHADPS4_ASYNC_COMPUTE_PIPELINES': '1',
    'SHADPS4_ASYNC_COMPUTE_THREADS': '12',
    'SHADPS4_ASYNC_RECORDING': '1',
    'SHADPS4_ASYNC_RECORDING_TIMING': '0',
    'SHADPS4_ASYNC_SUBMIT': '1',
    'SHADPS4_ASYNC_UPLOAD_RECORD': '1',
    'SHADPS4_BACK_CLEAN': '1',
    'SHADPS4_BIG_GROW': '1',
    'SHADPS4_BUFFER_RANGES_LOCK': '2',
    'SHADPS4_BUFFER_SAMPLE': '0',
    'SHADPS4_CACHE_PREFETCH': '3',
    'SHADPS4_CACHE_READBACKS_MODE': '1',
    'SHADPS4_CACHE_REBUILD': '1',
    'SHADPS4_CLAIM_WAKE': '1',
    'SHADPS4_CP_QOS': '3',
    'SHADPS4_CP_SPLIT': '2',
    'SHADPS4_CP_SPLIT_DEPTH': '1024',
    'SHADPS4_CP_SPLIT_DISP_GUARD': '0',
    'SHADPS4_CP_SPLIT_DONE_BATCH': '16',
    'SHADPS4_CP_SPLIT_FRONT_DERIVE': '1',
    'SHADPS4_CP_SPLIT_POST_ASC': '1',
    'SHADPS4_CP_SPLIT_POST_DMA': '1',
    'SHADPS4_CP_SPLIT_POST_FENCE': '1',
    'SHADPS4_CP_SPLIT_POST_FLIP': '1',
    'SHADPS4_CP_SPLIT_POST_ORDER': '1',
    'SHADPS4_CP_SPLIT_POST_TASK_DONE': '1',
    'SHADPS4_CP_SPLIT_POST_WDATA': '1',
    'SHADPS4_CP_SPLIT_REG_BATCH': '1',
    'SHADPS4_CP_SPLIT_RPTR_BACKING': '1',
    'SHADPS4_CP_SPLIT_TASK_GUARD': '3',
    'SHADPS4_CP_SPLIT_WAIT_BARRIER': '1',
    'SHADPS4_DECODE_IMAGE_VERIFY': '0',
    'SHADPS4_DEFERRED_PROTECT': '1',
    'SHADPS4_DEFER_READ_PROTECT': '1',
    'SHADPS4_DERIVE_INDIRECT': '1',
    'SHADPS4_DESCRIPTOR_TEMPLATES': '0',
    'SHADPS4_DESCRIPTOR_TEMPLATES_VERIFY': '0',
    'SHADPS4_DRAW_PACKET': '1',
    'SHADPS4_DRAW_PACKET_DELTA': '1',
    'SHADPS4_DRAW_REPEAT': '1',
    'SHADPS4_DRAW_SAMPLE': '0',
    'SHADPS4_DRAW_SPLIT': '0',
    'SHADPS4_EPOCH_DEFER_UPLOAD': '0',
    'SHADPS4_EXPOSURE_STALE_READ': '1',
    'SHADPS4_FAST_LANE_PRODUCER': '1',
    'SHADPS4_FAST_TEX_MUTEX': '1',
    'SHADPS4_FAULT_GUEST_COPY': '1',
    'SHADPS4_FAULT_OVERLAP_RETRY': '1',
    'SHADPS4_FAULT_VETO_EXACT': '1',
    'SHADPS4_FAULT_WRITE_SKIP_SETTLE': '1',
    'SHADPS4_FENCE_FORWARD': '1',
    'SHADPS4_FETCH_RESULTS': '0',
    'SHADPS4_FETCH_RESULTS_VERIFY': '0',
    'SHADPS4_FLUSH_EVERY_DRAWS': '1000',
    'SHADPS4_FLUSH_EVERY_OPS': '64',
    'SHADPS4_GPU_MOD_RELAXED': '1',
    'SHADPS4_GPU_RANGES_FLAT': '1',
    'SHADPS4_GPU_TIMESTAMPS': '0',
    'SHADPS4_GUEST_DISPATCH_ORDER': '1',
    'SHADPS4_HAIR_STENCIL_TRACE': '0',
    'SHADPS4_HEAP_WRITE_BLOCK': '65536',
    'SHADPS4_HOT_PAGE_PREFETCH': '1',
    'SHADPS4_HOT_PREFETCH_AT_WRITE': '1',
    'SHADPS4_IB_TASK_FREE': '1',
    'SHADPS4_LEAN': '1',
    'SHADPS4_LEAN_BIND': '1',
    'SHADPS4_LEAN_PARSE': '2',
    'SHADPS4_LEAN_PERDRAW': '1',
    'SHADPS4_LEAN_SPLIT_ANY': '128',
    'SHADPS4_LEAN_WORKERS': '8',
    'SHADPS4_NATIVE_BINDLESS': '1',
    'SHADPS4_NATIVE_BINDLESS_VS': '1',
    'SHADPS4_NATIVE_COHERENT': '1',
    'SHADPS4_NATIVE_COLOUR': '1',
    'SHADPS4_NATIVE_COLOUR_UNTEXTURED': '1',
    'SHADPS4_NATIVE_DIRECT': '1',
    'SHADPS4_NATIVE_GPU_DIRECT': '1',
    'SHADPS4_NATIVE_PASS': '1',
    'SHADPS4_NATIVE_PT': '1',
    'SHADPS4_NATIVE_REC': '0',
    'SHADPS4_NATIVE_SPARSE_LAYOUT': '1',
    'SHADPS4_NR_ITYPE_LATE': '1',
    'SHADPS4_NR_SHARDED': '2',
    'SHADPS4_NR_WINDOW': '16384',
    'SHADPS4_NR_WORKER': '1',
    'SHADPS4_OCCLUSION_QUERIES': '1',
    'SHADPS4_PAGE_OS_LOCK': '1',
    'SHADPS4_PARALLEL_WARMUP': '1',
    'SHADPS4_PARTICLE_ARENA_BLOCK': '1',
    'SHADPS4_PARTICLE_ARENA_DISCARD': '1',
    'SHADPS4_PARTICLE_LATE_READ': '1',
    'SHADPS4_PAR_REC': '2',
    'SHADPS4_PENDING_OPS_LOCK': '1',
    'SHADPS4_PGM_PREFETCH': '1',
    'SHADPS4_POOL_AUTO': '1',
    'SHADPS4_POP_FASTPATH': '1',
    'SHADPS4_PRECOMPILE_WAIT': '1',
    'SHADPS4_PREPARED_IMAGES': '1',
    'SHADPS4_PREPARED_IMAGES_VERIFY': '0',
    'SHADPS4_PREPARED_LOCAL': '1',
    'SHADPS4_PREPARED_PIPELINES': '1',
    'SHADPS4_PREPARED_PIPELINES_VERIFY': '0',
    'SHADPS4_PREPARED_PROGRAMS': '1',
    'SHADPS4_PREPARED_PROGRAMS_TWO_ENTRY': '0',
    'SHADPS4_PREPARED_PROGRAMS_VERIFY': '0',
    'SHADPS4_PREPARED_TARGETS': '1',
    'SHADPS4_PREPARED_TARGETS_VERIFY': '0',
    'SHADPS4_PRESENT_ASYNC': '1',
    'SHADPS4_PROTECT_LOCK_PROFILE': '0',
    'SHADPS4_PROTECT_STEAL': '2',
    'SHADPS4_READBACK_FAST_LANE': '1',
    'SHADPS4_READBACK_OFFLOAD': '1',
    'SHADPS4_READBACK_OFFLOAD_PROFILE': '0',
    'SHADPS4_READBACK_OFFLOAD_VERIFY': '0',
    'SHADPS4_READBACK_PREBATCH': '2',
    'SHADPS4_READBACK_PREFETCH': '0',
    'SHADPS4_READBACK_PREFETCH_VERIFY': '0',
    'SHADPS4_READBACK_QUEUE': '3',
    'SHADPS4_READ_BIND_MEMO': '3',
    'SHADPS4_READ_FAULT_WIDEN_SIZE': '1048576',
    'SHADPS4_READ_MEMO_PREFETCH': '1',
    'SHADPS4_READ_SYNC_FAST': '1',
    'SHADPS4_READ_UNTRACK_FENCE': '1',
    'SHADPS4_REC_CHUNKS': '64',
    'SHADPS4_RENDER_STATE_MEMO': '1',
    'SHADPS4_RESOLVED_RO_BUFFERS': '0',
    'SHADPS4_RESOLVED_RO_BUFFERS_VERIFY': '0',
    'SHADPS4_SHARED_FETCH_VERIFY': '0',
    'SHADPS4_SHARED_PROTECT': '1',
    'SHADPS4_SKIP_EMPTY_DYNSTATE': '1',
    'SHADPS4_SKIP_EMPTY_UPLOADS': '0',
    'SHADPS4_SKIP_EMPTY_UPLOADS_VERIFY': '0',
    'SHADPS4_SMART_BUFFER_BARRIERS': '1',
    'SHADPS4_SMOKE_SORT_SYNC': '1',
    'SHADPS4_SPINLOCK_PARK': '1',
    'SHADPS4_STENCIL_REPLACE_OP': '1',
    'SHADPS4_STREAM_MAX_SIZE': '0',
    'SHADPS4_SUBMIT_LOCK_SKIP': '1',
    'SHADPS4_SWAPCHAIN_SAFE_WAIT': '1',
    'SHADPS4_TEXTURE_STAGE_BATCH': '0',
    'SHADPS4_TEXTURE_STAGE_BATCH_PROFILE': '0',
    'SHADPS4_TEX_BIND_MEMO': '1',
    'SHADPS4_TEX_INVALIDATE_PRECHECK': '1',
    'SHADPS4_TEX_LOCKFREE_HIT': '1',
    'SHADPS4_TEX_MEMO_HASH': '1',
    'SHADPS4_TEX_MEMO_LOCAL_GEN': '1',
    'SHADPS4_TEX_MEMO_WAYS': '32',
    'SHADPS4_TEX_TABLE': '3',
    'SHADPS4_TEX_TABLE_V2': '1',
    'SHADPS4_TEX_TRANSIT_SKIP': '1',
    'SHADPS4_TEX_UNTRACK_OUTSIDE': '1',
    'SHADPS4_TRANSFER_COALESCE': '1',
    'SHADPS4_UPLOAD_AHEAD': '64',
    'SHADPS4_VIEWPORT_MEMO': '1',
    'SHADPS4_WATCH_MAPCHECK': '0',
    'SHADPS4_WATER_REFLECTION_SCALE': '4',
    'SHADPS4_WRITE_FAULT_OFFLOAD': '1',
    'SHADPS4_WRITE_FAULT_PAGES': '0',
    'SHADPS4_WRITE_FAULT_WIDEN': '2',
    'SHADPS4_WRM_PARTIAL': '1',
}

PROFILES = {
    title: {
        # Precise GPU readbacks: the game reads GPU particle results back (the smoke to drain,
        # drones); without them those effects are missing. The deferred and eager copies keep
        # the waits short (HayatoG's iss_pc defaults; his eager read-ahead made smoke flicker).
        'ISSN_READBACKS': '2',
        # Buffer-store barriers in uniform loops of workgroups wider than a GCN wave (AYOUB1080p).
        'ISSN_WAVE_MEMORY_BARRIERS': '1',
        'ISSN_DEFERRED_READBACK': '1',
        # The game's memcpy/memset into GPU-modified memory without a fault or GPU wait (iss_pc).
        'ISSN_WRITE_MERGE': '1',
        # Eager read-ahead: the GPU results the game reads back are copied at each fence. Halved
        # the guest's readback waits in a four-enemy fight; flicker only on the boot loading
        # screen on the tester's RX 9060 XT (HayatoG turned it off for smoke flicker).
        'ISSN_EAGER_READBACK': '1',
        # The compute queues the main thread waits on (~30 fences a frame): register writes and
        # dispatches no longer drain the draw pipe, and the graphics stream steps aside every 8
        # packets while compute work is queued (ISSN_LATENCY: 4.7 ms per fence before). Combat
        # median 29 -> 33 FPS (every 32 packets), 39 (every 8) on the tester's RX 9060 XT, no
        # effect lost.
        'ISSN_COMPUTE_PIPELINE': '1',
        'ISSN_COMPUTE_PRIORITY': '1',
        # Saving pipeline binaries (VK_KHR_pipeline_binary) for pipelines compiled into an empty
        # shader cache crashed AMD's Windows driver (amdvlk64.dll) at the first boot after the
        # cache was rebuilt (tester, RX 9060 XT); loading saved ones worked. Off until found.
        'ISSN_PIPELINE_BINARIES': '0',
        # Smaller batches reach the GPU sooner: what the game reads back waits less for it.
        # Combat (tester, RX 9060 XT): 1024 (default) median 30.6 / mean 29.1 FPS, 256 31.8 /
        # 32.4; compute fence 3.8 -> 3.1 ms, graphics fence 16.4 -> 12.3 ms.
        'ISSN_SUBMIT_DRAWS': '256',
        # The GPU command thread no longer frees the draw-preparation submissions (~18% of it) nor
        # takes the memory table lock for each constant copy (~7%): standing in the city its idle
        # time went 12% -> 22%, 36 -> 38.9 FPS and steadier (tester, RX 9060 XT).
        'ISSN_ASYNC_FREE': '1',
        'ISSN_LOCKFREE_READ': '1',
        # Draw-preparation submissions reused instead of allocated and zeroed on the game's main
        # thread each frame: standing in the city 37.0 -> 38.4 FPS (41 at times).
        'ISSN_RECYCLE_PREP': '1',
        # Draw distance (HayatoG's recommended setup, 1.07 only: src/view_hook.h): a shorter LOD
        # distance means far fewer draws in the open city, the bottleneck here.
        'ISSN_LOD_SCALE': '0.6',
        'ISSN_REGION_FADE': '0.8',
        # AMD: maximal reconvergence is suspected of keeping the game from starting (RX 7600).
        'ISSN_BP06': '0',
        # ISSN_FULL_BARRIERS=1 is no longer needed: the black 64x64 tiles stay away without it
        # since the vertex/index/indirect buffer barriers (BP10, HayatoG's core); off, the FPS
        # roughly doubles in the city (tester, RX 9060 XT).
        # The game reads rdtsc itself and scales it by sceKernelGetTscFrequency: the host's TSC
        # and frequency, or it runs several times too fast (runtime_kernel.c).
        'ISSN_TSC': 'host',
        # Diagnostic, printed only by the watchdog (ISSN_TIMEOUT): the render thread's fence
        # labels (pointer to the table) and the frame fence it waits on (target, label index)
        # in the 01.00 EU eboot (EP9000); the GPU writes those labels with end-of-pipe events.
    }
    for title in INFAMOUS_SECOND_SON
}
for _title in INFAMOUS_SECOND_SON:
    PROFILES[_title].update(AXY_RELEASE_FLAGS)
    # axylpup's per-game config (custom_configs/CUSA00004.json): direct memory access on.
    PROFILES[_title]['ISSN_DIRECT_MEMORY_ACCESS'] = '1'
PROFILES['CUSA00004']['ISSN_WATCHDOG_PEEK'] = '0x11c0380,0x4ec7310'
# The US release has the Brazilian Portuguese dub and text (lang_brazilian_*): the tester's
# choice. ISSN_LANGUAGE=1 gives English.
PROFILES['CUSA00223']['ISSN_LANGUAGE'] = '17'


def title_id(game):
    """TITLE_ID of the game folder's param.sfo, or None."""
    try:
        from prepare import read_sfo
    except ImportError:
        return None
    sfo = read_sfo(Path(game) / 'sce_sys' / 'param.sfo') or {}
    value = sfo.get('TITLE_ID')
    return str(value) if value else None


def environment(game, env):
    """(title ID, {variable: value}) to add for this game: only variables env does not set."""
    title = title_id(game)
    if env.get('ISSN_GAME_PROFILE') == '0' or title not in PROFILES:
        return title, {}
    return title, {key: value for key, value in PROFILES[title].items() if key not in env}
