#!/data/data/com.termux/files/usr/bin/bash
set -e
cd "$(dirname "$0")/.."
echo "== Updating TimeSplitters tools =="
git pull --ff-only

DOWNLOAD="/sdcard/Download"
ISO=""
for candidate in "$DOWNLOAD"/*.iso "$DOWNLOAD"/*.ISO; do
  [ -f "$candidate" ] || continue
  ISO="$candidate"
  case "$(basename "$candidate")" in
    *TimeSplitters*|*timesplitters*) break ;;
  esac
done

if [ -z "$ISO" ]; then
  echo "No ISO found in $DOWNLOAD"
  exit 1
fi

echo "== ISO: $ISO =="
python tools/prepare_game_data.py "$ISO"

echo
echo "== Scanning all PAK resources =="
python tools/analyze_all_paks.py game_data --out game_data/p5ck_analysis.json
python tools/extract_fp_resources.py game_data/PAK/STORY/L_2_ST.PAK --out game_data/analysis/L2ST_fp_resources

echo
echo "== Looking for checksum/name maps =="
find game_data -type f \( -iname "*.c2n" -o -iname "*.C2N" \) -print

echo
echo "== Extracting only small metadata from L_2_ST =="
python tools/inspect_p5ck.py game_data/PAK/STORY/L_2_ST.PAK --start 0 --count 30

echo
echo "== Pipeline complete =="
echo "Local reports:"
echo "  game_data/p5ck_analysis.json"
echo "Game data and reports are ignored by Git."
