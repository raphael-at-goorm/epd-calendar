#!/usr/bin/env python3
"""
fontconvert_kr.py — EPD47 font generator with Korean (KSX1001) + Latin support.

Usage:
    python3 fontconvert_kr.py --compress NotoSansKR_36 36 NotoSansKR-Regular.ttf > NotoSansKR_36.h

KSX1001 subset: 2,350 most common Korean syllables (~21% of full Hangul block,
covers ~99.9% of modern Korean text). Keeps flash usage manageable on ESP32-S3.
"""
import freetype
import zlib
import sys
import math
import argparse
from collections import namedtuple

parser = argparse.ArgumentParser()
parser.add_argument("name",      help="C identifier for the generated font.")
parser.add_argument("size",      type=int, help="Font size in pixels.")
parser.add_argument("fontstack", nargs='+', help="Font file(s), highest priority first.")
parser.add_argument("--compress", action="store_true", help="Compress glyph bitmaps.")
args = parser.parse_args()

GlyphProps = namedtuple("GlyphProps",
    ["width","height","advance_x","left","top","compressed_size","data_offset","code_point"])


# ── Build the KSX1001 Hangul syllable set via Python's EUC-KR codec ───────────
def _build_ksx1001():
    pts = set()
    for b1 in range(0xB0, 0xC9):
        for b2 in range(0xA1, 0xFF):
            try:
                ch = bytes([b1, b2]).decode('euc-kr')
                cp = ord(ch)
                if 0xAC00 <= cp <= 0xD7A3:
                    pts.add(cp)
            except Exception:
                pass
    return pts

_KSX1001 = _build_ksx1001()
print(f"KSX1001 syllables loaded: {len(_KSX1001)}", file=sys.stderr)


# ── Collapse sorted code-point list into (start, end) run intervals ───────────
def _make_runs(sorted_pts):
    runs = []
    if not sorted_pts:
        return runs
    s = e = sorted_pts[0]
    for cp in sorted_pts[1:]:
        if cp == e + 1:
            e = cp
        else:
            runs.append((s, e))
            s = e = cp
    runs.append((s, e))
    return runs


# ── Target Unicode intervals ──────────────────────────────────────────────────
_extra = sorted([
    *range(0x0020, 0x007F),   # Basic Latin (ASCII printable)
    0x00B7,                    # · middle dot
    0x2013, 0x2014,            # – — dashes
    0x2018, 0x2019,            # ' ' smart single quotes
    0x201C, 0x201D,            # " " smart double quotes
    0x2022,                    # • bullet
    0x2026,                    # … ellipsis
    0x25B6,                    # ▶ right triangle
    0x25CF,                    # ● black circle
    *range(0x3000, 0x3040),    # CJK Symbols & Punctuation (。「」、・ etc.)
])

_all_pts = sorted(set(_extra) | _KSX1001)
intervals = _make_runs(_all_pts)
print(f"Total code points: {len(_all_pts)}  intervals: {len(intervals)}", file=sys.stderr)


# ── Font setup ────────────────────────────────────────────────────────────────
font_stack = [freetype.Face(f) for f in args.fontstack]
for face in font_stack:
    face.set_char_size(args.size << 6, args.size << 6, 150, 150)

def norm_floor(v): return int(math.floor(v / (1 << 6)))
def norm_ceil(v):  return int(math.ceil(v  / (1 << 6)))

def chunks(l, n):
    for i in range(0, len(l), n):
        yield l[i:i + n]


# ── Rasterise every glyph ─────────────────────────────────────────────────────
def load_glyph(cp):
    for idx, face in enumerate(font_stack):
        gi = face.get_char_index(cp)
        if gi > 0:
            face.load_glyph(gi, freetype.FT_LOAD_RENDER)
            return face
        print(f"  fallback font {idx+1} for U+{cp:04X}", file=sys.stderr)
    raise ValueError(f"U+{cp:04X} not in any font")

total_raw  = 0
total_comp = 0
all_glyphs = []

for i_start, i_end in intervals:
    for cp in range(i_start, i_end + 1):
        face   = load_glyph(cp)
        bm     = face.glyph.bitmap
        pixels = []
        px     = 0
        for i, v in enumerate(bm.buffer):
            x = i % bm.width
            if x % 2 == 0:
                px = v >> 4
            else:
                px = px | (v & 0xF0)
                pixels.append(px)
                px = 0
            if x == bm.width - 1 and bm.width % 2:
                pixels.append(px)
                px = 0

        packed = bytes(pixels)
        total_raw += len(packed)
        data = zlib.compress(packed) if args.compress else packed
        total_comp += len(data)

        all_glyphs.append((GlyphProps(
            width           = bm.width,
            height          = bm.rows,
            advance_x       = norm_floor(face.glyph.advance.x),
            left            = face.glyph.bitmap_left,
            top             = face.glyph.bitmap_top,
            compressed_size = len(data),
            data_offset     = total_comp - len(data),
            code_point      = cp,
        ), data))

print(f"raw={total_raw}  compressed={total_comp}", file=sys.stderr)

face_pipe = load_glyph(ord('|'))

# ── Emit header ───────────────────────────────────────────────────────────────
name = args.name

print("#pragma once")
print('#include "epd_driver.h"')

# Bitmap array
all_bytes = []
for _, data in all_glyphs:
    all_bytes.extend(data)
print(f"const uint8_t {name}Bitmaps[{len(all_bytes)}] = {{")
for c in chunks(all_bytes, 16):
    print("    " + " ".join(f"0x{b:02X}," for b in c))
print("};")

# Glyph array
print(f"const GFXglyph {name}Glyphs[] = {{")
for g, _ in all_glyphs:
    label = repr(chr(g.code_point))
    print("    { " + ", ".join(str(x) for x in list(g[:-1])) + " },  // " + label)
print("};")

# Interval array
print(f"const UnicodeInterval {name}Intervals[] = {{")
offset = 0
for i_start, i_end in intervals:
    print(f"    {{ 0x{i_start:04X}, 0x{i_end:04X}, 0x{offset:X} }},")
    offset += i_end - i_start + 1
print("};")

# Font struct
print(f"const GFXfont {name} = {{")
print(f"    (uint8_t*){name}Bitmaps,")
print(f"    (GFXglyph*){name}Glyphs,")
print(f"    (UnicodeInterval*){name}Intervals,")
print(f"    {len(intervals)},")
print(f"    {1 if args.compress else 0},")
print(f"    {norm_ceil(face_pipe.size.height)},")
print(f"    {norm_ceil(face_pipe.size.ascender)},")
print(f"    {norm_floor(face_pipe.size.descender)},")
print("};")
