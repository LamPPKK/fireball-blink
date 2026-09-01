#!/usr/bin/env python3
"""
Fireball Multi-Store Extension Installer & CRX Downloader
Supports:
1. Chrome Web Store (https://chromewebstore.google.com)
2. Microsoft Edge Add-ons (https://microsoftedge.microsoft.com/addons)
3. Sideloading Local CRX / ZIP / Unpacked directories
"""

from __future__ import annotations

import argparse
import io
import json
import os
import pathlib
import re
import struct
import sys
import urllib.parse
import urllib.request
import zipfile
from typing import Optional, Tuple


CHROME_WEBSTORE_URL_PATTERN = re.compile(r"(?:chromewebstore\.google\.com|chrome\.google\.com/webstore)/detail/(?:[^/]+/)?([a-z0-9]{32})", re.IGNORECASE)
EDGE_ADDONS_URL_PATTERN = re.compile(r"microsoftedge\.microsoft\.com/addons/detail/(?:[^/]+/)?([a-z0-9]{32})", re.IGNORECASE)


def extract_extension_id(url_or_id: str) -> Tuple[str, str]:
    """Returns (extension_id, store_type: 'chrome' | 'edge' | 'raw')"""
    url_or_id = url_or_id.strip()
    
    # Chrome Web Store
    m_chrome = CHROME_WEBSTORE_URL_PATTERN.search(url_or_id)
    if m_chrome:
        return m_chrome.group(1).lower(), "chrome"
        
    # Edge Add-ons
    m_edge = EDGE_ADDONS_URL_PATTERN.search(url_or_id)
    if m_edge:
        return m_edge.group(1).lower(), "edge"
        
    # Raw 32-character ID (defaults to Chrome)
    if len(url_or_id) == 32 and url_or_id.isalnum():
        return url_or_id.lower(), "chrome"
        
    raise ValueError(f"Could not extract valid extension ID or URL from: {url_or_id}")


def get_crx_download_url(extension_id: str, store: str = "chrome") -> str:
    """Generates direct CRX download URL from official store CDN."""
    if store == "edge":
        return f"https://edge.microsoft.com/extensionwebstorebase/v1/crx?response=redirect&prod=edgecrx&prodchannel=&x=id%3D{extension_id}%26installsource%3Dondemand%26uc"
    
    # Chrome Web Store (CRX3 direct endpoint)
    return f"https://clients2.google.com/service/update2/crx?response=redirect&prodversion=120.0.0.0&acceptformat=crx3&x=id%3D{extension_id}%26uc"


def unpack_crx(crx_bytes: bytes, output_dir: pathlib.Path) -> dict:
    """Unpacks CRX2/CRX3 binary format into output folder and returns manifest.json."""
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # CRX Header Check
    # Magic: 'Cr24' (0x34327243)
    if len(crx_bytes) < 16:
        raise ValueError("File is too small to be a valid CRX")
        
    magic, version = struct.unpack("<4sI", crx_bytes[:8])
    
    zip_offset = 0
    if magic == b"Cr24":
        if version == 2:
            key_len, sig_len = struct.unpack("<II", crx_bytes[8:16])
            zip_offset = 16 + key_len + sig_len
        elif version == 3:
            header_size = struct.unpack("<I", crx_bytes[8:12])[0]
            zip_offset = 12 + header_size
        else:
            raise ValueError(f"Unsupported CRX version: {version}")
    elif crx_bytes[:4] == b"PK\x03\x04":
        # Standard ZIP format
        zip_offset = 0
    else:
        raise ValueError("Invalid extension file format: neither CRX nor ZIP magic header found")

    zip_data = crx_bytes[zip_offset:]
    with zipfile.ZipFile(io.BytesIO(zip_data)) as zf:
        zf.extractall(output_dir)

    manifest_path = output_dir / "manifest.json"
    if not manifest_path.exists():
        raise ValueError("Extracted extension does not contain manifest.json")

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    return manifest


def install_extension(url_or_id: str, dest_dir: pathlib.Path, store_override: Optional[str] = None) -> dict:
    """Fetches CRX from store, extracts it, and verifies its manifest."""
    ext_id, detected_store = extract_extension_id(url_or_id)
    store = store_override or detected_store
    
    download_url = get_crx_download_url(ext_id, store)
    print(f"📥 Downloading extension '{ext_id}' from {store.upper()} store...")
    print(f"🔗 Source URL: {download_url}")
    
    req = urllib.request.Request(download_url, headers={
        "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    })
    
    with urllib.request.urlopen(req, timeout=30.0) as resp:
        crx_data = resp.read()
        
    target_path = dest_dir / ext_id
    manifest = unpack_crx(crx_data, target_path)
    
    name = manifest.get("name", ext_id)
    ver = manifest.get("version", "unknown")
    print(f"✅ Successfully installed '{name}' v{ver} into {target_path}")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description="Fireball Extension Downloader & Installer")
    parser.add_argument("url_or_id", help="Chrome Web Store URL, Edge Add-on URL, or 32-character Extension ID")
    parser.add_argument("--dest", default="dist/installed_extensions", help="Destination directory for unpacked extension")
    parser.add_argument("--store", choices=["chrome", "edge"], help="Force specific store")
    args = parser.parse_args()

    try:
        dest_path = pathlib.Path(args.dest)
        install_extension(args.url_or_id, dest_path, store_override=args.store)
        return 0
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
