"""scripts/game_profiles.py: settings chosen by the title ID of the game."""
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (sys.path for scripts/)
import game_profiles
from test_issn_inspect import sfo_fixture


def game(root, title):
    folder = Path(root) / title
    (folder / 'sce_sys').mkdir(parents=True)
    (folder / 'sce_sys' / 'param.sfo').write_bytes(sfo_fixture({'TITLE_ID': title, 'APP_VER': '01.00'}))
    return folder


class GameProfileTests(unittest.TestCase):
    def test_second_son_gets_wave_barriers(self):
        with tempfile.TemporaryDirectory() as tmp:
            for title in ('CUSA00004', 'CUSA00223'):
                found, env = game_profiles.environment(game(tmp, title), {})
                self.assertEqual(found, title)
                self.assertEqual(env['GOW3_READBACKS'], '2')
                # The fence addresses are the EU 01.00 eboot's.
                self.assertEqual('GOW3_WATCHDOG_PEEK' in env, title == 'CUSA00004')

    def test_other_games_get_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(game_profiles.environment(game(tmp, 'CUSA01623'), {}), ('CUSA01623', {}))

    def test_explicit_values_and_opt_out_win(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = game(tmp, 'CUSA00004')
            self.assertNotIn('GOW3_READBACKS',
                             game_profiles.environment(folder, {'GOW3_READBACKS': '0'})[1])
            self.assertEqual(game_profiles.environment(folder, {'GOW3_GAME_PROFILE': '0'})[1], {})

    def test_missing_sfo(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(game_profiles.environment(Path(tmp), {}), (None, {}))


if __name__ == '__main__':
    unittest.main()
