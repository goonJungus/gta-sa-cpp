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
- [x] Whole-state map: LS, SF, LV, countryside, desert, mountains and seabed stream; collision ray audit shows 0 holes (w 2)
- [x] Timecycle lighting and weather presets (w 1)
- [x] 2DFX lights and night lamps (w 1)
- [x] Traffic lights with retail phase timing (w 1)
- [x] Custom road signs (CCustomRoadsignMgr port) (w 1)
- [~] Water, particles and sky effects (water.dat water, retail sky polys, sun, moon with phases that rises and sets, regional weather cycle, stars and low clouds; water splashes on entry, swimming and wading and landing dust; no waves or volumetric clouds yet) (w 2)

## Physics and player
- [x] COL2/COL3/COL4 collision: boxes, spheres, triangle walls and floors (w 2)
- [x] On-foot player controller and camera (classic ped zooms; rewrite first person and shoulder views with collision; rewrite camera shake, no producers yet; rewrite foot IK) (w 1)
- [~] Skinned CJ model and IFP animations (default outfit; retail blend system, walk/run/sprint, jumps, falls, landings and get-ups; classic or rewrite handling; ledge climbs, hangs and vaults (CTaskSimpleClimb port); fat/muscle body blend (ini Fat/Muscle, no stat changes yet); no wardrobe yet) (w 3)
- [~] Swimming (CTaskSimpleSwim: water level query, tread, breaststroke, crawl, dive and underwater, climb out; no breath, splash effects or body pitch yet) (w 2)
- [~] Crouch and aim stance (weapon.dat crouch fire loops, torso aim for body-aimed guns, Gun_2_IDLE on release; CTaskSimpleDuck: unarmed duck on C, crouch walk, rolls; aim on right mouse: camera-facing strafe with the gun move clips; rewrite style adds damped aim heading, strafe blend, pitch follow, stationary aim lean, crouch from a run, crouched foot IK; aim arm swung to the camera pitch) (w 1)
- [ ] Drivable vehicles with handling.cfg dynamics (w 4)
- [ ] Peds, traffic and path-node AI (w 4)
- [~] Weapons and combat (13 firearms from weapon.dat: models in hand, Q cycles, LMB fire with ammo and fire anim, R reload with rewrite crossfade, crouch fire and reload; hitscan down the camera ray with tracers, impact dust and sparks, hit sounds, muzzle flash on the shot, weapon move speed; two-bone arm IK for one-handed aim, weapon model frame transforms (sawn-off barrel, flash), sniper clip; trackpad look and click (cursor-delta fallback when the system ignores cursor warps, as on Wayland and VNC); --aimtest shooting range with dummy peds, damage and scoreboard; hold Q or LB for the weapon wheel; fist boxing stance; XInput gamepad; dev free camera on the backtick key; rewrite sprint field-of-view kick; no damage to real targets yet) (w 2)

## Frontend
- [~] Pause menu and settings in the retail layout (display options work; centred layout for any aspect ratio; rewrite mode toggle, gameplay, HUD and field of view, audio sliders; Map page with pan, zoom and waypoint; Stats and Brief pages; Controls list, mouse sensitivity and invert Y, display mode and window size; language page with English, French, German, Italian and Spanish text (extra labels translated); brightness, radar mode, HUD mode, mip mapping and restore defaults; rewrite mode swaps list pages for a flat panel layout with white selection bar and slider bars; stats and map pages restyled) (w 2)
- [~] HUD, radar and map (classic retail HUD layout and a modern rewrite HUD: wide flat radar with a vitals strip on top, fading cash with +/- change, ammo that shows on change, wanted stars, zone name, clock; classic radar from the radar tiles; pause map screen and blips not yet) (w 2)
- [ ] GXT text tables (w 1)

## Audio
- [~] Sound effects (retail SFX bank loader and software mixer on waveOut; gun shots, dry fire and reloads per weapon, footsteps, jumps, pain, menu cues; volume sliders; no vehicles, ambient, speech or radio yet) (w 2)
- [ ] Radio station playback (w 1)

## Scripts and missions
- [ ] SCM virtual machine (opcode dispatch, threads) (w 4)
- [ ] Missions (w 6)
