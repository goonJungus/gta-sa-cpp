# gta-sa-cpp

C++ port of Grand Theft Auto: San Andreas, in progress. Classes are ported from
a full decompilation, organized the way the original game organizes them, and
verified against the decomp as they land.

## Code only — bring your own copy

This repository contains **only code**. No game assets, no game data, no audio,
no textures, no models, and no files extracted from the retail game are
distributed here.

To build or run anything from this code you must provide **your own legally
obtained copy** of GTA: San Andreas (PC). Asset loading from your copy is not
implemented yet; when it is, the build will point at your install and nothing
else.

## Status

Ported so far: core math/containers, world entities
(`CPlaceable` → `CEntity` → `CPhysical` → `CObject`, plus `CBuilding` and
`CWorld`), `CRenderer`, `CWeather`, `CShadows`, `CFont`, `CSprite2d`,
`CPostEffects`, collision (`CColModel` / `CCollision`), and audio (`CAESound`).

`BUILD_NOTES.md` is the running log: what's verified, what's stubbed, and what
broke. `port_plan/` is the map — class inventory, dependencies, and the build
order contributors should follow.

## Building

Windows, MSVC, CMake:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Debug
```

Win32 (x86) matches the original binary. Some subsystems compile-check clean
individually before the full tree links — see `BUILD_NOTES.md`.

## Contributing

1. Pick a class from `port_plan/` (check `layering.json` for build order and
   `deps.tsv` for dependencies).
2. Port it, verify behavior against the decomp, mark stubs honestly.
3. Append what you did to `BUILD_NOTES.md` — the log is the project's memory.
