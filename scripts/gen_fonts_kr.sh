#!/usr/bin/env bash
# Generate Korean+Latin font headers for custom_calendar.
#
# Prerequisites:
#   pip3 install freetype-py
#   Download NotoSansKR-Regular.ttf from https://fonts.google.com/noto/specimen/Noto+Sans+KR
#   Place the .ttf file in the same directory as this script, or pass the path as $1.
#
# Usage:
#   cd scripts/
#   bash gen_fonts_kr.sh [path/to/NotoSansKR-Regular.ttf]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FONT_FILE="${1:-$SCRIPT_DIR/NotoSansKR-Regular.ttf}"
OUT_DIR="$SCRIPT_DIR/../examples/custom_calendar/fonts"
CONVERTER="$SCRIPT_DIR/fontconvert_kr.py"

if [ ! -f "$FONT_FILE" ]; then
    echo "ERROR: Font file not found: $FONT_FILE"
    echo "Download NotoSansKR-Regular.ttf from https://fonts.google.com/noto/specimen/Noto+Sans+KR"
    exit 1
fi

mkdir -p "$OUT_DIR"

echo "Generating fonts from: $FONT_FILE"

for SIZE in 12 16 24 36; do
    echo "  -> NotoSansKR_${SIZE}.h (size ${SIZE}px) ..."
    python3 "$CONVERTER" --compress "NotoSansKR_${SIZE}" "$SIZE" "$FONT_FILE" \
        > "$OUT_DIR/NotoSansKR_${SIZE}.h"
done

echo "Done. Font headers written to $OUT_DIR/"
