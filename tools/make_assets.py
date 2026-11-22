#!/usr/bin/env python3
"""Convert user-supplied local images/audio into firmware/StickStory/Assets.h.

Image slots (any of .jpg/.jpeg/.png/.webp, cover-cropped to 240x135 JPEG):
    ciallo.*      -> IMG_CIALLO     murasame1.* -> IMG_M1
    murasame2.*   -> IMG_M2         murasame3.* -> IMG_M3
    murasame8.*   -> IMG_WIN        murasame4.* -> IMG_FAIL

Audio slots (.wav only, resampled to 16000 Hz mono 16-bit, max 4 s each):
    murasame1.wav -> WAV_VOICE1 ... murasame5.wav -> WAV_VOICE5
    lemon.wav     -> WAV_LEMON      ciallo.wav    -> WAV_CIALLO

Missing slots become empty arrays and the firmware falls back to the
built-in procedural graphics/tones, so any subset of files works.

Usage:
    python tools/make_assets.py [assets_src_dir]

Only dependency: Pillow (images). WAV handling uses the standard library.
To convert mp3 first: ffmpeg -i in.mp3 -ar 16000 -ac 1 -c:a pcm_s16le out.wav
"""

from __future__ import annotations

import io
import struct
import sys
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
DEFAULT_SRC = HERE / "assets_src"
OUTPUT = PROJECT / "firmware" / "StickStory" / "Assets.h"

SCREEN = (240, 135)
JPEG_QUALITY = 60
AUDIO_RATE = 16000
AUDIO_MAX_SECONDS = 4

IMAGE_SLOTS = (
    ("IMG_CIALLO", ("ciallo",)),
    ("IMG_M1", ("murasame1",)),
    ("IMG_M2", ("murasame2",)),
    ("IMG_M3", ("murasame3",)),
    ("IMG_WIN", ("murasame8",)),
    ("IMG_FAIL", ("murasame4",)),
)
IMAGE_EXTS = (".jpg", ".jpeg", ".png", ".webp")

AUDIO_SLOTS = (
    ("WAV_VOICE1", "murasame1.wav"),
    ("WAV_VOICE2", "murasame2.wav"),
    ("WAV_VOICE3", "murasame3.wav"),
    ("WAV_VOICE4", "murasame4.wav"),
    ("WAV_VOICE5", "murasame5.wav"),
    ("WAV_LEMON", "lemon.wav"),
    ("WAV_CIALLO", "ciallo.wav"),
)


def find_image(src: Path, stem: str) -> Path | None:
    for ext in IMAGE_EXTS:
        candidate = src / (stem + ext)
        if candidate.is_file():
            return candidate
    return None


def convert_image(path: Path) -> bytes:
    from PIL import Image

    with Image.open(path) as img:
        img = img.convert("RGB")
        tw, th = SCREEN
        scale = max(tw / img.width, th / img.height)
        resized = img.resize((round(img.width * scale) + 1, round(img.height * scale) + 1))
        left = (resized.width - tw) // 2
        top = (resized.height - th) // 2
        cropped = resized.crop((left, top, left + tw, top + th))
        buffer = io.BytesIO()
        cropped.save(buffer, "JPEG", quality=JPEG_QUALITY)
        return buffer.getvalue()


def read_mono_float(wav: wave.Wave_read) -> tuple[list[float], int]:
    frames = wav.readframes(wav.getnframes())
    channels = wav.getnchannels()
    width = wav.getsampwidth()
    rate = wav.getframerate()
    count = wav.getnframes()
    if width == 1:
        raw = list(frames)
        samples = [(v - 128) / 128.0 for v in raw]
    elif width == 2:
        raw = struct.unpack(f"<{count * channels}h", frames)
        samples = [v / 32768.0 for v in raw]
    elif width == 3:
        samples = []
        for i in range(count * channels):
            chunk = frames[i * 3 : i * 3 + 3]
            value = int.from_bytes(chunk, "little", signed=True)
            samples.append(value / 8388608.0)
    elif width == 4:
        raw = struct.unpack(f"<{count * channels}i", frames)
        samples = [v / 2147483648.0 for v in raw]
    else:
        raise ValueError(f"unsupported sample width: {width}")
    if channels == 1:
        return samples, rate
    mono = []
    for i in range(count):
        mono.append(sum(samples[i * channels : (i + 1) * channels]) / channels)
    return mono, rate


