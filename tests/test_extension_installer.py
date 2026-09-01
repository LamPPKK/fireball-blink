#!/usr/bin/env python3
"""
Unit tests for Fireball Extension Downloader & Multi-Store Installer.
"""

import io
import json
import pathlib
import sys
import tempfile
import unittest
import zipfile

# Add tools to sys.path
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools"))
from extension_installer import extract_extension_id, get_crx_download_url, unpack_crx


class ExtensionInstallerTests(unittest.TestCase):
    def test_extract_chrome_webstore_urls(self):
        # Modern URL
        url1 = "https://chromewebstore.google.com/detail/ublock-origin/cjpalhdlnbpafiamejdnhcphjbkeiagm"
        ext_id1, store1 = extract_extension_id(url1)
        self.assertEqual(ext_id1, "cjpalhdlnbpafiamejdnhcphjbkeiagm")
        self.assertEqual(store1, "chrome")

        # Legacy URL
        url2 = "https://chrome.google.com/webstore/detail/tampermonkey/dhdgffkkmingnoiowhfoiddlmgmaenlh"
        ext_id2, store2 = extract_extension_id(url2)
        self.assertEqual(ext_id2, "dhdgffkkmingnoiowhfoiddlmgmaenlh")
        self.assertEqual(store2, "chrome")

        # Raw 32-char ID
        ext_id3, store3 = extract_extension_id("cjpalhdlnbpafiamejdnhcphjbkeiagm")
        self.assertEqual(ext_id3, "cjpalhdlnbpafiamejdnhcphjbkeiagm")
        self.assertEqual(store3, "chrome")

    def test_extract_edge_addons_urls(self):
        url = "https://microsoftedge.microsoft.com/addons/detail/ublock-origin/odfafepnkmbhccpbejgmiehpchacaeak"
        ext_id, store = extract_extension_id(url)
        self.assertEqual(ext_id, "odfafepnkmbhccpbejgmiehpchacaeak")
        self.assertEqual(store, "edge")

    def test_get_crx_download_url(self):
        chrome_url = get_crx_download_url("cjpalhdlnbpafiamejdnhcphjbkeiagm", "chrome")
        self.assertIn("clients2.google.com", chrome_url)
        self.assertIn("cjpalhdlnbpafiamejdnhcphjbkeiagm", chrome_url)

        edge_url = get_crx_download_url("odfafepnkmbhccpbejgmiehpchacaeak", "edge")
        self.assertIn("edge.microsoft.com", edge_url)
        self.assertIn("odfafepnkmbhccpbejgmiehpchacaeak", edge_url)

    def test_unpack_zip_extension(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            out_path = pathlib.Path(tmp_dir) / "test_ext"
            
            # Create a mock valid extension ZIP
            buf = io.BytesIO()
            with zipfile.ZipFile(buf, "w") as zf:
                zf.writestr("manifest.json", json.dumps({
                    "manifest_version": 3,
                    "name": "Mock Test Extension",
                    "version": "1.0.0"
                }))
                zf.writestr("background.js", "console.log('test');")
                
            manifest = unpack_crx(buf.getvalue(), out_path)
            self.assertEqual(manifest["name"], "Mock Test Extension")
            self.assertEqual(manifest["version"], "1.0.0")
            self.assertTrue((out_path / "background.js").exists())


if __name__ == "__main__":
    unittest.main()
