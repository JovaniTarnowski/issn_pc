"""link_modules.module on a synthetic SELF: reads the file once and reports its hash."""
import hashlib
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (sys.path for scripts/)
import link_modules
from test_link_libc import self_fixture


class ModuleTests(unittest.TestCase):
    def test_module_reads_the_file_once_and_reports_its_hash(self):
        data = self_fixture()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'libc.prx'
            path.write_bytes(data)
            info = link_modules.module(path)
        self.assertEqual(info['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(info['missing'], [])
        self.assertEqual(info['symbols'][0]['identity'], ('bzQExy189ZI', ('libc', 1), ('libc', 1)))


if __name__ == '__main__':
    unittest.main()


class FsLoadTests(unittest.TestCase):
    """issn: `mov r64, fs:[0]` into any register becomes gs:[0] (older SDKs use ten registers)."""

    def test_every_register_form_is_rewritten(self):
        forms = [bytes([0x64, rex, 0x8b, 0x04 | reg << 3, 0x25, 0, 0, 0, 0])
                 for rex in (0x48, 0x4c) for reg in range(8)]
        code = bytearray(b'\x90'.join(forms) + b'\xc3')
        ph = [dict(type=1, flags=5, vaddr=0, filesz=len(code))]
        self.assertEqual(link_modules.patch_fs_loads(code, ph, 0), 16)
        self.assertEqual(code.count(b'\x64'), 0)
        self.assertEqual(code.count(b'\x65'), 16)

    def test_other_fs_accesses_and_data_segments_are_left_alone(self):
        displaced = bytes.fromhex('64488b042510000000')  # mov rax, fs:[0x10]
        loaded = bytes.fromhex('64488b0c2500000000')    # mov rcx, fs:[0]
        code = bytearray(displaced + loaded)
        ph = [dict(type=1, flags=6, vaddr=0, filesz=len(code))]  # not executable
        self.assertEqual(link_modules.patch_fs_loads(code, ph, 0), 0)
        ph[0]['flags'] = 5
        self.assertEqual(link_modules.patch_fs_loads(code, ph, 0), 1)
        self.assertEqual(bytes(code), displaced + b'\x65' + loaded[1:])
