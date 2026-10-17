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
