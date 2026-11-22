"""Build StickS3 media and dialogue from a local Yuiline web checkout."""

import argparse
import io
import json
import re
import subprocess
from pathlib import Path

from PIL import Image, ImageOps

DISPLAY = (240, 135)
AUDIO = {
    "Jazz": "urban_acid_jazz_vn_bgm.mp3",
    "Crowd": "crowd_sound.mp3",
    "Rice": "fried_rice_sound.mp3",
    "Heels": "high_heels_sound.mp3",
    "Water": "water_fall_sound.mp3",
}


def extract_lines(source):
    html = (source / "game.html").read_text(encoding="utf-8-sig")
    start = re.search(r"\bconst\s+lines\s*=\s*(?=\[)", html)
    if start is None:
        raise ValueError("找不到 game.html 的剧情数组")
    lines, _ = json.JSONDecoder().raw_decode(html[start.end():])
    if not isinstance(lines, list) or not lines or not all(isinstance(line, str) for line in lines):
        raise ValueError("剧情数组应包含对白字符串")
    return lines


def jpeg(image):
    output = io.BytesIO()
    image.convert("RGB").save(output, format="JPEG", quality=86, optimize=True)
    return output.getvalue()


def make_images(source):
    day = ImageOps.fit(Image.open(source / "素材1.png").convert("RGBA"), DISPLAY, Image.Resampling.LANCZOS)
    night = ImageOps.fit(Image.open(source / "素材2.png").convert("RGBA"), DISPLAY, Image.Resampling.LANCZOS)
    yield "Day", jpeg(day)
    yield "Night", jpeg(night)

    for name, filename in (("Normal", "ss.png"), ("Sad", "ss-sad.png")):
        portrait = Image.open(source / filename).convert("RGBA")
        portrait = portrait.crop(portrait.getbbox())
        portrait.thumbnail((94, 107), Image.Resampling.LANCZOS)
        x = (DISPLAY[0] - portrait.width) // 2
        y = 105 - portrait.height
        for opacity in range(1, 4):
            composite = day.copy()
            layer = portrait.copy()
            layer.putalpha(portrait.getchannel("A").point(lambda a: a * opacity // 3))
            composite.alpha_composite(layer, (x, y))
            yield f"{name}{opacity}", jpeg(composite)

    back = Image.open(source / "back.png").convert("RGBA")
    back = back.crop(back.getbbox())
    back.thumbnail((18, 14), Image.Resampling.LANCZOS)
    icon = Image.new("RGBA", (18, 14), "#101b2a")
    icon.alpha_composite(back, ((18 - back.width) // 2, (14 - back.height) // 2))
    yield "Back", jpeg(icon)


def make_audio(source):
    for name, filename in AUDIO.items():
        result = subprocess.run(
            [
                "ffmpeg", "-nostdin", "-v", "error", "-i", str(source / filename),
                "-f", "u8", "-acodec", "pcm_u8", "-ac", "1", "-ar", "12000", "pipe:1",
            ],
            check=True,
            stdout=subprocess.PIPE,
        )
        yield name, result.stdout


def write_blob(stream, name, data):
    stream.write(f"alignas(4) static const char {name}Bytes[] =\n")
    for start in range(0, len(data), 32):
        chunk = "".join(f"\\x{byte:02x}" for byte in data[start:start + 32])
        stream.write(f'    "{chunk}"\n')
    stream.write(
        f";\nconst Blob {name}{{reinterpret_cast<const uint8_t*>({name}Bytes), "
        f"sizeof({name}Bytes) - 1}};\n\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    lines = extract_lines(args.source)
    images = list(make_images(args.source))
    audio = list(make_audio(args.source))
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "Generated.cpp").open("w", encoding="utf-8") as stream:
        stream.write('#include "YuilineData.h"\n\nnamespace yuiline {\n\n')
        stream.write("const char* const Lines[] = {\n")
        for line in lines:
            stream.write(f"    {json.dumps(line, ensure_ascii=False)},\n")
        stream.write("};\nconst size_t LineCount = sizeof(Lines) / sizeof(Lines[0]);\n\n")
        for name, data in images + audio:
            write_blob(stream, name, data)
        stream.write("}  // namespace yuiline\n")
    print(f"{len(lines)} lines; {sum(len(data) for _, data in images + audio):,} media bytes")


if __name__ == "__main__":
    main()
