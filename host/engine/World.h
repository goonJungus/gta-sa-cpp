#pragma once
// GameWorld: data-driven exterior world for gtasa_cpp.exe (play mode).
//
// Loads the GTA:SA exterior from the retail data on disk + gta3.img the same
// way the engine does (CStreaming):
//   - IDE definitions (modelId -> dff/txd/drawDistance/flags), 6 generic + LA.
//   - Text IPLs (the LOD layer per region).
//   - Binary *_stream*.ipl from the IMG (the HD layer per region).
//   - .col archives from the IMG (real collision: ground triangles + solid
//     world boxes).
//
// Every frame the world is streamed around the player: models shared per IDE
// mesh are loaded near the player (HD within drawDistance + margin, LOD for
// distant instances), unloaded after a grace period, terrain ground-height
// triangles are indexed into an 8m spatial hash, and solid COL boxes are kept
// for loaded non-ground models only (matches "walk through walls beats
// invisible walls" - collision exists only where geometry is loaded).

#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

#include "BinaryIpl.h"
#include "ColLoader.h"
#include "D3DRenderer.h"
#include "DffLoader.h"
#include "IdeLoader.h"
#include "ImgLoader.h"
#include "IplLoader.h"
#include "TxdLoader.h"

struct WorldColBox {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

// Oriented collision box: the 8 world-space corners of the COL box after the
// instance quaternion. Drawn by the F3 view so the pink wireframe hugs the
// model; the AABB above stays for the (conservative) collision queries.
// Corner order: index = i*4 + j*2 + k for i,j,k in {0,1} over
// (minX/maxX, minY/maxY, minZ/maxZ).
struct WorldColObb {
    float x[8], y[8], z[8];
};

// COL sphere in world space (bollards, bins, posts, props; 147 retail models
// collide with spheres only).
struct WorldColSphere {
    float x, y, z, r;
};

class GameWorld {
public:
    GameWorld() = default;
    ~GameWorld();
    GameWorld(const GameWorld&) = delete;
    GameWorld& operator=(const GameWorld&) = delete;

    // Load all IDE/IPL/stream/COL inventories. gameDir = the game root that
    // contains data\ and models\ (GameDir()). Returns false on fatal failure.
    bool Init(ImgLoader& img, D3DRenderer& renderer, const std::string& gameDir);
    void Shutdown();

    // Per-frame streaming decision based on the player's feet position.
    void Stream(float px, float py, float pz);

    // Draw every instance currently in range (call inside BeginFrame/EndFrame,
    // after the view/proj matrices are set). This is the OPAQUE pass: it draws
    // every non-blended mesh and queues the alpha-blended ones (glass, decals,
    // soft foliage) for RenderAlpha. Draw the player between the two so a fence
    // or window in front of the player correctly occludes it.
    void Render(D3DRenderer& r, float px, float py, float pz);

    // Second pass: draw the alpha-blended meshes queued by Render, sorted
    // back-to-front so overlapping transparent surfaces blend in the right
    // order. Depth-tested but not depth-writing (set per-mesh in DrawMesh).
    void RenderAlpha(D3DRenderer& r);

    // Night lighting: draw glowing coronas + warm ground pools at every street
    // lamp in view. intensity 0 = day (nothing drawn), 1 = full night. Call
    // after Render() + RenderSky() so the glows sit on top of the lit world.
    void RenderLamps(D3DRenderer& r, float camX, float camY, float camZ, float intensity);

    // Traffic signals: draws the lit vehicle bulb (red/amber/green, cycling) and
    // the pedestrian walk/don't-walk light, following CTrafficLights exactly.
    // timeMs is a real-time millisecond clock (the SA cycle is ~32.8s, wall time
    // independent of the day/night clock). dayNight 0=day..1=night scales the glow.
    void RenderTrafficLights(D3DRenderer& r, float camX, float camY, float camZ,
                             uint32_t timeMs, float dayNight);

    // Dev collision view (F3): pink wireframe of every active solid COL box
    // plus the terrain triangles around the player. Display only; the
    // collision queries themselves are unaffected.
    void DrawCollision(D3DRenderer& r, float px, float py, float pz, float radius) const;

    // Dev: dump nearby instances (model, pos, quat) to inst_dump.txt.
    void DumpNearby(float px, float py, float pz, float radius) const;

