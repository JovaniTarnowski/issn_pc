#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Makes a shader seed for the package from a verified pipeline cache folder.

A seed is what axylpup's build ships as its "shader cache included": the pipeline keys and shader
metas (register state, resource bindings, permutation keys - numbers) of a played-through cache,
without any shader code. On a player's first start the core (SHADPS4_CACHE_REBUILD=1) makes every
listed stage again from the player's own game files and compiles the pipelines at boot, so the
game does not stop for them while playing.

The cache folder must hold a seed-stamp.bin written by a VERIFY run (SHADPS4_CACHE_REBUILD=2) with
the executable that ships: the stamp names the emulator build, the game code and the GPU profile it
was verified on, and per shader the game file's crc32. Stages whose rebuild differed from the
cached SPIR-V (rebuild-differ.txt) are left out; they compile at play time as before.

Usage: make_shader_seed.py <cache folder> <seed folder>
"""
import shutil
import struct
import sys
from pathlib import Path

STAMP_MAGIC = 0x50545353  # "SSTP", vk_pipeline_serialization.cpp
STAMP_VERSION = 3
# Everything a seed may hold: keys, metas and the core's seed sidecars. Never SPIR-V (*.spv, a
# translation of the game's code), the machine's profile.bin or shader-index.bin (paths of the
# dump the cache was made from).
SIDECARS = ('seed-stamp.bin', 'vs-formats.bin')


def read_string(data, at):
    (length,) = struct.unpack_from('<I', data, at)
    at += 4
    return data[at:at + length].decode('utf-8', 'replace'), at + length


def read_stamp(path):
    try:
        return parse_stamp(path.read_bytes())
    except struct.error:
        raise ValueError(f'{path} is cut short') from None


def parse_stamp(data):
    magic, version = struct.unpack_from('<II', data, 0)
    if magic != STAMP_MAGIC or version != STAMP_VERSION:
        raise ValueError(f'not a version {STAMP_VERSION} seed stamp')
    at = 8
    serial, at = read_string(data, at)
    app_ver, at = read_string(data, at)
    build, at = read_string(data, at)
    (code_hash,) = struct.unpack_from('<Q', data, at)
    at += 8
    (profile_size,) = struct.unpack_from('<I', data, at)
    at += 4 + profile_size + 1
    (count,) = struct.unpack_from('<I', data, at)
    return {'serial': serial, 'app_ver': app_ver, 'build': build, 'code_hash': code_hash, 'shaders': count}


def differing_metas(cache):
    listed = cache / 'rebuild-differ.txt'
    if not listed.is_file():
        return set()
    names = set()
    for line in listed.read_text(errors='replace').splitlines():
        if line.startswith('0x'):
            names.add(line.split()[0] + '.meta')
    return names


def make_seed(cache, seed):
    cache, seed = Path(cache), Path(seed)
    stamp = read_stamp(cache / 'seed-stamp.bin')
    if stamp['shaders'] == 0:
        raise ValueError('the seed stamp lists no verified shader')
    drop = differing_metas(cache)
    if seed.exists():
        shutil.rmtree(seed)
    seed.mkdir(parents=True)
    keys = metas = dropped = 0
    graphics = []
    for entry in sorted(cache.iterdir()):
        name = entry.name
        if not entry.is_file():
            continue
        if name.endswith('.key') and (name.startswith('g_0x') or name.startswith('c_0x')):
            keys += 1
            if name.startswith('g_0x'):
                graphics.append(name[:-4])
        elif name.endswith('.meta') and name.startswith('0x'):
            if name in drop:
                dropped += 1
                continue
            metas += 1
        elif name not in SIDECARS:
            continue
        shutil.copyfile(entry, seed / name)
    (seed / 'seed-graphics-keys.txt').write_text(''.join(f'{name}\n' for name in graphics))
    return stamp, keys, metas, dropped


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    try:
        stamp, keys, metas, dropped = make_seed(argv[1], argv[2])
    except (OSError, ValueError, struct.error) as error:
        print(f'make_shader_seed: {error}', file=sys.stderr)
        return 1
    print(f"Shader seed: {stamp['serial']} {stamp['app_ver']}, game code {stamp['code_hash']:#018x}, "
          f"{stamp['shaders']} verified shaders; {keys} pipeline keys, {metas} metas "
          f"({dropped} differing left out)")
    print(f"Verified with: {stamp['build']}")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
