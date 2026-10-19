# Roadmap

The single source of truth for the README progress bar. `[x]` done, `[~]` partly
done (counts half), `[ ]` not started. The number in `(w N)` is a rough effort
weight, so a mission system does not count the same as a texture loader. After
changing anything here, run `python scripts/progress.py` to refresh the README.

This file lists every gameplay system and feature of the retail game, done and
not done, so the bar shows the real distance to a finished port. The first
sections are the systems built so far (each still lists what is missing in it);
the sections from "Gap list" down are systems not yet started or only started.

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
- [~] Physics props (object.dat parsed; every placement with a mass under 99999 and no uproot limit becomes a rigid box: gravity, air drag, ground and wall contacts, bounce from the elasticity, friction, sleep; the player shoves them, cars plough through them, bullets knock them about and smash cardboard boxes, explosions throw them and set off barrels; lamp posts, doors and fences stay fixed; no toppling or breakable parts yet) (w 2)
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
- [~] Melee weapons (melee.dat combos and the weapon.dat melee rows: bat, golf club, knife, katana, chainsaw, shovel, pool cue, nightstick, knuckles, dildos, flowers, cane in hand; CTaskSimpleFight: attack 1-3 with the chain frame, running attack, per-weapon anim group clips from anim.img as partial overlays, strike sphere at the hit frame with level offsets, damage x fat/muscle modifier, victim defend weakness, katana and force-death rules, ped hit reactions from the group, fight styles UNARMED_1..4 via ini Player/FightStyle; blocking, ground attacks, pistol whip, kick, objects and vehicles not ported) (w 2)
- [~] Thrown weapons (grenade, tear gas, molotov, satchel charge, detonator: CTaskSimpleThrowProjectile start_throw then throw/throwu clips from the grenade block, fire frames from weapon.dat, force from the button hold, wall-block drop, AddProjectile velocities, gravity bounce, 2 s fuse, molotov breaks on impact, tear gas cloud makes peds flee, satchel waits for the detonator and hands it over, the spent detonator is dropped and the weapon switches down a slot (ProcessWeaponSwitch), molotov leaves a burning patch for 5-10 s, ped/player blast damage; vehicle and object damage, creeping fire spread, choking anim not ported) (w 2)
- [~] Heavy weapons (rocket launcher and heat-seeking RPG: RocketFire clip on the rocket move set, rocket leaves 1 m ahead along the aim at 20 / 10 m/s, +20 m/s^2 along the nose, explodes on impact or after 3 s / 10 s, heat-seeker steers to the nearest ped or car in the nose cone; flamethrower, spray can, extinguisher: flame / spraycan clips, area-effect stream along the aim, flame damage to peds; minigun instant-hit with the flame clip; move sets RLAUNCHER / FTHROWER / MINIGUN; rocket launchers aim first person through the siterocket sight; heavy weapons hold the clip pose (no barrel levelling) and their shots never widen the crosshair (player accuracy 100 without CanAim); no spray paint, retail lock-on pick not ported) (w 2)
- [~] Misc weapons (night-vision and thermal goggles: goggles_put_on clip, CPostEffects NightVision / InfraredVision filters (frame brightened in place of the green / blue relight, multiplied (32, 255, 32) or (255, 64, 255), grain 48 / 64, switch-on flash), head model on the head bone, never held; camera: rifle clips, 1100 ms between photos, cameraCrosshair viewfinder drawn as four mirrored quarters, white flash; sniper scope: first-person view, FOV 70 down to 15 / 3 by mouse wheel at x1.084 per notch, SNIPERcrosshair texture with black past it; the country rifle aims over the shoulder like retail; parachute: weapon.dat model worn on the back at the retail spine offset, open clip then float / steer / brake clips, landing clips on ground or water; canopy object (parachute.dff, paracx.txd) skinned and posed by the PARA_*_O clip in step with CJ's clip; the parachute follows player_parachute.scm: auto skydive when falling past -10 m/s over 20 m, pad-smoothed steering and drift, fire pulls the cord, canopy at 5 m/s with 1/20 per tick vertical easing, FALL_skyDive_DIE on a hard skydive landing, PARA_01 subtitle at 60..100 m; subtitle position still a guess) (w 2)
- [~] Weapon anim groups: move set follows the weapon (armed for long guns, rocket, chainsaw; bat uses the player set), swapped in place when the weapon changes (w 1)
- [~] Gun aim: arm guns use the retail IK point-arm chain (IKChain_c sweep, effector 0.05 m along the hand, no bone limits as retail ships them, 250 ms blend) toward the crosshair ray (Find3rdPersonCamTargetVector) with RotateTorsoForArm, others the torso (ms_torsoInfo limits) with the heading turned to the crosshair; classic crosshair at 0.53 / 0.4 sized by GetWeaponRadiusOnScreen; classic aim camera (Process_AimWeapon free aim: weapon FOV, look-at offsets, 1.0 + 1.6 cos(pitch) distance, pitch limits) and pro-skill twin pistols (left gun at the left hand, left arm chain, 2guns_crouchfire) done; lock-on targeting and the camera transition curve still simplified (w 1)
- [~] Missions (mission table of main.scm read; the VM launches a mission script as its own thread with its own jump base, and START_NEW_SCRIPT passes locals; mission 1 runs 410 instructions before its first game command; nothing is playable yet) (w 6)

## Gap list: player and combat (not yet ported)
- [ ] Weapon polish against retail captures: pistol, silenced pistol, Desert Eagle, micro SMG, Tec-9, sawn-off, MP5 aim pose, left arm and hold (w 2)
- [ ] Side-by-side retail comparison rig (capture harness for retail and the port at identical positions and frames) (w 1)
- [ ] Wardrobe: clothes, tattoos, hairstyles, barber, gym and tattoo parlour, clothes shops, CJ's body model per outfit (w 5)
- [ ] Player stats beyond fat and muscle: respect, sex appeal, stamina and lung capacity effects, weapon skill progression, driving skills, vehicle skills (w 3)
- [ ] Health, armour, death and respawn (busted and wasted, hospital and police station spawns, money loss, weapons lost) (w 3)
- [ ] Lock-on auto target, target switching, free aim versus auto aim modes, aiming at vehicles (w 2)
- [ ] Spray paint (tagging and the gang tag system, graffiti decals) (w 2)
- [ ] Ped hit reactions and ragdoll falls from bullets, explosions, cars and fire (w 3)
- [ ] Choking animation (tear gas), burning animation, drowning and electrocution deaths (w 1)
- [ ] Vehicle and object damage from explosions, creeping fire spread across ground, peds, cars and props (w 3)
- [ ] Explosion visuals: all retail explosion types, shockwave, debris, camera shake and decals (w 2)
- [ ] Driveby shooting from cars and bikes (w 2)
- [ ] Melee gaps: blocking, ground attacks, kicks, stomps, grabbing, pistol whip, hitting cars and objects (w 2)
- [ ] Dive, roll, dodge and cover behaviours (take cover, blind fire, peek) (w 2)
- [ ] Hand-to-hand and gun fighting for the gym fight styles (boxing, kung fu, kickboxing, grab and kick) (w 1)
- [ ] Gang members as followers: recruit, command, group AI (w 2)
- [ ] Interior and exterior door entry, enter and exit markers, interior streaming (w 4)
- [ ] Crouch-and-aim edge cases, vault while armed, ladder climbing, swimming body pitch (w 1)
- [ ] Jetpack (w 1)

## Gap list: vehicles
- [ ] Per-vehicle real suspension and wheel physics from handling.cfg (w 4)
- [ ] Vehicle to vehicle and vehicle to world collisions with deformation and damage (w 4)
- [ ] Body panel damage, doors, bonnet, boot and wheels falling off, flat tyres (w 3)
- [ ] Bikes and bicycles with riders and lean (w 3)
- [ ] Boats (w 2)
- [ ] Planes and helicopters with flight models (w 4)
- [ ] Trains and trams on the track nodes (w 3)
- [ ] Trailers, towing and the tow truck, cranes, forklifts, tractors, special vehicles (w 2)
- [ ] Vehicle lights, indicators, sirens, horn, headlight damage, neon, nitro and modifications (w 2)
- [ ] Mod garages, respray, car modification shops, import and export (w 3)
- [ ] Car radio UI, vehicle stealing animations, jacking, hot wiring, passengers and cabin seats (w 3)
- [ ] Vehicle generators and parked cars (w 1)
- [ ] Car fire, explosion chain, wreck and petrol tank rules (w 1)
- [ ] Hydraulics, lowriders, bouncing, and the lowrider minigame (w 2)

## Gap list: world and AI
- [ ] Path nodes (nodes*.dat) and traffic: lanes, junctions, lights, overtaking, parking (w 5)
- [ ] Pedestrian generation per zone (popcycle.dat), gangs, cops, shoppers, dealers, beggars (w 4)
- [ ] Pedestrian tasks: walk paths, cross roads, flee, fight, panic, react to events, speech (w 4)
- [ ] Wanted level system: crimes, police response, cop AI, helicopters, roadblocks, SWAT, FBI, army (w 5)
- [ ] Gang territories, gang wars and turf takeover (w 3)
- [ ] Emergency services: ambulance, fire trucks, firemen, paramedics (w 2)
- [ ] Scripted shops: Ammu-Nation, food, clothes, tattoo, barber (w 3)
- [ ] Pickups: weapons, health, armour, money, hidden packages, oysters, horseshoes, snapshots (w 2)
- [ ] Properties: safe houses, save points, garages, buying and the income (w 3)
- [ ] Day and night cycle effects on populations, shop opening hours, radio schedule (w 1)
- [ ] Water waves, volumetric clouds, rain, fog, wind, sandstorm, thunder and lightning (w 2)
- [ ] Particles from particle.cfg and fx files: all effects (w 3)
- [ ] Decals: bullet holes, blood, skid marks, tyre tracks, scorch, footprints (w 2)
- [ ] Reflection and visual effects: car reflections, heat haze, motion blur, light coronas, lens flare, shadows (w 3)
- [ ] World destruction: lamp posts bending, breakable props, glass, fences, signs (w 2)
- [ ] Animals and wildlife: dogs, cats, birds, sharks, fish, cows, horses (w 1)
- [ ] Prop physics gaps: toppling, uprooting, breakable parts, real COL shapes per prop (w 2)

## Gap list: scripts, missions and minigames
- [ ] SCM opcodes implemented (all roughly 1000 commands of the VM: peds, cars, objects, audio, text, camera, stats) (w 8)
- [ ] Mission flow: mission start and fail, pass screen, rewards, retry, mission text and timers, cutscene triggers (w 4)
- [ ] Cutscenes from the .cut and .dat files with camera paths and lip sync (w 4)
- [ ] All story missions of Los Santos, Countryside, San Fierro, Las Venturas, and the endgame (w 10)
- [ ] Side missions: taxi, vigilante, paramedic, firefighter, pimping, trucking, courier, quarry, 555 We Tip (w 3)
- [ ] Schools: driving, boat, flight, bike (w 2)
- [ ] Minigames: pool, darts, basketball, arcade machines, dance, lowrider, gambling, casino, horse racing, 24-7 (w 4)
- [ ] Races: street, burglary, vehicle missions, stunt jumps, rampages (w 3)
- [ ] Dating and girlfriends, food and eating, sex appeal, gym workouts (w 3)
- [ ] Phone and shops text, subtitle, help messages, mission titles, countdowns (w 1)
- [ ] Save and load games, game states, auto save, statistics page complete (w 3)

## Gap list: frontend and polish
- [ ] Start screen, new game, load game, intro sequence and loading screens (w 1)
- [ ] Controller support parity: remapping for every action, vibration (w 1)
- [ ] Complete radar and map: all blips, zones, GPS route, legend with correct sprites (w 1)
- [ ] Audio: all retail ambient, traffic, weather, world and mission sounds; positional 3D audio, doppler, occlusion, cutscene and speech banks (w 4)
- [ ] Full radio programming and the cassette and radio UI (w 1)
- [ ] Performance, memory and streaming stability at full retail draw distance (w 2)
- [ ] Windows MSVC and Linux MinGW builds both at warning-free parity, regression test scripts (w 1)
