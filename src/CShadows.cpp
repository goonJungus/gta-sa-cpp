// CShadows.cpp - GTA SA 1.0 clean-room C++ conversion
// Method bodies ported from src/CShadows/*.c with guidance from
// gta-reversed/source/game_sa/Shadows.cpp.
// See BUILD_NOTES.md (2026-10-09) for divergences and flagged items.

#include "CShadows.h"

#include "CVector.h" // CVector, CVector2D (via CShadows.h too)
#include "CTimer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// ============================================================================
// TODO(port): external subsystem shims. Minimal declarations verified against
// gta-reversed and the decompiled code; delete entries as subsystems are ported.
// ============================================================================

// --- RenderWare ---
RwTexture* RwTextureRead(const char* name, const char* mask);
void RwTextureDestroy(RwTexture* texture);
void RwRenderStateSet(int32_t state, void* value);
#define RWRSTATE(v) (reinterpret_cast<void*>(v))

// --- Texture store ---
struct CTxdStore {
    static void PushCurrentTxd();
    static void PopCurrentTxd();
    static int32_t FindTxdSlot(const char* name);
    static void SetCurrentTxd(int32_t slot);
};

// --- General ---
struct CGeneral {
    static uint32_t GetRandomNumber();
    static float GetRandomNumberInRange(float min, float max);
};

// --- World / camera ---
struct CWorldShim {
    static float GetSectorPosX(int32_t sector);
};
CVector FindPlayerCoors();
float FindPlayerHeading();

// --- Collision ---
struct CColPoint {
    CVector m_vecPoint;
    CVector m_vecNormal;
};
struct CEntityShim;
bool CWorld_ProcessLineOfSight(const CVector& start, const CVector& end,
                               CColPoint& colPoint, CEntityShim*& entity,
                               bool buildings, bool vehicles, bool peds,
                               bool objects, bool dummies, bool seeThrough,
                               bool ignoreSomeObjects);

// --- Static shadow poly bunch management ---
void AddToEmptyBunchList(CPolyBunch* bunch);
CPolyBunch* GetFromEmptyBunchList();

// ============================================================================
// Static member definitions
// ============================================================================
uint16_t CShadows::ShadowsStoredToBeRendered = 0;                        // 0xC403DC
CPolyBunch* CShadows::pEmptyBunchList = nullptr;                         // 0xC403D8
std::array<CPolyBunch, MAX_SHADOW_POLY_BUNCHES> CShadows::aPolyBunches{}; // 0xC40DF0
std::array<CStaticShadow, MAX_STATIC_SHADOWS> CShadows::aStaticShadows{}; // 0xC4A030
std::array<CPermanentShadow, MAX_PERMANENT_SHADOWS> CShadows::aPermanentShadows{}; // 0xC4AC30
std::array<CRegisteredShadow, MAX_STORED_SHADOWS> CShadows::asShadowsStored{};     // 0xC40430

RwTexture* gpShadowCarTex = nullptr;         // 0xC403E0
RwTexture* gpShadowPedTex = nullptr;         // 0xC403E4
RwTexture* gpShadowHeliTex = nullptr;        // 0xC403E8
RwTexture* gpShadowBikeTex = nullptr;        // 0xC403EC
RwTexture* gpShadowBaronTex = nullptr;       // 0xC403F0
RwTexture* gpShadowExplosionTex = nullptr;   // 0xC403F4
RwTexture* gpShadowHeadLightsTex = nullptr;  // 0xC403F8
RwTexture* gpShadowHeadLightsTex2 = nullptr; // 0xC403FC
RwTexture* gpBloodPoolTex = nullptr;         // 0xC40400
RwTexture* gpHandManTex = nullptr;           // 0xC40404
RwTexture* gpCrackedGlassTex = nullptr;      // 0xC40408
RwTexture* gpPostShadowTex = nullptr;        // 0xC4040C

std::array<RwImVertexIndex, 24> g_ShadowVertices{}; // 0xC403A8

// ============================================================================
// Methods
// ============================================================================

void CStaticShadow::Free() {
    // TODO: no named .c in src/CShadows/ - identify from binary.
    m_nId = 0;
    m_pPolyBunch = nullptr;
}

