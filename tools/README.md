# Local game-data preparation

The repository does not contain the commercial game ISO or copyrighted game
assets. Keep your legally obtained demo/game image on your own device.

## Android / Termux

No third-party Python package is required.

```sh
python tools/prepare_game_data.py "/sdcard/Download/TimeSplitters - Future Perfect (Europe) (Demo).iso"
```

The script validates ISO9660, extracts the ISO filesystem into ignored
`game_data/`, and reports Future Perfect `P5CK` archive entry counts.

For format research:

```sh
python tools/prepare_game_data.py "/sdcard/Download/TimeSplitters - Future Perfect (Europe) (Demo).iso" --extract-p5ck
```

Do not commit `game_data/`.

## Vita demo test

The first Vita milestone is a native data-probe application. It does not execute
the original PS2 MIPS executable. Copy locally extracted demo data to:

```text
ux0:data/TimeSplitters/PAK/CHR.PAK
```

The Vita target checks the P5CK header and directory table. The next milestone is
decoding selected P5CK payloads and bringing up the first rendered asset.
