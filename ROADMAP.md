# Roadmap

The single source of truth for the README progress bar. `[x]` done, `[~]` partly
done (counts half), `[ ]` not started. The number in `(w N)` is a rough effort
weight, so a mission system does not count the same as a texture loader. After
changing anything here, run `python scripts/progress.py` to refresh the README.

## World and rendering
- [x] IMG v2 archive reader (w 1)
- [x] IDE object definitions, text and binary IPL placements (w 1)
- [x] DFF geometry and TXD textures, including parent-TXD chains (w 2)
- [x] Streaming with LOD swaps and HD draw distance (w 2)
- [~] Whole-state map (Los Santos streams today; SF, LV and countryside do not) (w 2)
- [x] Timecycle lighting and weather presets (w 1)
- [x] 2DFX lights and night lamps (w 1)
- [x] Traffic lights with retail phase timing (w 1)
- [x] Custom road signs (CCustomRoadsignMgr port) (w 1)
- [~] Water, particles and sky effects (water.dat water, retail sky polys, sun, moon, stars and low clouds; no waves, volumetric clouds or particles yet) (w 2)

## Physics and player
- [x] COL2/COL3/COL4 collision: boxes, spheres, triangle walls and floors (w 2)
- [x] On-foot player controller and camera (classic ped zooms; rewrite first person and shoulder views with collision) (w 1)
- [~] Skinned CJ model and IFP animations (default outfit; retail blend system, walk/run/sprint, jumps, falls, landings and get-ups; classic or rewrite handling; ledge climbs and vaults; no fat/muscle blend or wardrobe yet) (w 3)
- [ ] Drivable vehicles with handling.cfg dynamics (w 4)
- [ ] Peds, traffic and path-node AI (w 4)
- [ ] Weapons and combat (w 2)

## Frontend
- [~] Pause menu and settings in the retail layout (display options work) (w 2)
- [ ] HUD, radar and map (w 2)
- [ ] GXT text tables (w 1)

## Audio
- [ ] Sound effects (w 2)
- [ ] Radio station playback (w 1)

## Scripts and missions
- [ ] SCM virtual machine (opcode dispatch, threads) (w 4)
- [ ] Missions (w 6)
