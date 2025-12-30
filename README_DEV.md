# FreeClimbVR v1.0 - Source Code

## Overview
This is the cleaned and finalized source code for FreeClimbVR (v2.7/3.0). It includes the complete physical climbing system with haptics, interaction filtering, and a newly added Sound System.

## Key Changes in v1.0
- **Translation**: All source code comments have been translated from Portuguese to English.
- **Cleanup**: Debug logging and dead code (Legacy "Wall Push-Off") have been removed from `OnFrame.cpp` and `Utils.cpp`.
- **Sound System**: Implemented `Sound::PlayClimbSound` in `src/Sound.cpp`. It attempts to identify surface material (Wood, Stone, Metal, Snow) based on the object's Name or Type and plays the appropriate Footstep sound effect.
- **Physics Fixes**:
    - **Layer 56 Block**: Explicitly blocks Havok Layer 56 to prevent climbing on "Weapon Throw VR" shield throw physics meshes.
    - **Motion Smoothing**: Implemented Low-Pass Filter on climbing velocity to prevent camera jitter. Configurable via `fMotionSmoothing`.
    - **Fall Damage**: Fixed "Accumulated Damage" bug. Added `bDisableFallDamage` option for immortality.

## Structure
- `src/`: Source files (`OnFrame.cpp` contains the core logic).
- `include/`: Headers.
- `Settings.ini`: Configuration file with new options (`fMotionSmoothing`, `bDisableFallDamage`).

## Building
1. Required: CMake, Visual Studio 2022 (MSVC), VCPKG.
2. Open folder in VS Code or Visual Studio.
3. Configure CMake with `build-release-msvc`.
4. Build target `FreeClimbVR`.

## Known Issues / TODO
- Material detection is heuristic (Name check). A proper Physics Material lookup via SKSE could be more robust.
- "Wall Push-Off" is disabled because it causes drift, but code remains if you want to re-enable it (check git history or see commented sections in v2.6).

Good luck!