    // Dev (--probe-collision): write a collision ground-truth report at (px,py)
    // with feet at feetZ -- per-instance OBB footprints, an ASCII solid/open map,
    // and triangle-wall stats -- so walk-through / phantom-wall bugs are visible
    // without running the GUI. radius = map half-span (m).
    void DumpCollisionProbe(float px, float py, float feetZ, float radius,
                            const char* path) const;

    
    // World-space ground height at (x,y): the highest terrain triangle surface
    // at/below refZ + maxStepUp (1.5m, curb stepping). Returns kNoGround-like
    // sentinel (see ColLoader) when there is no terrain - caller falls back to
    // its grass safety net.
    float GetGroundHeight(float x, float y, float refZ) const;

    // Player collision: true when a solid COL box blocks movement INTO (x,y)
    // from (ox,oy) at feet height feetZ. Mirrors retail tolerances: boxes up
    // to feetZ+stepOver are stepped over, boxes above feetZ+playerHeight are
    // ignored (can walk under), and the player is only blocked when
    // ENTERING a box (not when already inside it).
    bool HitsSolidCircle(float x, float y, float feetZ, float ox, float oy,
                         float radius, float stepOver, float playerHeight) const;

    // Segment a -> b against the streamed collision (COL triangles, boxes and
    // spheres). Returns the fraction of the way to the first hit, 1 if clear
    // (the camera's CWorld::ProcessLineOfSight).
    float RayCast(float ax, float ay, float az, float bx, float by, float bz) const;

    size_t LoadedModelCount() const { return m_cache.size(); }
    size_t VisibleInstances() const { return m_visInsts; }
    size_t SolidBoxCount() const { return m_worldBoxes.size(); }
    size_t LastDrawnInsts() const { return m_lastDrawnInsts; }
    size_t LastDrawMeshCalls() const { return m_lastDrawMeshCalls; }
    size_t LastSkippedInsts() const { return m_lastSkippedInsts; }

    // --- Streaming attribution (updated every Stream(), for the profiler) ---
    // These let the host explain a heavy frame: how many DFF/TXD model loads
    // ran this frame and how long they took, and likewise for collision-tri
    // builds. Model loads are the main streaming hitch; collision builds are
    // the second. Both are budget-capped, so a spike here is a budget hit.
    size_t LastModelsLoaded() const { return m_lastModelsLoaded; }
    size_t LastColBuilds()    const { return m_lastColBuilds; }
    double LastLoadMs()       const { return m_lastLoadMs; }
    double LastColMs()        const { return m_lastColMs; }

    // Draw distance in metres, straight from timecyc.dat FarClp (retail keeps
    // models alive out to their IDE draw distance, capped by the far clip).
    void SetFarClip(float dist) { m_farClip = dist; }
    float FarClip() const { return m_farClip; }

    // Multiplier on every HD model's IDE draw distance (how far out the
    // detailed model is used before switching to its LOD). 1.0 = retail.
    void SetHdDrawScale(float s) { m_hdDrawScale = s; }
    float HdDrawScale() const { return m_hdDrawScale; }

private:
    struct Inst {
        // --- static (built at Init) ---
        // Every placement (binary or text IPL, HD or LOD) is its own Inst at
        // its own transform, like retail's CEntity. LOD links mirror
        // CEntity::m_pLod / m_nNumLodChildren: a LOD is hidden while any of
        // its children draws.
        std::string hdModel;   // dff base (lower, no ext)
        std::string hdTxd;
        float hdDraw = 0.0f;   // IDE draw distance
        float x = 0, y = 0, z = 0;
        float qx = 0, qy = 0, qz = 0, qw = 1;
        int32_t lodParent = -1;    // m_insts index of this placement's LOD (-1 none)
        uint16_t lodChildren = 0;  // placements that use this one as their LOD
        uint8_t lodLevel = 0;      // 0 = leaf, 1 = LOD of leaves, ...
        int32_t lodTwin = -1;      // extra LOD on the same spot as this LOD: draw with it
        bool isGround = false;    // classified ground (terrain tris + no solid boxes)
        // --- runtime (Stream / Render) ---
        bool needed = false;
        bool draw = false;         // Render: drawn this frame
        bool childDrawn = false;   // Render: a LOD child is drawn -> hide this LOD
        bool groundTrisIn = false; // terrain tris computed + hashed
        bool boxesIn = false;      // world-space COL boxes computed
        std::vector<WorldColBox> boxes;      // AABBs (collision queries)
        std::vector<WorldColObb> obb;        // oriented corners (F3 view)
        std::vector<WorldColSphere> spheres; // world-space COL spheres
        uint32_t distSq = 0xFFFFFFFFu;     // own XY distance^2 (collision, sorting)
        uint32_t lodDistSq = 0xFFFFFFFFu;  // HD/LOD switch distance^2: measured to the
                                           // LOD's position like retail, so siblings switch together
    };

