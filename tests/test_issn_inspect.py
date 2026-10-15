"""tools/issn/inspect_dump.py em um dump sintético (sem arquivos do jogo)."""
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

TOOL = ROOT / 'tools' / 'issn' / 'inspect_dump.py'
ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'


def elf_fixture(imports=(), exports=()):
    """Um ELF x86-64 de PS4 mínimo. imports/exports: (nid, biblioteca). Cada biblioteca vira
    um módulo de mesmo nome, como no PS4."""
    libraries = sorted({lib for _, lib in (*imports, *exports)})
    ids = {lib: i + 1 for i, lib in enumerate(libraries)}
    strings = bytearray(b'\0')
    offsets = {}

    def add(text):
        if text not in offsets:
            offsets[text] = len(strings)
            strings.extend(text.encode() + b'\0')
        return offsets[text]
    for lib in libraries:
        add(lib)
    symbols = bytearray()
    for (nid, lib), section in [(i, 0) for i in imports] + [(e, 1) for e in exports]:
        code = ALPHABET[ids[lib]]
        symbols += struct.pack('<IBBHQQ', add(f'{nid}#{code}#{code}'), 0x12, 0, section, 0 if not section else 0, 1)
    blob = bytes(strings) + bytes(-len(strings) % 8) + bytes(symbols)
    sym_off = len(strings) + (-len(strings) % 8)
    tags = [(0x61000035, 0), (0x61000037, len(strings)), (0x61000039, sym_off), (0x6100003f, len(symbols)),
            (0x61000029, 0), (0x6100002d, 0), (0x6100002f, 0), (0x61000031, 0)]
    for lib in libraries:
        value = (ids[lib] << 48) | (1 << 32) | offsets[lib]
        tags += [(0x61000015, value), (0x6100000f, value)]
    tags.append((0, 0))
    dynamic = b''.join(struct.pack('<QQ', t, v) for t, v in tags)
    code_off, blob_off = 0x200, 0x400
    dyn_off = blob_off + len(blob) + (-len(blob) % 16)
    data = bytearray(dyn_off + len(dynamic))
    struct.pack_into('<16sHHIQQQIHHHHHH', data, 0, b'\x7fELF\x02\x01\x01' + bytes(9), 0xfe10, 62, 1,
                     0, 64, 0, 0, 64, 56, 3, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, code_off, 0, 0, 16, 16, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, 120, 0x61000000, 4, blob_off, 0, 0, len(blob), 0, 16)
    struct.pack_into('<IIQQQQQQ', data, 176, 2, 4, dyn_off, 0, 0, len(dynamic), len(dynamic), 8)
    data[code_off] = 0xc3
    data[blob_off:blob_off + len(blob)] = blob
    data[dyn_off:] = dynamic
    return bytes(data)


def sfo_fixture(values):
    keys, entries, payload = bytearray(), [], bytearray()
    for key, value in values.items():
        raw = value.encode() + b'\0'
        entries.append((len(keys), 0x0204, len(raw), len(raw), len(payload)))
        keys += key.encode() + b'\0'
        payload += raw
    keys += bytes(-len(keys) % 4)
    key_table = 20 + 16 * len(entries)
    out = struct.pack('<4sIIII', b'\0PSF', 0x101, key_table, key_table + len(keys), len(entries))
    out += b''.join(struct.pack('<HHIII', *e) for e in entries)
    return out + keys + payload


class InspectTests(unittest.TestCase):
    def make_game(self, root, category='gd', eboot=None):
        game = root / 'CUSA00004'
        (game / 'sce_sys').mkdir(parents=True)
        (game / 'sce_module').mkdir()
        (game / 'sce_sys' / 'param.sfo').write_bytes(sfo_fixture(
            {'TITLE': 'inFAMOUS Second Son', 'TITLE_ID': 'CUSA00004', 'APP_VER': '01.07', 'CATEGORY': category}))
        (game / 'eboot.bin').write_bytes(eboot or elf_fixture(imports=[
            ('bzQExy189ZI', 'libc'),              # no runtime (src/runtime.c)
            ('b0xyllnVY-I', 'libSceGnmDriver'),   # no núcleo do shadPS4
            ('Fios2ExpAAA', 'libSceFios2'),       # exportada por um módulo do dump
            ('NpTrophyXXX', 'libSceNpTrophy'),    # ninguém implementa
        ]))
        (game / 'sce_module' / 'libc.prx').write_bytes(elf_fixture(exports=[('LibcExpAAAA', 'libc')]))
        (game / 'sce_module' / 'libSceFios2.prx').write_bytes(elf_fixture(exports=[('Fios2ExpAAA', 'libSceFios2')]))
        return game

    def run_tool(self, game, out):
        # cp1252: what a pipe gets on a Windows Python (its ANSI code page), not UTF-8.
        env = {**os.environ, 'PYTHONIOENCODING': 'cp1252'}
        result = subprocess.run([sys.executable, str(TOOL), str(game), '--out', str(out)],
                                capture_output=True, text=True, encoding='utf-8', env=env)
        return result, json.loads((out / 'issn_report.json').read_text(encoding='utf-8'))

    def test_classifies_every_import(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result, report = self.run_tool(self.make_game(root), root / 'out')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        sources = {r['nid']: r['source'] for r in report['imports']}
        self.assertEqual(sources, {'bzQExy189ZI': 'runtime', 'b0xyllnVY-I': 'gpu',
                                   'Fios2ExpAAA': 'modulo', 'NpTrophyXXX': 'FALTA'})
        self.assertEqual(report['sfo']['APP_VER'], '01.07')
        self.assertEqual(report['files']['eboot.bin'], 'elf')
        self.assertIn('faltando: 1', result.stdout)

    def test_patch_only_folder_is_a_problem(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result, report = self.run_tool(self.make_game(root, category='gp'), root / 'out')
        self.assertEqual(result.returncode, 1)
        self.assertTrue(any('CATEGORY=gp' in p for p in report['problems']))

    def test_encrypted_eboot_is_named(self):
        encrypted = bytearray(0x100)
        encrypted[:4] = b'O\x15=\x1d'
        struct.pack_into('<H', encrypted, 24, 1)
        struct.pack_into('<QQQQ', encrypted, 32, 0x800 | 2, 0x100, 0, 0)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result, report = self.run_tool(self.make_game(root, eboot=bytes(encrypted)), root / 'out')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report['files']['eboot.bin'], 'self-encrypted')
        self.assertTrue(any('criptografado' in p for p in report['problems']))

    def test_missing_folder_parts(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp) / 'CUSA00004'
            game.mkdir()
            result, report = self.run_tool(game, Path(tmp) / 'out')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(len(report['problems']), 3)


if __name__ == '__main__':
    unittest.main()
