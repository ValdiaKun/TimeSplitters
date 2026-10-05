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
- Future Perfect model/geometry metadata and VIF packet decoding are implemented
- VIF memory unpacking now models masks and addition-decompression state
- The Vita runtime can inspect the PS2 ELF `.vutext` and locate the `MSCAL 0x683` VU1 program
- A native VU1 interpreter core now covers the main arithmetic/conversion/integer/memory/control operations and captures complete `XGKICK` GIF packets
- The Vita runtime now traverses all validated CHR submeshes, executes each VIF/VU1 display list, converts GIF triangle/strip/fan/sprite output into Vita triangles, and renders the assembled model preview
- The VIF decoder now honors the UNPACK mask bit, cycle-slot column filling, and PS2 row-addition/difference semantics
- The VU core now implements CLIP flag generation used by PS2 microprogram control flow
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
    ux0:data/TimeSplitters/SLED_530.66

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
3. Vita-side data probe — **working**
4. Future Perfect VIF/VU1 ingestion — **working for the validated CHR model path**
5. Bring up a first 3D scene from the native VU/GIF path — **working model viewer**
6. Implement camera, player/input, collision and level logic
7. Add audio/video systems
8. Reach a playable demo slice
9. Expand toward the full game

## Legal/data boundary

Do not commit the commercial game ISO, extracted copyrighted assets, or other
redistributable game data to this repository. Use locally supplied data from a
copy you are legally entitled to use.


## Automated reverse-engineering pipeline

The repository now provides a single Termux entry point:

```sh
cd ~/TimeSplitters
bash tools/termux_analyze_demo.sh
```

The pipeline updates the tools, locates the local demo ISO, validates/extracts the ISO, scans every P5CK archive, writes a local JSON analysis report, finds optional `.c2n` checksum maps, and extracts a bounded set of Future Perfect resource records from `L_2_ST.PAK`. Generated game data and reports remain outside Git.

External format research confirms that P5CK is the Future Perfect PAK format and that its four-word directory record is CRC, offset, length, and a fourth field used by existing tooling as the compressed/GZip length. The project keeps the commercial/demo assets out of the repository and only commits the reverse-engineering tools and native Vita code.


<!-- Vita final gate -->
 




 
<!-- OPMSUB final validation gate -->