    struct ModelCacheEntry {
        std::string txdName;
        std::vector<D3DRenderMesh*> meshes;
        int refs = 0;
        int lastSeen = 0;
        float radius = 0.0f;  // model bounding-sphere radius (local origin), for frustum culling
        // Lamp bulb(s) in model-local space. A single-arm lamp has one (the head
        // at the arm end); a double-arm freeway lamp has two (one per arm). Set
        // only for lamp models; used to place the night glow(s) on the bulb(s).
        int   bulbCount = 0;
        float bulb[2][3] = {};
        // Traffic-light signal bulbs (model-local, from 2dfx EFFECT_LIGHT):
        // colorIdx 0=green, 1=amber, 2=red. Empty for non-signal models.
        struct TLBulb { float x, y, z; int colorIdx; };
        std::vector<TLBulb> tlBulbs;
        // Roadsign text (2dfx EFFECT_ROADSIGN): one model-local quad per text
        // line, each with its own generated text texture (owned here).
        std::vector<D3DRenderMesh*> signMeshes;
        std::vector<IDirect3DTexture9*> signTex;
    };

    struct ColArc {
        std::string name;
        std::vector<uint8_t> data;
    };

    struct ColEntryLoc {
        size_t arc;
        size_t off;
    };

    // --- loading ---
    bool LoadIdes(const std::string& gameDir);
    bool LoadTextIpls(const std::string& gameDir);
    void LoadStreams();
    void LoadCols();
    void BuildTextInsts();
    void LinkLodChildren();
    // HD switch distance: the IDE draw distance, scaled by the HD draw-distance
    // setting for leaf models (LODs keep their own retail distance).
    float DrawDist(const Inst& in) const {
        return in.lodChildren ? in.hdDraw : in.hdDraw * m_hdDrawScale;
    }
    bool LoadModel(const std::string& modelName, const std::string& txdName);
    bool LoadTxd(const std::string& txdName);
    IDirect3DTexture9* GetTexture(const std::string& txdName, const std::string& texName);
    const ColEntryLoc* FindColEntry(const std::string& modelName);
    bool ParseBoxes(size_t arc, size_t off, std::vector<WorldColBox>& out);
    void BuildGroundTris(Inst& inst);
    void BuildInstBoxes(Inst& inst);

    // --- helpers ---
    static std::string ToLower(const std::string& s);
    static bool IsGroundModel(const std::string& name, int flags);
    static int64_t CellKey(int gx, int gy);
    D3DMATRIX InstTransform(const Inst& in) const;

    ImgLoader* m_img = nullptr;
    D3DRenderer* m_renderer = nullptr;
    std::string m_gameDir;

    // Missing-model memo (avoid re-extracting DFFs that fail).
    std::set<std::string> m_badModels;
    IDirect3DTexture9* m_fallbackTex = nullptr;
    // Texture keys ("txd/tex") that aren't in their own txd nor any resident
    // txd, so GetTexture's cross-txd search doesn't rescan every frame. Cleared
    // whenever a new txd loads (it might now satisfy the name).
    std::set<std::string> m_texMiss;

    size_t m_streamFiles = 0;

    // IDE tables
    std::unordered_map<int, IdeObject> m_byId;     // merged modelId -> ide
    std::unordered_map<std::string, IdeObject> m_byName; // lower name -> ide

    // Text IPL region arrays: region key (lower) -> inst array.
    std::unordered_map<std::string, std::vector<IplInstance>> m_text;
    // Region key -> m_insts index of each text IPL line (-1 = not placed), so
    // binary stream LOD indices and text-to-text LOD indices resolve.
    std::unordered_map<std::string, std::vector<int32_t>> m_textInst;