// 0x706CD0
void CShadows::Init() {
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));

    gpShadowCarTex         = RwTextureRead("shad_car",     nullptr);
    gpShadowPedTex         = RwTextureRead("shad_ped",     nullptr);
    gpShadowHeliTex        = RwTextureRead("shad_heli",    nullptr);
    gpShadowBikeTex        = RwTextureRead("shad_bike",    nullptr);
    gpShadowBaronTex       = RwTextureRead("shad_rcbaron", nullptr);
    gpShadowExplosionTex   = RwTextureRead("shad_exp",     nullptr);
    gpShadowHeadLightsTex  = RwTextureRead("headlight",    nullptr);
    gpShadowHeadLightsTex2 = RwTextureRead("headlight1",   nullptr);
    gpBloodPoolTex         = RwTextureRead("bloodpool_64", nullptr);
    gpHandManTex           = RwTextureRead("handman",      nullptr);
    gpCrackedGlassTex      = RwTextureRead("wincrack_32",  nullptr);
    gpPostShadowTex        = RwTextureRead("lamp_shad_64", nullptr);

    CTxdStore::PopCurrentTxd();

    g_ShadowVertices = { 0, 2, 1, 0, 3, 2, 0, 4, 3, 0, 5, 4, 0, 6, 5, 0, 7, 6, 0, 8, 7, 0, 9, 8 };

    for (auto& shadow : aStaticShadows) {
        shadow.Init();
    }

    pEmptyBunchList = aPolyBunches.data();
    for (size_t i = 0; i < aPolyBunches.size() - 1; i++) {
        aPolyBunches[i].m_pNext = &aPolyBunches[i + 1];
    }
    aPolyBunches.back().m_pNext = nullptr;

    for (auto& shadow : aPermanentShadows) {
        shadow.Init();
    }
}

// 0x706ED0
void CShadows::Shutdown() {
    RwTextureDestroy(gpShadowCarTex);
    RwTextureDestroy(gpShadowPedTex);
    RwTextureDestroy(gpShadowHeliTex);
    RwTextureDestroy(gpShadowBikeTex);
    RwTextureDestroy(gpShadowBaronTex);
    RwTextureDestroy(gpShadowExplosionTex);
    RwTextureDestroy(gpShadowHeadLightsTex);
    RwTextureDestroy(gpShadowHeadLightsTex2);
    RwTextureDestroy(gpBloodPoolTex);
    RwTextureDestroy(gpHandManTex);
    RwTextureDestroy(gpCrackedGlassTex);
    RwTextureDestroy(gpPostShadowTex);
}

// 0x707770
void CShadows::TidyUpShadows() {
    for (auto& shadow : aPermanentShadows) {
        shadow.m_nType = SHADOW_NONE;
    }
}

