#pragma once
// C3dMarkers, the user 3D markers and CCheckpoints: the coloured shapes the game puts in the world.
//   cylinder  the red ground ring of a script locate (and a contact point off mission), snapped to the ground
//             or the water surface,
//   cone      the bobbing, spinning diamond over an entry/exit (yellow) and over blipped things,
//   tube      a race checkpoint column, torus an air-race hoop turned to face along the course,
//   user      the five script markers (CREATE_USER_3D_MARKER), a diamond in a HUD colour.
// Markers live only while something places them every frame: Render draws the ones placed since the last
// Render and frees the rest (C3dMarkers::Render). Models are gta3.img cylinder, hoop and diamond_3, drawn
// unlit in the marker colour with no depth write.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class D3DRenderer;
struct D3DRenderMesh;
struct IDirect3DTexture9;

class Markers3D {
public:
    struct Model { std::vector<D3DRenderMesh*> meshes; std::vector<IDirect3DTexture9*> textures; float radius = 1.0f; };

    enum Type { ARROW = 0, CYLINDER = 1, TUBE = 2, ARROW2 = 3, TORUS = 4, CONE = 5, CONE_NO_COLLISION = 6, NA = 257 };
    // eCheckpointType
    enum CpType { CP_TUBE = 0, CP_ENDTUBE = 1, CP_EMPTYTUBE = 2, CP_TORUS = 3, CP_TORUS_NOFADE = 4, CP_TORUSROT = 5,
                  CP_TORUSTHROUGH = 6, CP_TORUS_UPDOWN = 7, CP_TORUS_DOWN = 8 };

    struct Hooks {
        std::function<float(float x, float y, float z)> ground;                  // CWorld::FindGroundZFor3DCoord (< -1e5: none)
        std::function<bool(float x, float y, float* z)> water;                   // CWaterLevel::GetWaterLevelNoWaves
        std::function<bool(const float a[3], const float b[3], float* hitZ)> buildingLos;   // ProcessLineOfSight, buildings
    } hooks;

    bool Load(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);

    // Once a frame before anything places markers. timeStep: CTimer::ms_fTimeStep (dt * 50). centre: the
    // player's centre of world, cam: the camera, player: the player ped (checkpoint animations).
    void Update(float timeStep, const float centre[3], const float cam[3], const float player[3], uint32_t nowMs);

    void PlaceMarker(uint32_t id, int type, const float pos[3], float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     uint16_t pulsePeriod, float pulseFraction, int rotateRate, const float dir[3], bool zCheck);
    void PlaceMarkerSet(uint32_t id, int type, const float pos[3], float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                        uint16_t pulsePeriod, float pulseFraction);
    void PlaceMarkerCone(uint32_t id, const float pos[3], float size, uint8_t r, uint8_t g, uint8_t b, uint16_t pulsePeriod,
                         float pulseFraction, int rotateRate, bool collision);
    // CTheScripts::HighlightImportantArea: a locate's red ground ring, z <= -100 meaning on the ground.
    void HighlightArea(uint32_t id, float x1, float y1, float x2, float y2, float z);

    // C3dMarkers::User3dMarkerSet / Delete: -1 when all five are taken. colour: a CHudColours index.
    int UserMarkerSet(float x, float y, float z, int colour);
    void UserMarkerDelete(int i);

    // CTheScripts::AddScriptCheckpoint (red; tubes alpha 32, tori 96) and the CCheckpoints calls behind the
    // checkpoint commands. Returns the checkpoint's id (0 when the table is full).
    uint32_t AddScriptCheckpoint(int type, const float at[3], const float pointTo[3], float size);
    void DeleteCheckpoint(uint32_t id);
    void CheckpointPos(uint32_t id, const float pos[3]);          // CCheckpoints::UpdatePos
    void CheckpointHeading(uint32_t id, float headingDeg);        // CCheckpoints::SetHeading

    // CCheckpoints::Render and C3dMarkers::Render; call after the opaque world.
    void Render(D3DRenderer& r, const float cam[3]);

private:
    struct Marker {
        uint32_t id = 0;
        int type = NA;
        bool inUse = false;
        float rot[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};   // the spin (m_mat rows)
        float pos[3] = {};
        float size = 1.0f, stdSize = 1.0f;
        uint8_t col[4] = {255, 255, 255, 255};
        int rotateRate = 0;
        float pulseFraction = 0.0f;
        float normal[3] = {0, 0, 1};
        float dist2D = 0.0f;
        float roof = 65535.0f;
        float lastPos[3] = {};
        uint32_t onScreenTest = 0;
        int mapReadX = 30000, mapReadY = 30000;   // m_LastMapReadX/Y: where the ground was read
    };
    struct UserMarker { bool used = false; float pos[3] = {}; uint8_t rgb[3] = {}; };
    struct Checkpoint {
        int type = 0x101;
        bool used = false, rotFlag = false;
        uint32_t id = 0;
        uint8_t col[4] = {};
        uint16_t pulsePeriod = 0;
        int rotateRate = 0;
        float pos[3] = {}, fwd[3] = {};
        float pulseFraction = 0.0f, size = 0.0f, dist2D = 0.0f, multiSize = 0.0f;
    };

    const Model* ModelOf(int type) const;
    void UpdateZ(Marker& m, const float point[3], float zDistance);
    void CheckpointPlace(Checkpoint& c);

    Model m_cylinder, m_hoop, m_diamond;
    Marker m_m[32];
    UserMarker m_user[5];
    Checkpoint m_cp[32];
    uint32_t m_nextCpId = 1;
    float m_angleDiamond = 0.0f;   // m_angleDiamondDeg
    float m_timeStep = 1.0f;
    float m_centre[3] = {}, m_cam[3] = {}, m_player[3] = {};
    uint32_t m_now = 0;
    bool m_loaded = false;
};