    // World instances: every text and binary IPL placement.
    std::vector<Inst> m_insts;
    size_t m_lastDrawnInsts = 0, m_lastDrawMeshCalls = 0, m_lastSkippedInsts = 0;
    size_t m_visInsts = 0;
    float m_hdDrawScale = 1.1f;   // user asked for +10% HD distance (has headroom)
    size_t m_lastModelsLoaded = 0, m_lastColBuilds = 0;
    double m_lastLoadMs = 0.0, m_lastColMs = 0.0;

    // Alpha-blended meshes queued by Render (opaque pass) and drawn
    // back-to-front by RenderAlpha after the player. Rebuilt every frame.
    struct AlphaDraw { uint32_t distSq; D3DMATRIX world; D3DRenderMesh* mesh; };
    std::vector<AlphaDraw> m_alphaQueue;

    // COL archives from the IMG.
    std::vector<ColArc> m_cols;
    std::unordered_map<std::string, ColEntryLoc> m_colEntry; // model -> (arc, off)

    // Terrain: world-space triangles bucketed into 8m cells; 9-cell query.
    struct Cell { std::vector<ColTriangle> tris; };
    std::unordered_map<int64_t, Cell> m_cells;

    // Streaming caches.
    std::unordered_map<std::string, ModelCacheEntry> m_cache;

    // Street-lamp emitters: indices into m_insts, scanned at Init. RenderLamps
    // places a glow at each lamp's bulb (model bulb offset rotated by the
    // instance), so the light sits on the lamp head, not atop the pole.
    std::vector<uint32_t> m_lampEmitters;
    std::vector<uint32_t> m_tlEmitters;   // instance indices of traffic lights
    void BuildLampEmitters();

    // Roadsigns (CCustomRoadsignMgr): particle.txd "roadsignfont", 32x512
    // A8R8G8B8 = 4 x 32 glyphs of 8x16, kept on the CPU to build sign rasters.
    std::vector<uint32_t> m_signFont;
    void LoadRoadsignFont();
    void BuildRoadsigns(const std::vector<Dff2dfxRoadsign>& signs, ModelCacheEntry& ent);
    void FreeModel(ModelCacheEntry& e);
    std::unordered_map<std::string, std::vector<TxdTexture>> m_txdPool; // pointer-stable
    std::unordered_map<std::string, const TxdTexture*> m_texInfo;      // txd/texkey -> pool ptr
    std::unordered_map<std::string, IDirect3DTexture9*> m_d3dTex;      // txd/texkey -> d3d
    std::unordered_map<std::string, int> m_txdRefs;                    // txdname -> models
    std::unordered_map<std::string, std::vector<std::string>> m_txdTexKeys;
    // Txd parent chain (child txd -> parent txd), from "txdp" sections in the
    // IDEs plus data\txdcut.ide and data\maps\txd.ide (the gb_* global chain).
    // GetTexture walks this when a texture is missing from a model's own txd,
    // matching retail RW parent-dictionary lookup. Names are lowercase.
    std::unordered_map<std::string, std::string> m_txdParent;

    // Active solid boxes (rebuilt by Stream each frame).
    std::vector<WorldColBox> m_worldBoxes;
    std::vector<WorldColObb> m_worldObbs;
    std::vector<WorldColSphere> m_worldSpheres;
    std::vector<uint32_t> m_worldBoxOwner;   // m_insts index per m_worldBoxes entry (probe)

    int m_frame = 0;
    float m_farClip = 900.0f; // timecyc FarClp (SetFarClip)

    static constexpr float kStreamRadius = 350.0f;
    static constexpr float kGroundRadius = 450.0f;
    static constexpr float kLoadMargin = 25.0f;
    // Streaming budget: load up to kMaxLoadsPerFrame models, but stop early once
    // kLoadBudgetMs of wall time is spent this frame. The time cap bounds the
    // worst-case hitch when a few big models land together; the higher count cap
    // lets a burst of cheap models stream in quickly so the player sees HD (not
    // the low-res LOD) sooner when moving.
    static constexpr int kMaxLoadsPerFrame = 24;
    static constexpr double kLoadBudgetMs = 5.0;
    static constexpr int kUnloadGrace = 300; // frames a model stays after refs==0
    static constexpr float kCell = 8.0f;
    // Solid COL boxes/spheres are kept for every instance within this radius,
    // whether or not its render model is loaded (retail streams collision
    // separately from models).
    static constexpr float kColRadius = 160.0f;
};