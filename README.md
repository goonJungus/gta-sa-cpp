# gta-sa-cpp

**GTA: San Andreas, ported to modern C++20 one system at a time.**

San Andreas is twenty-plus years old and still runs on a 32-bit engine that was
built for a PlayStation 2. This project rebuilds it as clean, portable C++20 that
reads your own copy of the game and draws Los Santos with code written from
scratch. It is a port, not a mod: no injected DLLs, no patched executable, and no
hooks into the original binary.

You can already walk Grove Street at sunset, watch the traffic lights cycle on
the retail timer, read the green highway signs, and bump into every wall the
original game knows about.

## Status

<!-- progress:start -->
```
████████████░░░░░░░░  60%
```
**12 done, 12 in progress, 1 to go** across 25 milestones, weighted by effort (see [ROADMAP.md](ROADMAP.md)).
<!-- progress:end -->

The world is in place and you can explore it on foot. The game parts still to
come are the people, cars, guns, radio and missions.

## What works today

**The world**
- Los Santos streams in around you from the original `.img` archives, with LOD
  models swapping to full detail as you get close and an adjustable draw distance.
- Full texture support, including the parent-texture chains SA uses to share
  textures across districts.
- Timecycle lighting: dawn, dusk, night and weather presets interpolate the way
  they did in 2004, and street lamps switch on after dark.
- Water from `water.dat`, drawn the way `CWaterLevel` does it: two scrolling
  layers of the game's water texture in the timecycle's water colour, with the
  open sea running out past the edge of the map.
- The sky is built like `CClouds`: the timecycle gradient polys, the sun on its
  retail path, and the moon, stars and low clouds.
- Traffic lights run on the retail phase timer.
- Road signs are rebuilt from their 2DFX records with the original sign font, so
  "↑ Glen Park / ↑ Jefferson / ← Los Santos" appears where it should.

**Collision**
- Every COL2, COL3 and COL4 model is read: oriented boxes, spheres, steep
  triangles as walls and flat triangles as floors.
- An audit of all 2,272 placed models in the loaded area finds only one without
  collision, and that is a LOD stand-in that never needs it.

**Playing**
- An on-foot controller with running, jumping, step-up and a first or
  third-person camera.
- CJ himself, assembled from his clothing parts the way the game's clothes
  builder does it, skinned on the base player skeleton and animated with the
  original `ped.ifp` clips: idle, walk, run, sprint, jump and fall, blended and
  played at the rate that matches his speed.
- A pause menu rebuilt from the game's own screen table at 640×448, drawn with
  the original fonts and backgrounds. The frame limiter (60, 120, 144 or
  unlimited), VSync and draw distance work and are saved between runs.

**Developer tools**
- Collision view, wireframe, noclip, a time-of-day scrubber, weather cycling and
  saved positions, all on the F-keys.
- Headless mode for scripted screenshots, walk tests and collision probes, so you
  can check a change without picking up a controller.

## No game assets

This repository contains **source code only**. It ships no models, textures,
audio, scripts or text from San Andreas, and it never will. To run it you need
your own legal copy of **GTA: San Andreas (PC, v1.0)**; the engine reads the
original files from that install at runtime.

## Layout

```
gta-sa-cpp/
├── include/gta_sa/   Public API of the portable library (namespace gta)
├── src/              Library implementation: world, peds, vehicles, audio, scripts
├── host/engine/      gta_standalone: renderer, player, camera, input, collision
├── scripts/          progress.py refreshes the progress bar from ROADMAP.md
└── ROADMAP.md        Every milestone with its status
```

The project is split in two on purpose:

- **`gta_sa_module`** is a static library. It reads the archives, map data and
  game state, and has no `main()`, no renderer, no `<windows.h>` and no
  dependencies outside the C++ standard library. It builds on Windows, Linux and
  macOS.
- **`gta_standalone`** is the playable host built on that library. It owns the
  renderer, player, camera, input and collision. Direct3D 9 and Win32 live only
  here, which keeps the library free to be driven by a different renderer.

## Building

You need CMake 3.20 or newer and a C++20 compiler (MSVC 2022, GCC 11+ or
Clang 14+). Builds are 64-bit, and a Release build compiles with zero warnings.

```sh
cmake -S . -B build
cmake --build build --config Release
```

On Linux and macOS this builds the library on its own. On Windows it also builds
`gta_standalone`.

## Running

Just run it:

```sh
build\Release\gta_standalone --play
```

The first time, it looks for your San Andreas install. It checks the program's
own folder (so you can drop the build into your game folder), then the usual
Steam and Rockstar locations. If it still can't find the game, it opens a folder
picker. The choice is saved in `gtasa_cpp.ini` next to the program, so you only
do this once. You can also point it somewhere explicitly with
`--game "D:\Games\GTA San Andreas"` or the `GTA_SA_DIR` environment variable.

Useful flags:

| Flag | What it does |
|---|---|
| `--game DIR` | Use the San Andreas install in DIR |
| `--play` | Spawn on foot instead of in the free camera |
| `--pos x,y,z` / `--yaw deg` | Start somewhere specific |
| `--hour h` / `--weather NAME` | Set the time of day and weather (e.g. `SUNNY_LA`) |
| `--window WxH` / `--res WxH` | Window size and render resolution |
| `--frames N --screenshot out.bmp` | Run N frames headless, then save a screenshot |
| `--third` | Start in the third-person camera |
| `--walk [--gait walk\|run\|sprint\|jump]` | Headless: keep moving forward (and jumping) |
| `--probe-collision` | Print a solidity map and the owner of every nearby collision box |

**Controls:** WASD to move (CJ runs by default), Shift to sprint, Alt to walk, Space to jump, the mouse to look, V to
switch camera, and Esc or P for the pause menu, which also lists the F1–F12
developer keys.

## Contributing

Pull requests are welcome, from one-line fixes to whole subsystems.

- Pick an open item from [ROADMAP.md](ROADMAP.md), or open an issue for a bug you
  can reproduce. A position and a screenshot help a lot.
- Follow the house style: `namespace gta`, snake_case file names in the library,
  `m_` prefixes for members, and standard C++20 with no compiler extensions.
- Keep the library portable. Platform and graphics code belongs in `host/`.
- A Release build must stay at zero warnings.
- Never commit game files or anything extracted from them.
- When a milestone changes, update `ROADMAP.md` and run
  `python scripts/progress.py` so the progress bar stays accurate.

## Community

Come hang out, share screenshots or help out on Discord:
**https://discord.gg/7hvNqZDZdW**

## Credits

- **Rockstar North**, for making San Andreas.
- The GTA modding community, whose decades of format documentation (IMG, DFF,
  TXD, COL, IPL and IDE) made these readers possible.
- Everyone who has filed a bug, tested a build or sent a screenshot of an
  invisible wall.

## License

The source code is released under the [MIT License](LICENSE).

Grand Theft Auto and San Andreas are trademarks of Take-Two Interactive. This
project is not affiliated with or endorsed by Rockstar Games or Take-Two
Interactive.
