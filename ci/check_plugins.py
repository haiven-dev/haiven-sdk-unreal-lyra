#!/usr/bin/env python3
"""Check the Haiven plugin descriptors that ship in this Lyra checkout."""

from __future__ import annotations

import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
PLUGINS = (
    "Plugins/Haiven/Haiven.uplugin",
    "Plugins/HaivenLyraDemo/HaivenLyraDemo.uplugin",
    "Plugins/HaivenLyraInventory/HaivenLyraInventory.uplugin",
)


def load_plugin(relative: str) -> dict:
    path = ROOT / relative
    descriptor = json.loads(path.read_text(encoding="utf-8"))
    if not descriptor.get("Modules"):
        raise AssertionError(f"{relative} has no Modules")
    if descriptor.get("FileVersion") != 3:
        raise AssertionError(f"{relative} FileVersion is {descriptor.get('FileVersion')!r}")
    return descriptor


class PluginDescriptorTests(unittest.TestCase):
    def test_haiven_plugins_are_loadable_descriptors(self) -> None:
        for relative in PLUGINS:
            with self.subTest(relative=relative):
                load_plugin(relative)


if __name__ == "__main__":
    unittest.main()
