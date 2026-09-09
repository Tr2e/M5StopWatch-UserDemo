import copy
from pathlib import Path
import tempfile
import unittest

from PIL import Image

from check_evidence import check, relative_file, validate_manifest


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.frames = self.root / "frames"
        self.baseline = self.root / "baseline"
        self.frames.mkdir()
        self.baseline.mkdir()
        Image.new("RGB", (16, 12), "red").save(self.frames / "view.png", compress_level=0)
        Image.new("RGB", (16, 12), "red").save(self.baseline / "view.png", compress_level=9)
        self.data = {"schema": 1, "identity_view": "showcase", "models": [{
            "id": "car", "required_views": ["showcase"], "frames": [
                {"view": "showcase", "file": "view.png", "expect_unchanged": True}]}]}

    def run_check(self, baseline=True):
        return check(self.data, self.frames, self.baseline if baseline else None, self.root / "output")

    def test_compression_does_not_change_pixels_or_sheet(self):
        self.assertNotEqual((self.frames / "view.png").read_bytes(), (self.baseline / "view.png").read_bytes())
        self.assertTrue(self.run_check()["evidence_checks_passed"])
        with Image.open(self.root / "output/car.png") as sheet, Image.open(self.frames / "view.png") as source:
            self.assertEqual(sheet.crop((0,28,16,40)).tobytes(), source.convert("RGBA").tobytes())

    def test_cross_format_baseline(self):
        Image.new("RGB", (16,12), "red").save(self.baseline / "original.ppm")
        self.data["models"][0]["frames"][0]["baseline_file"] = "original.ppm"
        self.assertTrue(self.run_check()["evidence_checks_passed"])

    def test_changed_pixels_fail(self):
        Image.new("RGB", (16,12), "blue").save(self.frames / "view.png")
        self.assertIn("expected unchanged", " ".join(self.run_check()["errors"]))

    def test_change_can_be_intentional(self):
        Image.new("RGB", (16,12), "blue").save(self.frames / "view.png")
        self.data["models"][0]["frames"][0]["expect_unchanged"] = False
        report = self.run_check()
        self.assertTrue(report["evidence_checks_passed"])
        self.assertFalse(report["models"][0]["frames"][0]["pixels_equal_to_baseline"])

    def test_missing_required_role(self):
        self.data["models"][0]["required_views"].append("rear")
        self.assertIn("missing required view rear", " ".join(self.run_check()["errors"]))

    def test_duplicate_identity_frame(self):
        second = copy.deepcopy(self.data["models"][0])
        second["id"] = "car2"
        self.data["models"].append(second)
        self.assertIn("duplicate identity frame", " ".join(self.run_check()["errors"]))

    def test_missing_file(self):
        self.data["models"][0]["frames"][0]["file"] = "missing.png"
        self.assertFalse(self.run_check()["evidence_checks_passed"])

    def test_invalid_image(self):
        (self.frames / "view.png").write_bytes(b"not an image")
        self.assertFalse(self.run_check()["evidence_checks_passed"])

    def test_missing_baseline(self):
        self.assertIn("baseline required", " ".join(self.run_check(False)["errors"]))

    def test_paths_cannot_escape_root(self):
        for value in ("../x.png", "/tmp/x.png", ""):
            with self.subTest(value=value), self.assertRaises(ValueError):
                relative_file(self.frames, value)
        (self.frames / "escape.png").symlink_to(self.baseline / "view.png")
        with self.assertRaises(ValueError):
            relative_file(self.frames, "escape.png")

    def test_malformed_and_duplicate_entries(self):
        cases = [[], {"schema": True}, {"schema": 1, "models": []}]
        duplicated = copy.deepcopy(self.data)
        duplicated["models"].append(copy.deepcopy(duplicated["models"][0]))
        cases.append(duplicated)
        for field, value in (("view", ""), ("expect_unchanged", "false"), ("file", "../x")):
            bad = copy.deepcopy(self.data)
            bad["models"][0]["frames"][0][field] = value
            cases.append(bad)
        duplicated_view = copy.deepcopy(self.data)
        duplicated_view["models"][0]["frames"] *= 2
        cases.append(duplicated_view)
        for data in cases:
            with self.subTest(data=data), self.assertRaises(ValueError):
                validate_manifest(data)

    def test_existing_output_not_overwritten(self):
        (self.root / "output").mkdir()
        with self.assertRaises(FileExistsError):
            self.run_check()

    def test_typo_cannot_silently_disable_baseline_gate(self):
        frame = self.data["models"][0]["frames"][0]
        frame["expect_unchangd"] = frame.pop("expect_unchanged")
        with self.assertRaisesRegex(ValueError, "unknown frame fields"):
            self.run_check()


if __name__ == "__main__":
    unittest.main()
