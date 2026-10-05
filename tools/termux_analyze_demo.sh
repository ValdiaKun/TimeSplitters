#!/data/data/com.termux/files/usr/bin/bash
set -e

cd "$(dirname "$0")/.."
echo "== Updating TimeSplitters port tools =="
git pull --ff-only

echo
echo "== Looking for Future Perfect ISO =="
DOWNLOAD="/sdcard/Download"
ISO=""
for candidate in "$DOWNLOAD"/*.iso "$DOWNLOAD"/*.ISO; do
  if [ -f "$candidate" ]; then
    case "$(basename "$candidate")" in
      *"TimeSplitters"*|"*" ) ISO="$candidate"; break ;;
    esac
  fi
done

if [ -z "$ISO" ]; then
  echo "No ISO found in $DOWNLOAD"
  echo "Put your legally obtained TimeSplitters: Future Perfect demo ISO in Downloads and run this script again."
  exit 1
fi

echo "ISO: $ISO"
echo
echo "== Extracting and validating game data =="
python tools/prepare_game_data.py "$ISO"

echo
echo "== Searching for C2N name maps =="
find game_data -type f \( -iname "*.c2n" -o -iname "*.C2N" \) -print

echo
echo "== Inspecting L_2_ST entries 0-19 =="
python tools/inspect_p5ck.py game_data/PAK/STORY/L_2_ST.PAK --start 0 --count 20

echo
echo "== Analysis complete =="
echo "Game data remains local and is not committed to Git."
