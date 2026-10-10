# gta-sa-cpp

C++ port of Grand Theft Auto: San Andreas, in progress. Classes are ported from
a full decompilation, organized the way the original game organizes them, and
verified against the decomp as they land.

> **Goal:** a compilable C++ codebase that mirrors the original game's
> structure — every class ported, every method verified.

## Code only — bring your own copy

This repository contains **only code**. No game assets, no game data, no audio,
no textures, no models, and no files extracted from the retail game are
distributed here, and none are needed to build the codebase.

To run anything built from this code you must provide **your own legally
obtained copy** of GTA: San Andreas (PC, 1.0 US). Asset loading from your copy
is not implemented yet; when it is, the build will point at your install and
nothing else.

## Status

The full `gta_sa` static library builds with **zero errors** (13.3 MB lib).

**Ported:** core math/containers, collision (`CColModel` / `CCollision`),
world entities (`CPlaceable` → `CEntity` → `CPhysical` → `CObject`, `CBuilding`,
`CWorld`), camera, renderer, post effects, shadows, weather, font, sprites,
model info, animation, vehicles, weapons, fire/explosion, audio engine, fx,
file manager, timer, pad input, peds, player, HUD, streaming, and the SCM
script VM (84 opcodes so far).

**Still TODO:** the real RenderWare layer (`RenderTypes.h` is a stand-in),
`CGame`, full audio entities/enums, the remaining 26 SCM opcode dispatch
blocks, and many method bodies marked TODO. Stubs are honest — `BUILD_NOTES.md`
says what's verified vs stubbed, and it's the project's running log.

`port_plan/` is the map: class inventory, dependencies (`deps.tsv`), subsystem
breakdown (`subsystems.tsv`), and the build order (`layering.json`).

## Repository layout

```
include/        # Class headers, organized like the original game
src/            # Method implementations
port_plan/      # Contributor map: classes, deps, subsystems, build order
CMakeLists.txt  # Static lib (gta_sa) + standalone D3D9 exe (gtasa_cpp)
BUILD_NOTES.md  # Running port log — read this first
```

## Tools used

- **Ghidra** — the decompilation the port is verified against (separate
  workflow, not in this repo)
- **CMake 3.20+** — build configuration
- **MSVC / Visual Studio 2022** — the only tested compiler, Win32 (x86) target
  to match the original binary
- **Git** — version control

No package manager, no external dependencies. The static lib needs nothing but
a compiler; the `gtasa_cpp` exe links system D3D9.

## Building

Windows, PowerShell:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Debug
```

Notes:
- C++20, `/W3 /permissive-`. MSVC only — GCC/Clang untested.
- `CPed.cpp` needs `/Zm8000` (compiler heap); the CMake file sets it via the
  `CL` environment variable automatically. If you compile files manually, set
  `$env:CL="/Zm8000"` first.
- `build/` is git-ignored. Out-of-source builds only.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Short version: pick a class from
`port_plan/`, port it, verify against the decomp, log it in `BUILD_NOTES.md`.

## Credits & references

This project stands on the shoulders of the GTA modding and
reverse-engineering community:

- **[gta-reversed](https://github.com/gta-reversed/gta-reversed)** — many
  headers in `include/` are adapted from here (plugin-sdk specifics stripped).
  The authority on SA class layouts.
- **[re3 / reVC](https://github.com/GTAmodding/re3)** — clean-room reversals
  of GTA III/VC. Collision layouts and load-time behavior reference.
- **[librw](https://github.com/aap/librw)** (aap) — open-source RenderWare
  implementation. DFF/TXD format reference.
- **[openrw](https://github.com/rwengine/openrw)** — RenderWare format
  insights.
- **[SanAndreasUnity](https://github.com/in0finite/SanAndreasUnity)** —
  engine behavior reference.
- **[opensa](https://github.com/nvwrist/opensa)** — format and streaming
  reference.
- **[jte/GTASA](https://github.com/jte/GTASA)** — clean-room C++ SA
  reimplementation. Struct layouts, sector formulas.
- **[GTA-San-AnSkateas](https://github.com/ryglizzy/GTA-San-AnSkateas)**
  (ryglizzy) — triangle-soup collision approach.
- **[ariane](https://gtastuff.com)** (Dryxio) — binary IPL research,
  map-editor tooling, reverse-engineering workflow.

All port code in this repository is written for this project. No Rockstar
Games assets are included.

## Legal

This project does not contain, download, or redistribute any Rockstar Games
assets or game data. You must provide your own legal copy of GTA San Andreas.
Ported from a decompilation done for interoperability research.
