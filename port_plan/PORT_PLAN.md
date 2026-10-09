# Rust/Bevy port plan: GTA SA 1.0 US

Generated from this decomp's call graph (`scripts/CallGraph.java` -> `types/callgraph.tsv`, 54,694 call edges) and
`tools/portmap.py`. Machine-readable output is in this folder:
- `subsystems.tsv`: functions, strong-name %, bytes and top classes per subsystem
- `deps.tsv`: cross-subsystem call counts (caller -> callee)
- `layering.json`: dependency rank (0 = depended on by the most, ports first)
- `classes/<subsystem>.txt`: every class/namespace in the subsystem with its function count

How functions were assigned to subsystems: by class name rules, then by the gta-reversed header directory of the class or hook,
then by address neighbours (one .obj compiles to contiguous code) confirmed by a call-graph vote. "approx" counts are
the functions placed by address. 2,707 small helpers (pool getters, inlined templates) remain unclassified.

Rules for the port: load assets from the user's own game install at runtime and never ship them. The decomp (`src/`) is
the behaviour reference. gta-reversed (readable C++) is the primary guide where it exists.

## Subsystem table (game code only; CRT 5,020, protection, QuickTime 711 and EH funclets excluded)

| rank | subsystem | functions | strong-named | code bytes | notes |
|---|---|---|---|---|---|
| 0 | renderware (RW 3.6 engine + D3D9 pipelines/plugins) | 3,644 | 29% | 827 KB | **do not port**: replace with Bevy renderer; only the file formats (DFF/TXD/COL/IFP) are needed |
| 1 | core_math_containers (CVector, CMatrix, CPool, link lists) | 538 | 40% | 43 KB | trivial: glam + slotmap/Vec |
| 2 | collision (CColModel, CCollision tests) | 247 | 40% | 49 KB | port 1:1, needed by physics; or use Bevy-Rapier with SA's COL shapes |
| 3 | platform_os (CFileMgr, CdStream, CTimer, CPad, memory) | 1,896 | 19% | 131 KB | mostly replaced by std/Bevy input/time; the IMG/cdstream reader is needed |
| 3 | fx (particle system, fx BP) | 563 | 46% | 85 KB | later; bevy_hanabi or custom |
| 5 | streaming (CStreaming, CStreamingInfo, CColStore, IMG) | 336 | 33% | 34 KB | **core**: async asset loading by model id |
| 6 | animation (CAnimManager, blend, IFP) | 286 | 60% | 37 KB | IFP loader + blend tree to Bevy animation |
| 7 | file_loading (CFileLoader IDE/IPL/DAT, handling.cfg, timecyc, weapon.dat, GXT) | 640 | 58% | 83 KB | **first thing to port**: pure parsers, easy to test |
| 7 | models (CBaseModelInfo hierarchy, clothes) | 503 | 49% | 48 KB | model registry indexed by model id |
| 8 | audio (CAE*, SFX banks, streams) | 1,188 | 65% | 198 KB | late; bevy_kira_audio; bank formats needed |
| 9 | render (CRenderer visibility, shadows, coronas, weather, water) | 821 | 63% | 198 KB | rewrite on Bevy; keep the LOD/visibility rules |
| 9 | world_entities (CWorld sectors, CEntity/CPhysical/CObject/CBuilding) | 1,185 | 58% | 295 KB | ECS mapping; CWorld = 120x120 sector grid |
| 12 | camera (CCamera, CCam modes) | 295 | 66% | 150 KB | port 1:1 (feel matters) |
| 12 | weapons_combat (CWeapon, bullets, explosions, fire) | 142 | 78% | 61 KB | port 1:1 |
| 13 | peds_ai (CPed, CPlayerPed, tasks, events, decision makers, population) | 5,958 | 64% | 862 KB | **largest**: about 900 task/event classes |
| 13 | ui_hud_frontend (CHud, CRadar, CMenuManager, CFont) | 427 | 47% | 126 KB | bevy_ui; fonts/sprites from TXD |
| 16 | scripts_missions (CRunningScript SCM VM, ~1,800 opcode handlers, pickups, stats, save) | 1,164 | 41% | 313 KB | the SCM VM runs the original main.scm: the whole storyline for free |
| 17 | vehicles (CVehicle/CAutomobile/CBike/..., CCarCtrl AI, path find, handling) | 1,552 | 58% | 486 KB | physics + AI; port 1:1 |

"strong-named" = public-source or library-signature names. The remaining functions have weak inferred names, so read the body
(and gta-reversed when it has the function) before porting them.

## Main cross-subsystem dependencies (call edges, top)
peds_ai -> world_entities 937, scripts -> peds_ai 613, peds_ai -> platform 584, vehicles -> math 546, vehicles -> peds_ai 510,
vehicles -> world 497, peds_ai -> math 466, peds_ai -> file_loading 387, world -> math 370, ui -> render 365,
peds_ai -> animation 357, render -> renderware 329, models -> renderware 313, scripts -> world 299, peds_ai <-> vehicles 288/510.
Peds and vehicles are mutually dependent (enter/exit car, drivers, passengers), so port them in the same phase.

## Port order (each phase ends with something runnable)

