# TimeSplitters — PSVITA Port

Native PSVITA port/reimplementation work for TimeSplitters: Future Perfect.

## Status

The repository now contains a minimal VitaSDK boot target. The long-term goal is a native ARM/Vita implementation of the game engine which loads assets supplied from a legally owned copy of the original game.

Current milestone:
- VitaSDK/CMake project
- Native Vita executable target
- VPK generation
- P5CK/Pak asset pipeline: pending
- Future Perfect asset readers: pending
- Renderer: pending
- Audio: pending
- Input/gameplay: pending
- First playable level: pending

## Build

Install a current VitaSDK and export VITASDK.

Run from the repository root:

    cmake -S . -B build
    cmake --build build -j

The resulting VPK is generated in the build directory.

## Game data

This repository does not contain copyrighted Future Perfect game assets or an ISO. Use assets from a legally owned copy locally.

The OpenRadical tspak utility supports the Future Perfect P5CK archive format and will be used as an external extraction/reference tool while the native Vita asset pipeline is developed.

## Architecture

Original PS2 data -> P5CK/asset readers -> Vita-compatible runtime -> Vita ARM -> VPK

The target is a native port/reimplementation, not execution of the original PS2 MIPS executable through an emulator.

## Legal

Keep commercial game images and copyrighted game assets out of this repository.