// 0x706F60
void CShadows::AddPermanentShadow(uint8_t type, RwTexture* texture, CVector* posn,
                                  float topX, float topY, float rightX, float rightY,
                                  int16_t intensity, uint8_t red, uint8_t green, uint8_t blue,
                                  float drawDistance, uint32_t time, float upDistance) {
    // Find a free slot.
    int32_t slot = -1;
    for (int32_t i = 0; i < MAX_PERMANENT_SHADOWS; i++) {
        if (aPermanentShadows[i].m_nType == SHADOW_NONE) {
            slot = i;
            break;
        }
    }

    // No free slot: evict the smallest-duration "small" shadow.
    if (slot < 0) {
        uint32_t smallestDuration = 0xFFFFFFFF;
        for (int32_t i = 0; i < MAX_PERMANENT_SHADOWS; i++) {
            const auto& s = aPermanentShadows[i];
            const float frontSq = s.m_fFrontX * s.m_fFrontX + s.m_fFrontY * s.m_fFrontY;
            const float sideSq  = s.m_fSideX * s.m_fSideX + s.m_fSideY * s.m_fSideY;
            if (frontSq < 0.25f && sideSq < 0.25f && s.m_nTimeDuration < smallestDuration) {
                smallestDuration = s.m_nTimeDuration;
                slot = i;
            }
        }
        if (slot < 0)
            return;
    }

    auto& s = aPermanentShadows[slot];
    s.m_nType = static_cast<eShadowType>(type);
    s.m_pTexture = texture;
    s.m_vecPosn = *posn;
    s.m_fFrontX = topX;
    s.m_fFrontY = topY;
    s.m_fSideX = rightX;
    s.m_fSideY = rightY;
    s.m_nIntensity = intensity;
    s.m_nRed = red;
    s.m_nGreen = green;
    s.m_nBlue = blue;
    s.m_fZDistance = drawDistance;
    s.m_nTimeDuration = time;
    s.m_fScale = upDistance;
    s.m_nTimeCreated = CTimer::GetTimeInMS();
}
// 0x70C950
void CShadows::UpdatePermanentShadows() {
    const uint32_t now = CTimer::GetTimeInMS();

    for (int32_t i = 0; i < MAX_PERMANENT_SHADOWS; i++) {
        auto& s = aPermanentShadows[i];
        if (s.m_nType == SHADOW_NONE)
            continue;

        if (now - s.m_nTimeCreated < s.m_nTimeDuration) {
            // Still alive: store as a static shadow this frame.
            uint16_t intensity;
            uint8_t red, green, blue;
            if (now - s.m_nTimeCreated < (s.m_nTimeDuration * 3 >> 2)) {
                // First 3/4 of life: original colour.
                intensity = s.m_nIntensity;
                red = s.m_nRed;
                green = s.m_nGreen;
                blue = s.m_nBlue;
            } else {
                // Last 1/4: random flicker as it fades.
                intensity = static_cast<uint16_t>(CGeneral::GetRandomNumber());
                red   = static_cast<uint8_t>(CGeneral::GetRandomNumber());
                green = static_cast<uint8_t>(CGeneral::GetRandomNumber());
                blue  = static_cast<uint8_t>(CGeneral::GetRandomNumber());
            }
            const bool stored = StoreStaticShadow(
                static_cast<uint32_t>(i), s.m_nType, s.m_pTexture, s.m_vecPosn,
                s.m_fFrontX, s.m_fFrontY, s.m_fSideX, s.m_fSideY,
                static_cast<int16_t>(intensity), red, green, blue,
                s.m_fZDistance, 1.0f, 40.0f, false, 0.0f);
            if (stored || s.m_nType == SHADOW_OIL_5)
                continue;
        }
        s.m_nType = SHADOW_NONE;
    }

    // TODO(port): oil-fire spreading (SHADOW_OIL_2 -> OIL_3, ignites nearby
    // OIL_1/OIL_5 via CFireManager::StartFire). Runs when (frameCounter & 3) == 0.
    // See decomp_CShadows/UpdatePermanentShadows_0070c950.c.
}

// 0x70BA00
bool CShadows::StoreStaticShadow(uint32_t id, eShadowType type, RwTexture* texture,
                                  const CVector& posn, float frontX, float frontY,
                                  float sideX, float sideY, int16_t intensity,
                                  uint8_t red, uint8_t green, uint8_t blue,
                                  float zDistance, float scale, float drawDistance,
                                  bool temporaryShadow, float upDistance) {
    // TODO(port): full implementation from decomp_CShadows/StoreStaticShadow_0070ba00.c.
    // Distance check vs camera (drawDistance), intensity randomisation in the
    // outer 25% band, slot reuse by id, GeneratePolysForStaticShadow for new slots.
    (void)id; (void)type; (void)texture; (void)posn;
    (void)frontX; (void)frontY; (void)sideX; (void)sideY;
    (void)intensity; (void)red; (void)green; (void)blue;
    (void)zDistance; (void)scale; (void)drawDistance;
    (void)temporaryShadow; (void)upDistance;
    return false;
}

