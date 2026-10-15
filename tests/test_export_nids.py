"""src/runtime_export_nids.inc lists the NID of every name the runtime exports."""
import subprocess
import sys
import unittest

from paths import ROOT


class ExportNidsTest(unittest.TestCase):
    def test_generated_file_is_up_to_date(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools' / 'issn' / 'gen_export_nids.py'), '--check'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_known_nids(self):
        text = (ROOT / 'src' / 'runtime_export_nids.inc').read_text(encoding='utf-8')
        self.assertIn('{"u1GRHp+oWoY#libScePad","scePadGetHandle"},', text)
        self.assertIn('{"BpFoboUJoZU#libkernel","sceKernelCreateEventFlag"},', text)
        # A name in one library is not another library's function of the same name.
        self.assertNotIn('{"n88vx3C5nW8#libc","gettimeofday"},', text)


if __name__ == '__main__':
    unittest.main()
