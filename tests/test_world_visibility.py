import importlib.util, unittest, tempfile
from pathlib import Path
from types import SimpleNamespace
spec=importlib.util.spec_from_file_location('geometry',Path(__file__).resolve().parents[1]/'extractor/bully_geometry.py')
geometry=importlib.util.module_from_spec(spec);spec.loader.exec_module(geometry)

class WorldVisibilityTests(unittest.TestCase):
    def test_native_appearance_bits_are_read_from_object_definitions(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'Objects').mkdir()
            (root/'Objects/test.ide').write_text('objs\n10, seasonal, texture, 1, 200, 8192, 0, 1, 0, 196, 0, 0, 0\n11, permanent, texture, 1, 200, 8192, 0, 1, 0, 255, 0, 0, 0\nend\n')
            masks=geometry.appearance_masks(root)
            self.assertFalse(masks[10] & (1<<5));self.assertTrue(masks[10] & (1<<2))
            self.assertTrue(all(masks[11]&(1<<phase) for phase in range(8)))

    def test_walkable_and_nogo_collision_is_preserved_like_native_bully(self):
        self.assertTrue(geometry.solid_model(SimpleNamespace(minor_type=1)))
        self.assertTrue(geometry.solid_model(SimpleNamespace(minor_type=2)))
        self.assertTrue(geometry.solid_model(SimpleNamespace(minor_type=3)))

    def test_missing_object_definitions_fail_without_guessing_seasons(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):geometry.appearance_masks(Path(temp))