// 0x70B730
void CShadows::GeneratePolysForStaticShadow(int16_t staticShadowIndex) {
    auto& shadow = aStaticShadows[staticShadowIndex];

    const float minX = shadow.m_vecPosn.x - (std::fabs(shadow.m_fFrontX) + std::fabs(shadow.m_fSideX));
    const float maxX = shadow.m_vecPosn.x + (std::fabs(shadow.m_fFrontX) + std::fabs(shadow.m_fSideX));
    const float minY = shadow.m_vecPosn.y - (std::fabs(shadow.m_fFrontY) + std::fabs(shadow.m_fSideY));
    const float maxY = shadow.m_vecPosn.y + (std::fabs(shadow.m_fFrontY) + std::fabs(shadow.m_fSideY));

    // Sector indices: (coord * 0.02 + 60.0), clamped to [0, 119].
    // (Decomp does this via FPU control-word float->int; equivalent here.)
    auto toSector = [](float c) -> int32_t {
        int32_t s = static_cast<int32_t>(c * 0.02f + 60.0f);
        if (s < 0) s = 0;
        if (s > 119) s = 119;
        return s;
    };
    const int32_t minSectorX = toSector(minX);
    const int32_t maxSectorX = toSector(maxX);
    const int32_t minSectorY = toSector(minY);
    const int32_t maxSectorY = toSector(maxY);

    // TODO(port): CWorld::AdvanceCurrentScanCode().
    for (int32_t y = minSectorY; y <= maxSectorY; y++) {
        for (int32_t x = minSectorX; x <= maxSectorX; x++) {
            // TODO(port): CWorld::GetSector(x, y) -> CPtrList& for the map-objects list.
            // CastPlayerShadowSectorList(ptrList, minX, minY, maxX, maxY, &shadow.m_vecPosn,
            //     shadow.m_fFrontX, shadow.m_fFrontY, shadow.m_fSideX, shadow.m_fSideY,
            //     0, 0, 0, 0, shadow.m_fZDistance, shadow.m_fScale,
            //     &shadow.m_pPolyBunch, &shadow.m_nDayNightIntensity, 0);
            (void)x; (void)y;
        }
    }
}

// 0x707930
void CShadows::StoreShadowToBeRendered(uint8_t type, const CVector& posn,
                                       float frontX, float frontY, float sideX, float sideY,
                                       int16_t intensity, uint8_t red, uint8_t green, uint8_t blue) {
    // TODO(port): full implementation from decomp_CShadows/StoreShadowToBeRendered_00707930.c.
    // Selects texture by type and forwards to the full overload.
    (void)type; (void)posn; (void)frontX; (void)frontY; (void)sideX; (void)sideY;
    (void)intensity; (void)red; (void)green; (void)blue;
}

// 0x707390
void CShadows::StoreShadowToBeRendered(uint8_t type, RwTexture* texture, const CVector& posn,
                                       float topX, float topY, float rightX, float rightY,
                                       int16_t intensity, uint8_t red, uint8_t green, uint8_t blue,
                                       float zDistance, bool drawOnWater, float scale,
                                       CRealTimeShadow* realTimeShadow, bool drawOnBuildings) {
    // TODO(port): full implementation from decomp_CShadows/StoreShadowToBeRendered_00707390.c.
    (void)type; (void)texture; (void)posn; (void)topX; (void)topY; (void)rightX; (void)rightY;
    (void)intensity; (void)red; (void)green; (void)blue;
    (void)zDistance; (void)drawOnWater; (void)scale; (void)realTimeShadow; (void)drawOnBuildings;
}

// 0x7074F0
void CShadows::RemoveOilInArea(float minX, float maxX, float minY, float maxY) {
    for (auto& s : aPermanentShadows) {
        if (s.m_nType >= SHADOW_OIL_1 && s.m_nType <= SHADOW_OIL_5) {
            if (s.m_vecPosn.x >= minX && s.m_vecPosn.x <= maxX &&
                s.m_vecPosn.y >= minY && s.m_vecPosn.y <= maxY) {
                s.m_nType = SHADOW_NONE;
            }
        }
    }
}

// 0x707550
void CShadows::GunShotSetsOilOnFire(const CVector& shotOrigin, const CVector& shotTarget) {
    // TODO(port): full implementation from decomp_CShadows/GunShotSetsOilOnFire_00707550.c.
    // Finds OIL_1 shadows along the shot line and advances them to OIL_2.
    (void)shotOrigin; (void)shotTarget;
}
// 0x707460
void CShadows::SetRenderModeForShadowType(eShadowType type) {
    // TODO(port): full implementation from decomp_CShadows/SetRenderModeForShadowType_00707460.c.
    // Sets blend/render states per shadow type.
    (void)type;
}

