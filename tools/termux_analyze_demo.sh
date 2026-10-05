#!/data/data/com.termux/files/usr/bin/bash
set -e
cd "$(dirname "$0")/.."
git pull --ff-only
ISO="/sdcard/Download/TimeSplitters - Future Perfect (Europe) (Demo).iso"
if [ ! -f "$ISO" ]; then
  echo "ISO not found: $ISO"
  exit 1
fi
python tools/prepare_game_data.py "$ISO"
echo
echo "=== C2N files ==="
find game_data -type f \( -iname "*.c2n" -o -iname "*.C2N" \) -print
echo
echo "=== L_2_ST entries 0-19 ==="
python tools/inspect_p5ck.py game_data/PAK/STORY/L_2_ST.PAK --start 0 --count 20
echo
echo "=== Next steps ==="
echo "Run this script again after repository updates; it will fast-forward and repeat the analysis."
