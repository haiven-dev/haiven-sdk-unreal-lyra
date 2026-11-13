#!/usr/bin/env python3
"""Build the source archive `.github/workflows/release.yml` publishes.

The archive is one zip of this repository: Lyra source for Windows, Mac,
Linux, Android, and iOS. Cooked player binaries are not produced. Those need
Lyra Starter Game content from Fab, and this tree has no .uasset or .umap
files. This script does not fetch or copy that content.

Usage:

    python3 ci/release.py version [--tag v1.2.3]
        Print the release version. The two Lyra demo plugin descriptors
        (HaivenLyraDemo and HaivenLyraInventory) must share one VersionName,
        which release-please owns. With --tag, the tag must be `v` plus that
        VersionName. Plugins/Haiven/Haiven.uplugin is not read: it is the
        upstream haiven-sdk-unreal copy, and its VersionName names the SDK
        version that the last plugin sync brought in.

    python3 ci/release.py source --version 1.2.3 --out dist/
        Write `haiven-unreal-lyra-source-<version>.zip`. The zip is the git
        tree plus `Lyra/release-manifest.json`.

    python3 ci/release.py player
        Exit non-zero. This repository cannot build a cooked player.

Only stdlib Python, so the same command runs on Linux, macOS, and Windows.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
PROJECT = "Lyra"
ARCHIVE_PREFIX = PROJECT + "/"
# The demo plugins carry this repository's release version. release-please
# bumps their VersionName (release-please-config.json extra-files).
PLUGIN_DESCRIPTORS = (
    "Plugins/HaivenLyraDemo/HaivenLyraDemo.uplugin",
    "Plugins/HaivenLyraInventory/HaivenLyraInventory.uplugin",
)
# The vendored SDK plugin keeps the haiven-sdk-unreal version. A plugin sync
# overwrites it, so it is not compared with the demo version.
SDK_DESCRIPTOR = "Plugins/Haiven/Haiven.uplugin"
# Epic's platform folders. The source zip is the cross-platform artifact.
PLATFORMS = ("Windows", "Mac", "Linux", "Android", "IOS")
# Editor and UBT output. Never ship these, at any depth.
EXCLUDED_DIRS = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", "__pycache__"}
# Marketplace content and cooked output. Refusing these keeps Fab out of a release.
FORBIDDEN_SUFFIXES = {".uasset", ".umap", ".uexp", ".ubulk", ".uptnl", ".pak", ".ucas", ".utoc"}
VERSION = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z][0-9A-Za-z.-]*)?$")
# Fixed timestamp so the same tree always produces the same zip bytes.
EPOCH = (1980, 1, 1, 0, 0, 0)


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def descriptor_version(root=None):
    root = Path(root) if root else REPO
    found = {}
    for relative in PLUGIN_DESCRIPTORS:
        path = root / relative
        try:
            found[relative] = read_json(path)["VersionName"]
        except (OSError, json.JSONDecodeError, KeyError) as error:
            raise RuntimeError(f"{relative}: {error}") from error
    versions = set(found.values())
    if len(versions) != 1:
        detail = ", ".join(f"{name}={value}" for name, value in sorted(found.items()))
        raise RuntimeError(f"Lyra demo plugin VersionName values differ: {detail}")
    return versions.pop()


def engine_association(root=None):
    root = Path(root) if root else REPO
    path = root / "Lyra.uproject"
    try:
        value = read_json(path).get("EngineAssociation")
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"{path.name}: {error}") from error
    if not value or not isinstance(value, str):
        raise RuntimeError("Lyra.uproject has no EngineAssociation")
    return value


def release_version(tag=None, root=None):
    declared = descriptor_version(root)
    if not VERSION.match(declared):
        raise RuntimeError(f"VersionName {declared!r} is not a semantic version")
    if not tag:
        return declared
    if not tag.startswith("v") or not VERSION.match(tag[1:]):
        raise RuntimeError(f"tag {tag!r} is not v<major>.<minor>.<patch>[-prerelease]")
    if tag[1:] != declared:
        raise RuntimeError(
            f"tag {tag} does not match the Lyra demo plugin VersionName {declared!r}; "
            f"bump those descriptors to {tag[1:]!r} before tagging")
    return declared


def checked(version):
    if not VERSION.match(version):
        raise RuntimeError(f"version {version!r} is not <major>.<minor>.<patch>[-prerelease]")
    return version


def require_platforms(root):
    missing = [name for name in PLATFORMS if not (root / "Platforms" / name / "readme.txt").is_file()]
    if missing:
        raise RuntimeError("missing platform source markers: " + ", ".join(missing))


def _accept(root, path):
    relative = path.relative_to(root).as_posix()
    parts = Path(relative).parts
    if ".git" in parts or any(part in EXCLUDED_DIRS for part in parts):
        return None
    if path.is_symlink():
        raise RuntimeError(f"refusing to archive symlink {relative}")
    if not path.is_file():
        return None
    if path.suffix.lower() in FORBIDDEN_SUFFIXES:
        raise RuntimeError(
            "refusing to archive Fab marketplace or cooked content "
            f"({relative}); it is not part of this source release")
    return path


def tracked_files(root):
    """Files in the git index at `root`, or None when `root` is not its own checkout."""
    try:
        top = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "--show-toplevel"],
            check=False, capture_output=True, text=True)
    except OSError:
        return None
    if top.returncode != 0:
        return None
    if Path(top.stdout.strip()).resolve() != root.resolve():
        return None
    listed = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=True, capture_output=True)
    files = []
    for raw in listed.stdout.split(b"\0"):
        if not raw:
            continue
        path = _accept(root, root / raw.decode())
        if path is not None:
            files.append(path)
    return files


def walked_files(root, skip_dirs):
    skipped = {path.resolve() for path in skip_dirs}
    files = []
    for directory, subdirectories, names in os.walk(root, followlinks=False):
        current = Path(directory)
        if current.resolve() in skipped:
            subdirectories[:] = []
            continue
        kept = []
        for name in sorted(subdirectories):
            child = current / name
            if name in EXCLUDED_DIRS or name == ".git" or child.resolve() in skipped:
                continue
            kept.append(name)
        subdirectories[:] = kept
        for name in sorted(names):
            path = _accept(root, current / name)
            if path is not None:
                files.append(path)
    return files


def collect_files(root, skip_dirs=()):
    """Tracked files when `root` is a checkout. Otherwise walk, for a fixture tree."""
    root = root.resolve()
    files = tracked_files(root)
    if files is None:
        files = walked_files(root, skip_dirs)
    return sorted(files, key=lambda path: path.relative_to(root).as_posix())


def manifest(version, association):
    payload = {
        "engineAssociation": association,
        "fabContent": "not-included",
        "kind": "source",
        "name": "haiven-sdk-unreal-lyra",
        "platforms": list(PLATFORMS),
        "playerBinary": None,
        "version": version,
    }
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")


def write_zip(root, archive, version, skip_dirs=()):
    root = Path(root).resolve()
    require_platforms(root)
    association = engine_association(root)
    files = collect_files(root, skip_dirs)
    if not files:
        raise RuntimeError(f"nothing to package under {root}")
    archive = Path(archive)
    archive.parent.mkdir(parents=True, exist_ok=True)
    entries = [(ARCHIVE_PREFIX + path.relative_to(root).as_posix(), path.read_bytes()) for path in files]
    manifest_name = ARCHIVE_PREFIX + "release-manifest.json"
    if any(name == manifest_name for name, _ in entries):
        raise RuntimeError(f"{manifest_name} is already in the tree")
    entries.append((manifest_name, manifest(version, association)))
    entries.sort(key=lambda item: item[0])
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for name, data in entries:
            info = zipfile.ZipInfo(name, date_time=EPOCH)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100000 | 0o644) << 16
            bundle.writestr(info, data)
    return archive


def source(version, out, root=None):
    version = checked(version)
    root = Path(root).resolve() if root else REPO
    declared = descriptor_version(root)
    if version != declared:
        raise RuntimeError(
            f"version {version!r} does not match the Lyra demo plugin VersionName {declared!r}")
    out = Path(out).resolve()
    skip = [out] if out == root or root in out.parents else []
    archive = out / f"haiven-unreal-lyra-source-{version}.zip"
    return write_zip(root, archive, version, skip)


def refuse_player_binary():
    """Cooked players are not a product of this repository."""
    raise RuntimeError(
        "cooked Lyra player binaries need Lyra Starter Game from Fab, owned by "
        "a Haiven Inc Epic account; that content is not in this repository and "
        "is not fetched or vendored")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    version_parser = commands.add_parser("version")
    version_parser.add_argument("--tag")
    source_parser = commands.add_parser("source")
    source_parser.add_argument("--version", required=True)
    source_parser.add_argument("--out", required=True)
    commands.add_parser("player")
    args = parser.parse_args(argv)
    if args.command == "version":
        print(release_version(args.tag))
    elif args.command == "source":
        print(source(args.version, args.out))
    elif args.command == "player":
        refuse_player_binary()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        sys.exit(str(error))