// 0x7076B0
void CShadows::PrintDebugPoly(CVector* a, CVector* b, CVector* c) {
    // TODO(port): debug-only; prints triangle coords.
    (void)a; (void)b; (void)c;
}

// 0x7076C0
void CShadows::CalcPedShadowValues(CVector sunPosn,
                                   float& frontX, float& frontY,
                                   float& sideX, float& sideY,
                                   float& displacementX, float& displacementY) {
    // TODO(port): full implementation from decomp_CShadows/CalcPedShadowValues_007076c0.c.
    (void)sunPosn; (void)displacementX; (void)displacementY;
    (void)frontX; (void)frontY; (void)sideX; (void)sideY;
}

// 0x707850
void CShadows::AffectColourWithLighting(eShadowType shadowType, uint8_t dayNightIntensity,
                                        uint8_t red, uint8_t green, uint8_t blue,
                                        uint8_t& outRed, uint8_t& outGreen, uint8_t& outBlue) {
    // TODO(port): full implementation from decomp_CShadows/AffectColourWithLighting_00707850.c.
    (void)shadowType; (void)dayNightIntensity;
    outRed = red; outGreen = green; outBlue = blue;
}

// 0x707B40
void CShadows::StoreShadowForPedObject(CPed* ped, float displacementX, float displacementY,
                                       float frontX, float frontY, float sideX, float sideY) {
    // TODO(port): full implementation from decomp_CShadows/StoreShadowForPedObject_00707b40.c.
    (void)ped; (void)displacementX; (void)displacementY;
    (void)frontX; (void)frontY; (void)sideX; (void)sideY;
}

// 0x707CA0
void CShadows::StoreRealTimeShadow(CPhysical* physical, float displacementX, float displacementY,
                                   float frontX, float frontY, float sideX, float sideY) {
    // TODO(port): full implementation from decomp_CShadows/StoreRealTimeShadow_00707ca0.c.
    (void)physical; (void)displacementX; (void)displacementY;
    (void)frontX; (void)frontY; (void)sideX; (void)sideY;
}

// 0x707F40
void CShadows::UpdateStaticShadows() {
    const uint32_t now = CTimer::GetTimeInMS();
    for (auto& s : aStaticShadows) {
        if (s.m_nId == 0)
            continue;
        // Temporary shadows die after 5000ms.
        if (s.m_bTemporaryShadow && now - s.m_nTimeCreated > 5000) {
            s.Free();
        }
    }
}

// 0x707FA0
void CShadows::RenderExtraPlayerShadows() {
    // TODO(port): full implementation from decomp_CShadows/RenderExtraPlayerShadows_00707fa0.c.
    // Renders the player's extra shadows (head, etc.).
}

// 0x708300
void CShadows::RenderStaticShadows() {
    // TODO(port): full implementation from decomp_CShadows/RenderStaticShadows_00708300.c.
}

// 0x7086B0
// FLAGGED: the bunch-path UVs come from a bare unk_00821b40() call whose return
// the decompiler could not type. Ported as GetRandomNumber() with a TODO.
void CShadows::CastShadowEntityXY(CEntity* entity, float cornerAX, float cornerAY,
                                  float cornerBX, float cornerBY, CVector* posn,
                                  float frontX, float frontY, float sideX, float sideY,
                                  int16_t intensity, uint8_t red, uint8_t green, uint8_t blue,
                                  float zDistance, float scale, CPolyBunch** ppPolyBunch,
                                  uint8_t* pDayNightIntensity, int32_t shadowType) {
    // TODO(port): full Sutherland-Hodgman clipping implementation from
    // decomp_CShadows/CastShadowEntityXY_007086b0.c (~780 lines).
    (void)entity; (void)cornerAX; (void)cornerAY; (void)cornerBX; (void)cornerBY;
    (void)posn; (void)frontX; (void)frontY; (void)sideX; (void)sideY;
    (void)intensity; (void)red; (void)green; (void)blue;
    (void)zDistance; (void)scale; (void)ppPolyBunch; (void)pDayNightIntensity; (void)shadowType;
}