**Phase 0: Asset readers (no game logic).** Crate `sa-formats`.
IMG v2 archive (`CStreaming::InitImageList` 0x4083c0, `CdStream*`), DFF/RW stream chunks (geometry, frames, materials,
skin, 2dfx), TXD (D3D9 raster formats incl. DXT), COL v1-v3 (`CFileLoader::LoadCollisionFile` 0x538440,
`LoadCollisionModelVer3` 0x537ce0), IFP (`CAnimManager::LoadAnimFile` 0x4d47f0), GXT (`CText::Load` 0x6a01a0),
binary IPL. Test: dump all files from the user's install with no errors.

**Phase 1: Static world viewer.** Crates `sa-data`, `sa-world`.
`CFileLoader::LoadLevel` 0x5b9030 (gta.dat/default.dat), `LoadObjectTypes` (IDE) 0x5b8400, `LoadScene` (IPL) 0x5b8700,
`CIplStore::LoadIpl` 0x406080, model info registry, `CTimeCycle::Initialise` 0x5bbac0. Then render all buildings in Bevy
with an LOD distance from the IDE draw distance. Streaming (`CStreaming::RequestModel` 0x4087e0, `LoadAllRequestedModels`
0x40ea10, `ConvertBufferToObject` 0x40c6b0) is replaced by Bevy asset handles keyed by model id plus distance-based
loading around the camera, the way CStreaming does it. Fly camera. **Result: San Andreas map in Bevy.**

**Phase 2: Time, world grid, collision, physics.**
`CTimer::Update` 0x561b10 (SA timestep = frametime / 20 ms, i.e. 50 steps/s; keep this: physics constants depend on it).
`CWorld` sectors (120x120, plus 16x16 repeat sectors), `CWorld::Add` 0x563220, `CWorld::Process` 0x5684a0.
`CCollision::ProcessColModels` 0x4185c0, `CPhysical::ProcessCollision` 0x54dfb0, `ProcessShift` 0x54db10,
`ApplyCollision` 0x5435c0. Decide here: 1:1 SA physics (needed for authentic vehicle handling) or Rapier.
Recommendation: port SA's own; handling.cfg values only make sense with it.

**Phase 3: Player on foot.**
`CPad::Update` 0x541c40 -> Bevy input; `CPlayerPed::ProcessControl` 0x60ea90, `CPed::ProcessControl` 0x5e8cd0, the
player task subset (CTaskSimplePlayerOnFoot and friends), animation blending (Phase 0 IFP + `CAnimBlendAssociation`),
`CCamera::Process` 0x52b730 (follow-ped mode first). **Result: walk around the map.**

**Phase 4: Vehicles.**
`cHandlingDataMgr::LoadHandlingData` 0x5bd830, `CAutomobile::ProcessControl` 0x6b1880, wheels/suspension, damage
(`CDamageManager`), enter/exit tasks, car camera. Then bikes, boats, planes and helis.

**Phase 5: The world comes alive.**
Path nodes (`CPathFind::LoadPathFindData` 0x452f40; note `CPathFind::Init` lives at 0x1560dc0 in the relocated .HOODLUM
section), `CPopulation::Update` 0x616650, `CCarCtrl::GenerateRandomCars` 0x4341c0, ped AI tasks/events/decision makers.
This is the biggest phase (about 6K functions).

**Phase 6: Missions.**
SCM VM: `CTheScripts::Init` 0x468d50, `CTheScripts::Process` 0x46a000, `CRunningScript::Process` 0x469f00, and the
opcode handlers (gta-reversed `Scripts/Commands/*.cpp` has about 1,800 command implementations). Running the original
main.scm gives the full storyline. Also pickups, stats, save/load (`CGenericGameStorage`), weapons and combat.

**Phase 7: Presentation.**
HUD (`CHud::Draw` 0x58fae0), radar (`CRadar::DrawMap` 0x586b00), menus, audio (`CAudioEngine::Service` 0x507750),
weather/clouds (`CClouds::Render` 0x713950), water (`CWaterLevel::RenderWater` 0x6ef650), shadows, coronas, fx.

## Bevy mapping cheatsheet
| SA | Bevy |
|---|---|
| CPool<T> + handles (ref = index<<8 / flags) | `Entity` (keep a `ScriptHandle` component for SCM refs) |
| CEntity::m_matrix / CPlaceable | `Transform` |
| CPhysical (velocity, mass, turn speed) | `Physical` component + a fixed-timestep system at 50 Hz |
| CWorld sectors / scan lists | spatial-hash resource used by the collision systems |
| CStreaming | `AssetServer` + `ModelRegistry` resource + distance-based load/unload system |
| CTask hierarchy (virtual MakeAbortable/ProcessPed) | trait objects in a `PedTasks` component; port the logic 1:1 |
| CEvent queue | Bevy events + per-ped event queue component |
| CTimer::ms_fTimeStep | `Time<Fixed>`, timestep = 20 ms * game speed |
| CRunningScript | resource holding VM threads, run in the `Update` schedule |

## What the decomp gives you per function
`src/<Class>/<Name>_<addr>.c`: Ghidra pseudo-C with gta-reversed types (`src/_types.h`, 2,237 types, 567 with validated
size), typed `this` for 8,063 functions, and 1,729 named and typed globals. `src/_index.tsv` lists each function's name
source and evidence. Weak names (`unk_*`, `get_0x1c_*`, `uses_X_*`) are hints, not facts.
