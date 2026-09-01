#!/usr/bin/env python3
"""
Unit tests for Fireball Installer Builders.
"""

import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
from build_installers import calculate_sha256


class InstallerBuilderTests(unittest.TestCase):
    def test_calculate_sha256(self):
        repo_root = pathlib.Path(__file__).resolve().parent.parent
        readme = repo_root / "README.md"
        sha = calculate_sha256(readme)
        self.assertEqual(len(sha), 64)
        self.assertTrue(all(c in "0123456789abcdef" for c in sha))

    def test_installer_manifest_structure(self):
        repo_root = pathlib.Path(__file__).resolve().parent.parent
        manifest_file = repo_root / "dist" / "installers" / "installer_manifest.json"
        if manifest_file.exists():
            with open(manifest_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            self.assertEqual(data.get("ecosystem"), "Fireball Browser")
            self.assertEqual(data.get("version"), "1.0.0")
            self.assertIn("installers", data)


if __name__ == "__main__":
    unittest.main()