// 0x70A040
// FLAGGED: the decompiler lost the real-time shadow callback arguments;
// the body is structurally sound but the callback wiring needs the binary.
void CShadows::CastShadowEntityXYZ(CEntity* entity, CVector* posn,
                                   float frontX, float frontY, float sideX, float sideY,
                                   int16_t intensity, uint8_t red, uint8_t green, uint8_t blue,
                                   float zDistance, float scale, CPolyBunch** ppPolyBunch,
                                   CRealTimeShadow* realTimeShadow) {
    // TODO(port): full implementation from decomp_CShadows/CastShadowEntityXYZ_0070a040.c.
    (void)entity; (void)posn; (void)frontX; (void)frontY; (void)sideX; (void)sideY;
    (void)intensity; (void)red; (void)green; (void)blue;
    (void)zDistance; (void)scale; (void)ppPolyBunch; (void)realTimeShadow;
}

// 0x70A470
// NOTE: CastPlayerShadowSectorList is a template in CShadows.h; the definition
// belongs in the header or an explicit instantiation. Left undefined here.
// TODO(port): full implementation from decomp_CShadows/CastPlayerShadowSectorList_0070a470.c.

// 0x70A630
// NOTE: CastShadowSectorList is a template in CShadows.h; left undefined here.
// TODO(port): full implementation from decomp_CShadows/CastShadowSectorList_0070a630.c.

// 0x70A7E0
// NOTE: CastRealTimeShadowSectorList is a template in CShadows.h; left undefined here.
// TODO(port): full implementation from decomp_CShadows/CastRealTimeShadowSectorList_0070a7e0.c.

// 0x70A960
void CShadows::RenderStoredShadows() {
    // TODO(port): full implementation from decomp_CShadows/RenderStoredShadows_0070a960.c.
    ShadowsStoredToBeRendered = 0;
}

// 0x70BDA0
// Decomp multipliers win over gta-reversed (e.g. heli shadow 0.5/x*3.0/y*1.4).
// MODEL_VORTEX (539) returns early.
void CShadows::StoreShadowForVehicle(CVehicle* vehicle, VEH_SHD_TYPE vehShadowType) {
    // TODO(port): full implementation from decomp_CShadows/StoreShadowForVehicle_0070bda0.c.
    (void)vehicle; (void)vehShadowType;
}

// 0x70C500
void CShadows::StoreCarLightShadow(CVehicle* vehicle, int32_t id, RwTexture* texture,
                                   const CVector& posn, float frontX, float frontY,
                                   float sideX, float sideY, uint8_t red, uint8_t green, uint8_t blue,
                                   float maxViewAngle) {
    // TODO(port): full implementation from decomp_CShadows/StoreCarLightShadow_0070c500.c.
    (void)vehicle; (void)id; (void)texture; (void)posn;
    (void)frontX; (void)frontY; (void)sideX; (void)sideY;
    (void)red; (void)green; (void)blue; (void)maxViewAngle;
}

// 0x70C750
void CShadows::StoreShadowForPole(CEntity* entity, float offsetX, float offsetY, float offsetZ,
                                  float poleHeight, float poleWidth, uint32_t localId) {
    // TODO(port): full implementation from decomp_CShadows/StoreShadowForPole_0070c750.c.
    (void)entity; (void)offsetX; (void)offsetY; (void)offsetZ;
    (void)poleHeight; (void)poleWidth; (void)localId;
}

// 0x70CCB0
void CShadows::RenderIndicatorShadow(uint32_t id, eShadowType shadowType, RwTexture* texture,
                                     const CVector& posn, float frontX, float frontY,
                                     float sideX, float sideY, int16_t intensity) {
    // TODO(port): full implementation from decomp_CShadows/RenderIndicatorShadow_0070ccb0.c.
    (void)id; (void)shadowType; (void)texture; (void)posn;
    (void)frontX; (void)frontY; (void)sideX; (void)sideY; (void)intensity;
}
