import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

spec = importlib.util.spec_from_file_location("lyra_release", Path(__file__).with_name("release.py"))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


def fixture(root):
    """A tiny Lyra-shaped tree: descriptors, uproject, and the five platform markers."""
    (root / "Lyra.uproject").write_text(json.dumps({"EngineAssociation": "5.8"}))
    for relative in release.PLUGIN_DESCRIPTORS:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"VersionName": "1.2.3"}))
    sdk = root / release.SDK_DESCRIPTOR
    sdk.parent.mkdir(parents=True, exist_ok=True)
    sdk.write_text(json.dumps({"VersionName": "0.1.0-preview.1"}))
    for name in release.PLATFORMS:
        readme = root / "Platforms" / name / "readme.txt"
        readme.parent.mkdir(parents=True, exist_ok=True)
        readme.write_text(name + "\n")
    (root / "Source" / "LyraGame").mkdir(parents=True)
    (root / "Source" / "LyraGame" / "LyraGame.Build.cs").write_text("class LyraGame {}\n")


class VersionTests(unittest.TestCase):
    def test_tag_must_match_descriptor(self):
        with mock.patch.object(release, "descriptor_version", return_value="1.2.3"):
            self.assertEqual(release.release_version("v1.2.3"), "1.2.3")
            self.assertEqual(release.release_version(None), "1.2.3")
            for tag in ("v1.2.4", "1.2.3", "v1.2", "v1.2.3-rc.1", "vx.y.z", "v1.2.3/../x"):
                with self.subTest(tag=tag), self.assertRaises(RuntimeError):
                    release.release_version(tag)

    def test_prerelease_tag(self):
        with mock.patch.object(release, "descriptor_version", return_value="1.2.3-rc.1"):
            self.assertEqual(release.release_version("v1.2.3-rc.1"), "1.2.3-rc.1")

    def test_descriptor_must_be_semver(self):
        with mock.patch.object(release, "descriptor_version", return_value="1.0"):
            with self.assertRaises(RuntimeError):
                release.release_version(None)

    def test_checked_rejects_path_like_versions(self):
        for version in ("../1.2.3", "1.2.3/x", ""):
            with self.subTest(version=version), self.assertRaises(RuntimeError):
                release.checked(version)

    def test_plugin_versions_must_agree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root)
            self.assertEqual(release.descriptor_version(root), "1.2.3")
            path = root / release.PLUGIN_DESCRIPTORS[0]
            path.write_text(json.dumps({"VersionName": "9.9.9"}))
            with self.assertRaises(RuntimeError):
                release.descriptor_version(root)

    def test_sdk_plugin_version_is_not_the_release_version(self):
        # A plugin sync rewrites Haiven.uplugin to the upstream SDK version.
        # That must not change or block the demo release version.
        self.assertNotIn(release.SDK_DESCRIPTOR, release.PLUGIN_DESCRIPTORS)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root)
            for synced in ("0.1.0-preview.2", "2.0.0", "not-a-version"):
                with self.subTest(synced=synced):
                    (root / release.SDK_DESCRIPTOR).write_text(json.dumps({"VersionName": synced}))
                    self.assertEqual(release.descriptor_version(root), "1.2.3")
                    self.assertEqual(release.release_version("v1.2.3", root), "1.2.3")
            with self.assertRaises(RuntimeError):
                release.release_version("v2.0.0", root)


class SourceZipTests(unittest.TestCase):
    def test_source_zip_excludes_build_products_and_fab_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "proj"
            root.mkdir()
            fixture(root)
            for relative in ("Binaries/Linux/Lyra.so", "Intermediate/Build/x.o",
                             "Source/LyraGame/Intermediate/y.o", "Saved/z.log",
                             "Content/Fab/Hero.uasset"):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"nope")
            with self.assertRaises(RuntimeError):
                release.source("1.2.3", Path(directory) / "out", root)
            (root / "Content" / "Fab" / "Hero.uasset").unlink()
            first = release.source("1.2.3", Path(directory) / "out", root)
            second = release.source("1.2.3", Path(directory) / "again", root)
            self.assertEqual(first.read_bytes(), second.read_bytes(), "zip output must be deterministic")
            with zipfile.ZipFile(first) as bundle:
                names = bundle.namelist()
                payload = json.loads(bundle.read("Lyra/release-manifest.json"))
            self.assertIn("Lyra/Lyra.uproject", names)
            self.assertIn("Lyra/Source/LyraGame/LyraGame.Build.cs", names)
            self.assertNotIn("Lyra/Binaries/Linux/Lyra.so", names)
            self.assertFalse(any(part in release.EXCLUDED_DIRS for name in names for part in name.split("/")))
            self.assertFalse(any(Path(name).suffix.lower() in release.FORBIDDEN_SUFFIXES for name in names))
            self.assertEqual(payload["playerBinary"], None)
            self.assertEqual(payload["fabContent"], "not-included")
            self.assertEqual(payload["platforms"], list(release.PLATFORMS))
            self.assertEqual(payload["engineAssociation"], "5.8")

    def test_repository_source_bundle(self):
        # Read the version from the descriptors so a release-please bump does not break this test.
        version = release.descriptor_version()
        with tempfile.TemporaryDirectory() as directory:
            archive = release.source(version, directory)
            self.assertEqual(archive.name, f"haiven-unreal-lyra-source-{version}.zip")
            with zipfile.ZipFile(archive) as bundle:
                names = bundle.namelist()
                payload = json.loads(bundle.read("Lyra/release-manifest.json"))
            self.assertIn("Lyra/Lyra.uproject", names)
            self.assertIn("Lyra/Plugins/Haiven/Haiven.uplugin", names)
            for name in release.PLATFORMS:
                self.assertIn(f"Lyra/Platforms/{name}/readme.txt", names)
            self.assertTrue(all(name.startswith("Lyra/") for name in names))
            self.assertNotIn("Lyra/.git", names)
            self.assertFalse(any(Path(name).suffix.lower() in release.FORBIDDEN_SUFFIXES for name in names))
            self.assertEqual(payload["version"], version)
            self.assertEqual(payload["engineAssociation"], release.engine_association())
            self.assertIsNone(payload["playerBinary"])

    def test_symlink_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "proj"
            root.mkdir()
            fixture(root)
            target = root / "Source" / "LyraGame" / "LyraGame.Build.cs"
            link = root / "Source" / "LyraGame" / "alias.cs"
            link.symlink_to(target)
            with self.assertRaises(RuntimeError):
                release.source("1.2.3", Path(directory) / "out", root)

    def test_player_binary_is_refused(self):
        with self.assertRaises(RuntimeError) as raised:
            release.refuse_player_binary()
        self.assertIn("Fab", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
