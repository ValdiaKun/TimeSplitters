# TimeSplitters — PSVITA Port

Native PSVITA port/reimplementation work for TimeSplitters: Future Perfect.

## Current status

The first target is the **Europe demo** so we can validate the porting architecture
before tackling the full release.

Verified from the demo image:
- Valid ISO9660 filesystem
- PS2 ELF boot executable: `SLED_530.66`
- `SYSTEM.CNF` boots `cdrom0:\\SLED_530.66;1`
- Future Perfect `P5CK` archives are present
- Demo includes the story archive `PAK/STORY/L_2_ST.PAK`
- Native C P5CK header/directory parser is now part of this repository
- Android/Termux extraction requires no third-party Python package

The original PS2 MIPS executable is **not** copied into the Vita build and is not
executed by the Vita target. The project is a native reimplementation/port.

## Android / Termux

Keep the legally obtained demo ISO on your Android device. From the repository:

    python tools/prepare_game_data.py "/sdcard/Download/TimeSplitters - Future Perfect (Europe) (Demo).iso"

This creates the ignored `game_data/` directory and reports every P5CK archive
and its directory-table size.

## Vita data-probe milestone

Copy the locally extracted demo data to the Vita, at minimum:

    ux0:data/TimeSplitters/PAK/CHR.PAK

Build with VitaSDK:

    cmake -S . -B build
    cmake --build build -j

The resulting VPK contains a native test program that opens `CHR.PAK`, validates
the `P5CK` header and directory table, and renders a visual status screen.
Press START to exit.

## Architecture

    legally owned demo ISO
            |
            v
      ISO9660 extractor
            |
            v
       P5CK archives
            |
            v
     native asset readers
            |
            v
      Vita renderer/input
            |
            v
        playable demo

## Roadmap

1. ISO/P5CK pipeline — **working**
2. Native P5CK reader — **working**
3. Vita-side data probe — **implemented**
4. Decode selected demo asset types
5. Bring up a first 3D scene
6. Implement camera, player/input, collision and level logic
7. Add audio/video systems
8. Reach a playable demo slice
9. Expand toward the full game

## Legal/data boundary

Do not commit the commercial game ISO, extracted copyrighted assets, or other
redistributable game data to this repository. Use locally supplied data from a
copy you are legally entitled to use.
