# SPDX-License-Identifier: GPL-2.0-or-later
"""Settings that a game needs, chosen by the title ID of its param.sfo.

run.py adds them to the environment of the game unless the variable is already set (an explicit
GOW3_... value always wins), and GOW3_GAME_PROFILE=0 leaves every game on the defaults.
"""
from pathlib import Path

# inFAMOUS Second Son (US, EU). From the settings measured by the shadPS4 build made for this game
# (AYOUB1080p, github.com/weshayoub92/shadPS4-perf): compute workgroups wider than a host subgroup
# need barriers where the game relies on GCN waves of 64 running in lockstep.
INFAMOUS_SECOND_SON = ('CUSA00004', 'CUSA00223')

PROFILES = {
    title: {
        'GOW3_WAVE_MEMORY_BARRIERS': '1',
        # A full memory barrier around each render pass and dispatch: without it 64x64 tiles of
        # the image go black or stale (the game reads compute output the port's barriers miss).
        # Tested on the tester's RX 9060 XT; the exact missing barrier is still to find.
        'GOW3_FULL_BARRIERS': '1',
        # Diagnostic, printed only by the watchdog (GOW3_TIMEOUT): the render thread's fence
        # labels (pointer to the table) and the frame fence it waits on (target, label index)
        # in the 01.00 EU eboot (EP9000); the GPU writes those labels with end-of-pipe events.
        'GOW3_WATCHDOG_PEEK': '0x11c0380,0x4ec7310',
    }
    for title in INFAMOUS_SECOND_SON
}


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
    if env.get('GOW3_GAME_PROFILE') == '0' or title not in PROFILES:
        return title, {}
    return title, {key: value for key, value in PROFILES[title].items() if key not in env}
