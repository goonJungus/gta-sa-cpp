# Portable host plan: SDL + OpenGL

Goal: `gta_standalone` runs natively on Windows, Linux and macOS. Today it is
Win32 + Direct3D 9 only (works on Linux via Wine, see scripts/run_wine.sh).

## Decisions
- Window, input, timing, GL context: SDL2 (system package / find_package).
- OpenGL 3.3 core (macOS caps at GL 4.1 core; no compat profile there, so no
  fixed-function GL). The D3D9 fixed pipeline is emulated by one uber shader.
- Own tiny GL function loader via SDL_GL_GetProcAddress. No vendored code.
- D3D9 backend stays for Windows until GL matches it, then becomes optional.
- Textures: keep DXT1/3/5 on GPU (EXT_texture_compression_s3tc; present on
  Linux/Windows/macOS), uncompressed BGRA8 otherwise.

## Coupling measured (host/engine, 12.6k lines)
- D3D types outside the renderer: World.h/.cpp (IDirect3DTexture9*), Sky,
  Fe2d (raw device + state block), TxdLoader (D3DFORMAT), DffLoader, PlayerPed.
- Renderer: D3DRenderer.cpp, 1001 lines, ~100 render/texture-stage state calls.
- Win32 in main.cpp (~18 sites): HWND, GetAsyncKeyState (GameKey), message
  loop, cursor, folder picker, wide-char entry point. Also TxdLoader/Fe2d.

## Phases (each ends with a clean build; Windows D3D9 path never regresses)
1. **Gfx abstraction.** Add `gfx.h`: `Texture` handle, `Mat4`, `Color`,
   `PixelFormat` (BGRA8, DXT1, DXT3, DXT5). Replace IDirect3DTexture9*,
   D3DMATRIX, D3DCOLOR, D3DFORMAT in World, Sky, Fe2d, TxdLoader, DffLoader,
   PlayerPed. Move Fe2d to renderer-owned draw calls (no raw device).
   Define `IRenderer` = current D3DRenderer public API; D3D9 backend implements it.
2. **Platform layer.** `platform.h`: window, events, key/mouse state (SDL
   scancode table replaces VK_*), cursor capture, time, folder picker,
   UTF-8 `main`. Backed by Win32 now, SDL later (SDL on all three once GL lands).
3. **GL backend.** `GlRenderer`: VAO/VBO meshes (MeshVertex layout as-is),
   uber shader (per-vertex 2-light + ambient, linear fog, texture stage
   modulate/TFACTOR, alpha test, additive/alpha blend modes, vertex colour
   source), sky dome, coronas, debug lines, screenshot, vsync, wireframe.
   Render-state cache to keep draw calls cheap (3k+ draws/frame).
4. **Build.** CMake `GTA_BACKEND=d3d9|gl` (default gl off Windows, d3d9 on
   Windows until parity). Linux/macOS: find_package(SDL2). Drop WIN32-only gate.
5. **Parity check.** Same `--pos/--yaw/--frames/--screenshot` shot on D3D9
   (Wine) and GL; compare images by eye + per-channel diff; fix shader gaps.
6. **File access.** Game data paths are case-insensitive on Windows/Wine but
   not on Linux ext4/macOS APFS-cs. Add a case-insensitive path resolver in
   the loaders (IMG/IDE/IPL/COL/IFP/data files) with a directory cache.
7. **CI.** GitHub Actions: Windows (MSVC), Linux, macOS compile; no assets in CI.

## Risks
- Fixed-function lighting/fog subtleties (vertex colour * lighting, TFACTOR
  tint, `sqrt` trick on blended draws): expect a tuning pass in phase 5.
- macOS cannot be tested from this Linux box: rely on CI compile + user report.
- Library `gta_sa_module` stays renderer-free; nothing here changes its API.
