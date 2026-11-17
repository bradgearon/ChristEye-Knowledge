"""Supply-chain/provenance guard for standalone Anaiero science fixtures."""
import hashlib
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCI = ROOT / 'examples' / 'scientific'


class ScientificTraceTests(unittest.TestCase):
    def test_sealed_reproducible_fixtures(self):
        for basename, channel, count in (
            ('native-reference.cevscitrace', 'ELECTRICAL_ACTIVITY', 112),
            ('native-environment.cevscitrace', 'ENVIRONMENT_PROXY', 28),
        ):
            with self.subTest(file=basename):
                data = (SCI / basename).read_bytes()
                body, mark = data.rsplit(b'seal ', 1)
                self.assertEqual(hashlib.sha256(body).hexdigest().encode() + b'\n', mark)
                lines = body.decode('ascii').splitlines()
                self.assertEqual(lines[0], 'CEVSCITRACE/1')
                self.assertIn(f'channel {channel}', lines)
                self.assertIn('classification SYNTHETIC_FIXTURE', lines)
                rows = [x for x in lines if x.startswith('sample ')]
                self.assertEqual(len(rows), count)
                self.assertEqual([int(x.split()[1]) for x in rows], list(range(1,count+1)))
                self.assertTrue(all(0 <= int(x.split()[2]) <= 1000 for x in rows))


if __name__ == '__main__':
    unittest.main()