def resample_linear(samples: list[float], src_rate: int, dst_rate: int) -> list[float]:
    if src_rate == dst_rate:
        return samples
    ratio = src_rate / dst_rate
    out_len = int(len(samples) / ratio)
    out: list[float] = []
    for i in range(out_len):
        pos = i * ratio
        lo = int(pos)
        frac = pos - lo
        hi = lo + 1 if lo + 1 < len(samples) else lo
        out.append(samples[lo] * (1.0 - frac) + samples[hi] * frac)
    return out


def convert_wav(path: Path) -> bytes:
    with wave.open(str(path), "rb") as wav:
        if wav.getcomptype() != "NONE":
            raise ValueError(f"{path.name}: only PCM wav is supported")
        samples, rate = read_mono_float(wav)
    samples = resample_linear(samples, rate, AUDIO_RATE)
    limit = AUDIO_RATE * AUDIO_MAX_SECONDS
    if len(samples) > limit:
        print(f"  note: {path.name} truncated to {AUDIO_MAX_SECONDS}s")
        samples = samples[:limit]
    pcm = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, s)) * 32767)) for s in samples)
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", 36 + len(pcm), b"WAVE", b"fmt ", 16, 1, 1,
        AUDIO_RATE, AUDIO_RATE * 2, 2, 16, b"data", len(pcm),
    )
    return header + pcm


def emit_array(lines: list[str], name: str, data: bytes) -> None:
    if not data:
        lines.append(f"inline constexpr uint8_t {name}[] = {{0}};")
        lines.append(f"inline constexpr size_t {name}_LEN = 0;")
        return
    lines.append(f"inline constexpr uint8_t {name}[] = {{")
    for offset in range(0, len(data), 20):
        chunk = ", ".join(f"0x{b:02x}" for b in data[offset : offset + 20])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append(f"inline constexpr size_t {name}_LEN = sizeof({name});")


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SRC
    if not src.is_dir():
        print(f"assets dir not found: {src}")
        print("create it and drop your own local image/audio files in, then rerun.")
        return 1

    images: dict[str, bytes] = {}
    for slot, stems in IMAGE_SLOTS:
        found = None
        for stem in stems:
            found = find_image(src, stem)
            if found:
                break
        if found is None:
            print(f"  [img] {slot}: missing -> procedural fallback")
            images[slot] = b""
            continue
        try:
            images[slot] = convert_image(found)
        except ImportError:
            print("Pillow is required for images: pip install pillow")
            return 1
        print(f"  [img] {slot}: {found.name} -> {len(images[slot])} bytes JPEG")

    audios: dict[str, bytes] = {}
    for slot, filename in AUDIO_SLOTS:
        path = src / filename
        if not path.is_file():
            print(f"  [wav] {slot}: missing -> tone fallback")
            audios[slot] = b""
            continue
        try:
            audios[slot] = convert_wav(path)
        except Exception as exc:  # keep going; this slot falls back to tones
            print(f"  [wav] {slot}: {filename} skipped ({exc})")
            audios[slot] = b""
            continue
        print(f"  [wav] {slot}: {filename} -> {len(audios[slot])} bytes")

    lines = [
        "// Generated by tools/make_assets.py from LOCAL user-supplied files.",
        "// Do not commit this file. Missing slots fall back to procedural art/tones.",
        "#pragma once",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "namespace assets {",
    ]
    for slot, _ in IMAGE_SLOTS:
        emit_array(lines, slot, images[slot])
    for slot, _ in AUDIO_SLOTS:
        emit_array(lines, slot, audios[slot])
    lines += ["}", ""]
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")

    total = sum(len(v) for v in images.values()) + sum(len(v) for v in audios.values())
    print(f"wrote {OUTPUT} ({total} bytes of asset data)")
    if total > 1024 * 1024:
        print("warning: over 1 MB of assets; watch flash usage at compile time.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
