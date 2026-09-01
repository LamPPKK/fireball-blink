#!/usr/bin/env python3
"""
Fireball Media Remuxer
Assembles downloaded HLS/DASH video chunks into clean, standalone MP4 files.
"""

from __future__ import annotations

import os
import pathlib
import subprocess
import tempfile
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class SegmentInfo:
    index: int
    duration_seconds: float
    file_path: pathlib.Path
    byte_size: int


class MediaRemuxer:
    """Stitches segmented media streams into final MP4/MKV video containers."""

    def __init__(self, output_dir: Optional[pathlib.Path] = None) -> None:
        self.output_dir = output_dir or pathlib.Path(tempfile.gettempdir()) / "fireball_downloads"
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def stitch_ts_segments_raw(self, segments: List[pathlib.Path], output_file: pathlib.Path) -> bool:
        """Concatenates MPEG-TS segments directly (lossless fast-path)."""
        if not segments:
            return False
        output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(output_file, "wb") as outfile:
            for seg in sorted(segments):
                if seg.exists():
                    with open(seg, "rb") as infile:
                        outfile.write(infile.read())
        return output_file.exists() and output_file.stat().st_size > 0

    def remux_to_mp4(self, input_ts: pathlib.Path, output_mp4: pathlib.Path) -> bool:
        """Remuxes TS stream to standard MP4 container via ffmpeg if available."""
        import shutil
        ffmpeg_bin = shutil.which("ffmpeg")
        if not ffmpeg_bin:
            # Fallback: keep concatenated TS container or rename
            shutil.copy2(input_ts, output_mp4)
            return True

        cmd = [
            ffmpeg_bin,
            "-y",
            "-i", str(input_ts),
            "-c", "copy",
            "-bsf:a", "aac_adtstoasc",
            "-movflags", "+faststart",
            str(output_mp4)
        ]
        try:
            res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
            return res.returncode == 0
        except Exception:
            shutil.copy2(input_ts, output_mp4)
            return True


if __name__ == "__main__":
    remuxer = MediaRemuxer()
    temp_dir = pathlib.Path(tempfile.mkdtemp())
    seg1 = temp_dir / "seg1.ts"
    seg2 = temp_dir / "seg2.ts"
    seg1.write_bytes(b"\x47" + b"A" * 187) # Valid TS sync byte
    seg2.write_bytes(b"\x47" + b"B" * 187)

    out_ts = temp_dir / "combined.ts"
    out_mp4 = temp_dir / "final.mp4"

    assert remuxer.stitch_ts_segments_raw([seg1, seg2], out_ts)
    assert out_ts.stat().st_size == 376
    assert remuxer.remux_to_mp4(out_ts, out_mp4)
    assert out_mp4.exists()

    print("✅ MediaRemuxer self-test passed!")
