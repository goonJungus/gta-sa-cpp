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
- [~] Skinned CJ model and IFP animations (default outfit; retail blend system, walk/run/sprint, jumps, falls, landings and get-ups; classic or rewrite handling; ledge climbs, hangs and vaults (CTaskSimpleClimb port); fat/muscle body blend from the fat and muscle stats, which exercise changes as in CStats::UpdateFatAndMuscleStats (ar_stats.dat rates and limits; saved to the ini); no wardrobe yet) (w 3)
- [~] Swimming (CTaskSimpleSwim: water level query, tread, breaststroke, crawl, dive and underwater, climb out; breath meter, splash effects and sounds; no body pitch yet) (w 2)
- [~] Crouch and aim stance (weapon.dat crouch fire loops, torso aim for body-aimed guns, Gun_2_IDLE on release; CTaskSimpleDuck: unarmed duck on C, crouch walk, rolls; aim on right mouse: camera-facing strafe with the gun move clips; rewrite style adds damped aim heading, strafe blend, pitch follow, stationary aim lean, crouch from a run, crouched foot IK; aim arm swung to the camera pitch) (w 1)
- [~] Drivable vehicles with handling.cfg dynamics (vehicles.ide and handling.cfg parsed; car models load from the DFF frame tree with body parts and wheels; dev key B spawns the next car ahead, F gets in and out, W S A D drive with arcade dynamics from the handling line, wall blocking, steering wheels; paint from carcols.dat; shots damage cars: smoke under 250 health, fire and an explosion at 0, burnt black wreck; no suspension or collisions between cars yet) (w 4)
- [~] Peds, traffic and path-node AI (peds.ide and animgrp.dat parsed; any of 208 civilian models loads as its own skinned ped with texture and its group walk and idle clips; dev key K spawns one that wanders and turns at walls; ambient crowd of 12 around CJ, key O toggles; peds avoid walls, cars and each other, watch CJ up close, run from an aimed gun, and die from hitscan shots (head shot kills at once) and lie for 25 s; no path nodes or traffic yet) (w 4)
- [~] Weapons and combat (13 firearms from weapon.dat: models in hand, Q cycles, LMB fire with ammo and fire anim, R reload with rewrite crossfade, crouch fire and reload; hitscan down the camera ray with tracers, impact dust and sparks, hit sounds, muzzle flash on the shot, weapon move speed; fire rate from the weapon.dat anim loop (end - start per shot, loop wraps with carry-over), CJ starts at poor skill like a new game (ini Player/WeaponSkill, GTA_SKILL), poor fire clips; two-bone arm IK for one-handed aim, weapon model frame transforms (sawn-off barrel, flash), sniper clip; trackpad look and click (cursor-delta fallback when the system ignores cursor warps, as on Wayland and VNC); --aimtest shooting range with dummy peds, damage and scoreboard; hold Q or LB for the weapon wheel; fist boxing stance; XInput gamepad; dev free camera on the backtick key; rewrite sprint field-of-view kick; FireInstantHit ballistics: aim scatter from accuracy and attack counter, shotgun pellets, point-blank 150, pedstats.dat defend weakness, gun force-death rule) (w 2)

## Frontend
- [~] Pause menu and settings in the retail layout (display options work; centred layout for any aspect ratio; rewrite mode toggle, gameplay, HUD and field of view, audio sliders; Map page with pan, zoom and waypoint; Stats and Brief pages; Controls list, mouse sensitivity and invert Y, display mode and window size; language page with English, French, German, Italian and Spanish text (extra labels translated); brightness, radar mode, HUD mode, map legend, mip mapping, anti aliasing (off, 2x, 4x) and restore defaults; rewrite mode swaps list pages for a flat panel layout with white selection bar and slider bars; stats and map pages restyled) (w 2)
- [~] HUD, radar and map (classic retail HUD layout and a modern rewrite HUD: wide flat radar with a vitals strip on top, fading cash with +/- change, ammo that shows on change, wanted stars, zone name, clock; classic radar from the radar tiles; pause map screen and blips not yet) (w 2)
- [x] GXT text tables (w 1)

## Audio
- [~] Sound effects (retail SFX bank loader and software mixer on waveOut; gun shots, dry fire and reloads per weapon, footsteps, jumps, pain, menu cues; water splash and landing sounds from the retail swimming and feet banks; volume sliders; player car engine from the retail vehicle banks; ped speech from the retail CAEPedSpeechAudioEntity tables (context, voice and bank lookups read from gta_sa.exe): CJ pulls a gun on a ped, peds run from a gun, repeat times per context) (w 2)
- [~] Optional active-ragdoll plugin (rewrite style only): a separately licensed physics DLL is loaded at run time when [Delirium] Dll / Data are set in the ini; absent, the engine ragdoll stays. Loader and exports bound; ped hookup (skeleton mapping, shot and fall tasks) not done (w 1)
- [~] Radio station playback (11 retail stations from the scrambled Ogg Vorbis stream packs, decoded by our own Vorbis decoder and streamed through the mixer; Q and E change station while driving; retail programming: CAERadioTrackManager tables from gta_sa.exe pick idents, adverts, DJ banter (first, time-of-day, general) and songs with their DJ intros and outros, with play histories) (w 1)

## Scripts and missions
- [~] SCM virtual machine (main.scm header chunks parsed; every thread runs the core language: variables, arrays, arithmetic, comparisons, AND/OR, jumps, subroutines, waits; game commands are skipped by argument count (662 opcodes settled against the compiled script, scripts/scm_args.py) and listed in the log; CREATE_PLAYER, coordinates and SET_TIME_OF_DAY are wired, GTA_SCM=1 runs the script in game and places CJ at the retail start) (w 4)
- [~] Weapon anim groups: move set follows the weapon (armed for long guns, rocket, chainsaw; bat uses the player set), swapped in place when the weapon changes; no rocket / chainsaw / bat weapons exist yet to use them (w 1)
- [~] Gun aim: arm guns use the retail IK point-arm chain (IKChain_c sweep, effector 0.05 m along the hand, no bone limits as retail ships them, 250 ms blend) toward the crosshair ray (Find3rdPersonCamTargetVector) with RotateTorsoForArm, others the torso (ms_torsoInfo limits) with the heading turned to the crosshair; classic crosshair at 0.53 / 0.4 sized by GetWeaponRadiusOnScreen; classic aim camera (Process_AimWeapon free aim: weapon FOV, look-at offsets, 1.0 + 1.6 cos(pitch) distance, pitch limits) and pro-skill twin pistols (left gun at the left hand, left arm chain, 2guns_crouchfire) done; lock-on targeting and the camera transition curve still simplified (w 1)
- [~] Missions (mission table of main.scm read; the VM launches a mission script as its own thread with its own jump base, and START_NEW_SCRIPT passes locals; mission 1 runs 410 instructions before its first game command; nothing is playable yet) (w 6)
