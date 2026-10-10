# Contributing to gta-sa-cpp

## Picking up work

1. Open `port_plan/` — it's the map for the whole project.
   - `layering.json` — the build order. Work top-down; don't port a class
     whose dependencies aren't ported yet.
   - `deps.tsv` — what each class depends on.
   - `subsystems.tsv` / `classes/` — what's ported, what's stubbed.
2. Claim a class by opening an issue or saying so in the Discord — two people
   porting the same class is wasted work.
3. Port it: header in `include/`, methods in `src/`, following the existing
   naming and layout conventions.

## Porting standard

- **Verify against the decomp.** Every method body should be checked against
  the decompiled original before it's marked done. Behavior first, then
  cleanup.
- **Mark stubs honestly.** If a method isn't verified, leave a `TODO` saying
  what it needs. A wrong method is worse than a stub.
- **Keep sizes right.** `static_assert(sizeof(...))` where the layout is
  certain (guard 32-bit-only asserts with `#if INTPTR_MAX == INT32_MAX`).
- **No RenderWare SDK.** RW types are forward-declared in `RenderTypes.h`
  until the real RW layer lands. Don't pull in external RW headers.
- **C++20, MSVC-clean.** `/W3 /permissive-`, zero warnings. If MSVC rejects
  something GCC accepts (or vice versa), MSVC wins — it's the tested
  compiler.
- **Win32 (x86) is the target.** The original binary is 32-bit; pointer sizes
  and struct layouts assume it.

## Logging

Every change gets an entry in `BUILD_NOTES.md`:
- What you ported or fixed
- What's verified vs what's still stubbed (and why)
- Anything surprising you found in the decomp

The log is the project's memory. Future contributors read it before touching
your code.

## What not to submit

- Decompiled Rockstar code pasted verbatim — port it, don't copy it.
- Game assets of any kind (models, textures, audio, scripts, data files).
- Build output (`build/`), zips, or transfer artifacts.
- Changes to `port_plan/` structure without discussion — it's the shared map.

## Pull requests

- One class (or one coherent subsystem) per PR.
- PR description says: what was ported, what was verified against the decomp,
  what's still TODO.
- Must build clean: `cmake -B build -G "Visual Studio 17 2022" -A Win32`
  then `cmake --build build --config Debug`, zero errors, zero warnings.
