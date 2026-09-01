#!/usr/bin/env python3
"""
Fireball Master Ecosystem Packager
Builds, verifies, and packages distribution bundles for all 3 Pillars and Platforms.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import zipfile


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def package_extension(repo_root: pathlib.Path, dist_dir: pathlib.Path) -> pathlib.Path:
    ext_dir = repo_root / "fireball-extension"
    out_zip = dist_dir / "fireball-extension-v1.0.0.zip"
    print(f"📦 Packaging WebExtension into {out_zip.name}...")
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(ext_dir):
            for file in files:
                full_path = pathlib.Path(root) / file
                rel_path = full_path.relative_to(ext_dir)
                zf.write(full_path, arcname=str(rel_path))
    return out_zip


def package_server(repo_root: pathlib.Path, dist_dir: pathlib.Path) -> pathlib.Path:
    srv_dir = repo_root / "fireball-server"
    out_tar = dist_dir / "fireball-server-v1.0.0.tar.gz"
    print(f"📦 Packaging Fireball Server into {out_tar.name}...")
    import tarfile
    with tarfile.open(out_tar, "w:gz") as tf:
        tf.add(srv_dir, arcname="fireball-server")
    return out_tar


def package_j2me(repo_root: pathlib.Path, dist_dir: pathlib.Path) -> pathlib.Path:
    j2me_dir = repo_root / "fireball-j2me"
    out_zip = dist_dir / "fireball-j2me-v1.0.0.zip"
    print(f"📦 Packaging Java ME Edition into {out_zip.name}...")
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(j2me_dir):
            for file in files:
                full_path = pathlib.Path(root) / file
                rel_path = full_path.relative_to(j2me_dir)
                zf.write(full_path, arcname=str(rel_path))
    return out_zip


def package_windows_source(repo_root: pathlib.Path, dist_dir: pathlib.Path) -> pathlib.Path:
    win_dir = repo_root / "fireball-win"
    out_zip = dist_dir / "fireball-win-v1.0.0-src.zip"
    print(f"📦 Packaging Windows Lite Source into {out_zip.name}...")
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(win_dir):
            for file in files:
                full_path = pathlib.Path(root) / file
                rel_path = full_path.relative_to(win_dir)
                zf.write(full_path, arcname=str(rel_path))
    return out_zip


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    dist_dir = repo_root / "dist"
    dist_dir.mkdir(parents=True, exist_ok=True)

    print("==========================================")
    print("🚀 Fireball Master Ecosystem Packager")
    print("==========================================")

    artifacts = [
        package_server(repo_root, dist_dir),
        package_extension(repo_root, dist_dir),
        package_j2me(repo_root, dist_dir),
        package_windows_source(repo_root, dist_dir),
    ]

    manifest = {
        "schema_version": 1,
        "ecosystem": "Fireball Browser",
        "version": "1.0.0",
        "artifacts": {}
    }

    for art in artifacts:
        manifest["artifacts"][art.name] = {
            "bytes": art.stat().st_size,
            "sha256": sha256_file(art)
        }

    manifest_path = dist_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"📝 Manifest written to {manifest_path.name}")
    print("==========================================")
    print("✅ All distribution packages generated successfully in dist/")
    print("==========================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
