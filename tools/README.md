# Local game-data preparation

This directory contains development tooling only. Do not add the commercial
game ISO, extracted game assets, or other copyrighted game data to Git.

## Prepare a local copy

1. Obtain an ISO from a copy of the game you are legally entitled to use.
2. Install Python 3.9+.
3. Install the helper dependency:

    python -m pip install -r tools/requirements.txt

4. Run:

    python tools/prepare_game_data.py "/path/to/TimeSplitters - Future Perfect.iso"

The script prints the ISO SHA-256, extracts the disc filesystem into the ignored
game_data directory, and reports any P5CK/PAK archives it finds.

## Repository boundary

The repository contains source code and tools for the port/reimplementation.
The original game's ISO and copyrighted assets remain local to the developer.

The next asset-pipeline step is to add readers for the Future Perfect P5CK
format and convert only the data needed by the native Vita runtime.
