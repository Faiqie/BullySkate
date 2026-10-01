import importlib.util,unittest,tempfile,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
audit=module('audit',ROOT/'tools/AuditDistribution.py')
geometry=module('geometry',ROOT/'extractor/bully_geometry.py')
class DistributionTests(unittest.TestCase):
    def test_game_assets_and_state_are_rejected(self):
        for path in ['default.xex','runtime/skate-assets/private/game.json','scripts/BullyMotion/assets/world.bmgeo','jimmy-bind.json','BullyFile6','../bad.dll']:
            with self.subTest(path=path),self.assertRaises(ValueError):audit.check_name(path)
    def test_mod_code_and_authored_action_bank_are_allowed(self):
        for path in ['native/rig.h','scripts/BullyMotion/main.lua','scripts/BullyMotion/BullyMotion.cat','runtime/expected-skate-assets.sha256']:
            audit.check_name(path)
    def test_truncated_native_col_is_refused(self):
        with self.assertRaises(ValueError):geometry.BullyColFile.from_bytes(b'COL3'+struct.pack('<I',1000)+bytes(100))
    def test_synthetic_col_reads_only_header_values(self):
        raw=bytearray(104);raw[:4]=b'COL3';struct.pack_into('<I',raw,4,96);struct.pack_into('<I',raw,32,123)
        struct.pack_into('<3f',raw,52,-1,-2,-3);struct.pack_into('<3f',raw,68,1,2,3)
        record=geometry.BullyColFile.from_bytes(raw).models[0]
        self.assertEqual(record.object_id,123);self.assertEqual(record.bounds.min,(-1.,-2.,-3.))
if __name__=='__main__':unittest.main()
