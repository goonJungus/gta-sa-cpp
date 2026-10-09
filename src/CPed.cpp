// CPed.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CPed/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CPed.h"
#include <cstring> // memcpy (decomp float byte-access)

// TODO(port): original #define value; 2500.0f is a placeholder (50^2).
// Used by CPed::PreRenderAfterTest.
static constexpr float _MAX_DISTANCE_PED_SHADOWS_SQR = 2500.0f;

// Local audio event constants (gta-reversed eAudioEvents.h values).
// Defined here because shared eAudioEvents.h is owned by another worker.
// TODO(port): remove when eAudioEvents.h has the full enum.
#ifndef AE_PED_FOOTSTEP_LEFT
#define AE_PED_FOOTSTEP_LEFT 54
#define AE_PED_FOOTSTEP_RIGHT 55
#define AE_PED_SKATE_LEFT 56
#define AE_PED_SKATE_RIGHT 57
#endif

// Local task type constant (gta-reversed value 0xF2).
// Defined here because shared CPedIntelligence.h is owned by another worker.
// TODO(port): remove when CPedIntelligence.h has TASK_SIMPLE_LAND.
#define AE_SPEECH_PED 0 // TODO(port): remove when eAudioEvents.h has it
#define AE_PED_KNOCK_DOWN 0 // TODO(port): remove when eAudioEvents.h has it (value TBD)
#define AE_PED_CRUNCH 0 // TODO(port): remove when eAudioEvents.h has it (value TBD)
#define AE_PED_BOUNCE 0 // TODO(port): remove when eAudioEvents.h has it (value TBD)
#ifndef TASK_SIMPLE_LAND
#define TASK_SIMPLE_LAND 242
#endif

// Decomp DAT_ globals (original static data).
// TODO(port): verify values from the original binary.
static constexpr float _DAT_008d1378 = 0.0f; // TODO(port): unknown value
static constexpr float _DAT_008d137c = 0.0f; // TODO(port): unknown value
static constexpr float _DAT_008d1380 = 0.0f; // TODO(port): unknown value
static constexpr float _DAT_008d21f0 = 0.0f; // TODO(port): unknown value
static float _DAT_00b6f180 = 0.0f; // TODO(port): unknown value
static int32_t DAT_00a9ae30 = 0; // TODO(port): unknown value
static int32_t DAT_00a9ae38 = 0; // TODO(port): unknown value
static float _DAT_008d22a8 = 0.0f; // TODO(port): value from binary unknown
static float _DAT_008d22ac = 0.0f; // TODO(port): value from binary unknown
static float _DAT_008d22a4 = 0.0f; // TODO(port): value from binary unknown
static float DAT_008d22a0 = 1.0f; // TODO(port): value from binary unknown; 1.0f to avoid div-by-zero
#include "CCamera.h" // CCamera, TheCamera, SetNewPlayerWeaponMode
#include "CAutomobile.h" // CAutomobile, RemoveBonnetInPedCollision
#include "CCrime.h" // CCrime::ReportCrime
#include "CWeaponInfo.h" // CWeaponInfo::GetWeaponInfo, m_nSlot, m_nModelId1
#include "CGeneral.h" // CGeneral::GetRandomNumber (was unk_00821b40 in decomp)
#include "CPlayerPedData.h" // CPlayerPedData (full type; was forward-declared)
#include "CGame.h" // CGame::currArea (minimal)
#include "CWorld.h" // FindPlayerPed, FindPlayerCoors, CWorld::bForceProcessControl
#include "CTimer.h" // CTimer::m_FrameCounter
#include "CShadows.h" // CShadows::AddPermanentShadow, gpBloodPoolTex
#include "CCollision.h" // CCollision::ProcessColModels
#include "CModelInfo.h" // CModelInfo::ms_modelInfoPtrs
#include "CObject.h" // CObject (GiveObjectToPedToHold)
#include "CTaskSimpleLand.h" // CTaskSimpleLand (PlayFootSteps)
#include "CPedClothesDesc.h" // CPedClothesDesc (PlayFootSteps)
#include "CVehicle.h" // CVehicle (full type)
#include "CPlayerPed.h" // CPlayerPed (full type)
#include "FxManager.h" // FxManager_c, g_fxMan
#include "FxSystem.h" // FxSystem_c
#include "CVisibilityPlugins.h" // CVisibilityPlugins (minimal)
#include "CLocalisation.h" // CLocalisation::Blood (minimal)
#include "CPopulation.h" // CPopulation::UpdatePedCount (minimal)
#include "CPedGroups.h" // CPedGroups::ms_groups, CPedGroupMembership (minimal)
#include "CPostEffects.h" // CPostEffects::IsVisionFXActive etc.
#include "CMirrors.h" // CMirrors::bRenderingReflection (minimal)
#include "CRenderer.h" // CRenderer::SetupLightingForEntity
#include "CPointLights.h" // CPointLights, ActivateDirectional, SetAmbientColours
#include "CColLine.h" // CColLine (full type)
#include "CEventDamage.h" // CEventDamage, CPedDamageResponseCalculator
#include "CTaskSimpleUseGun.h" // CTaskSimpleUseGun::m_WeaponInfo
#include "CTaskSimpleJetPack.h" // CTaskSimpleJetPack::RenderJetPack
#include "CCoverPoint.h" // CCoverPoint::ReleaseCoverPointForPed
#include "CTempColModels.h" // CTempColModels::ms_colModelPed2
#include "CRadar.h" // CRadar::ClearBlipForEntity, BLIP_CHAR
#include "Pools.h" // GetPedPool
#include "CCustomBuildingDNPipeline.h" // CCustomBuildingDNPipeline::m_fDNBalanceParam
#include "CSurfaceInfos.h" // g_surfaceInfos, CSurfaceInfos::IsSteepSlope
#include "Hoodlum.h" // Hoodlum::unk_015637e0 (stub)
#include "CGameLogic.h" // CGameLogic (stub)
#include "CPedSaveStructure.h" // CPedSaveStructure (stub)
#include "CGenericGameStorage.h" // CGenericGameStorage (stub)
#include "CAnimBlendAssociation.h" // CAnimBlendAssociation (full type)
#include "CAnimManager.h" // CAnimManager::BlendAnimation
#include "CTask.h"
#include "CTaskComplex.h" // CTaskComplex (minimal stub) // CTask (complete type for task casts)
#include "CTaskComplexFacial.h" // CTaskComplexFacial (minimal stub)
#include "CTaskSimpleStandStill.h" // CTaskSimpleStandStill (minimal stub)
#include "CEventAcquaintancePed.h" // CEventAcquaintancePed/Hate (minimal stub)
#include "CCheat.h" // CCheat::m_aCheatsActive (minimal stub)
#include "CPedType.h" // CPedType::GetPedTypeAcquaintances/GetPedFlag
#include "CPickup.h" // ePickupType::PICKUP_ONCE_TIMEOUT
#include "CReplay.h" // CReplay (minimal stub)
#include "CTheScripts.h" // CTheScripts (adapted)
#include "CConversations.h" // CConversations/CPedToPlayerConversations (minimal stubs)
#include "CPickups.h" // CPickups (adapted)
#include "CCarEnterExit.h" // CCarEnterExit (adapted)
#include "CPedStats.h" // CPedStats (minimal stub)
#include "CStreaming.h" // CStreaming::SetMissionDoesntRequireModel
#include "CStats.h" // CStats::GetStatValue (includes eStats.h)
#include "CBike.h" // CBike
#include "tHandlingData.h" // tHandlingData
#include "CTaskSimpleHoldEntity.h" // CTaskSimpleHoldEntity (minimal stub)
#include "CBuoyancy.h" // CBuoyancy, mod_Buoyancy
#include "CPathFind.h" // CPathFind, CPathNode, ThePaths (minimal stubs)
#include "CPedPlacement.h" // CPedPlacement (minimal stub)
#include "CPad.h" // CPad, GetPad
#include "CTimeCycle.h" // CTimeCycle (minimal stub)
#include "CWeather.h" // CWeather (minimal stub)
#include "CCullZones.h" // CCullZones (minimal stub)
#include "FxManager.h" // FxManager_c, g_fxMan, g_fx
#include "CEventHitByWaterCannon.h" // CEventHitByWaterCannon (minimal stub)
#include "CWaterLevel.h" // CWaterLevel::GetWaterLevel (added 2026-10-09 for CPed)
#include "CAudioEngine.h" // CAudioEngine::ReportWaterSplash, AudioEngine (added 2026-10-09 for CPed)
#include "Fx.h" // Fx_c::TriggerWaterSplash, g_fx (added 2026-10-09 for CPed)
#include "CTaskSimpleSwim.h" // CTaskSimpleSwim::m_fSwimStopTime (added 2026-10-09 for CPed)

// TODO(port): decomp globals/macros used by the CPed TU
// TODO(port): unidentified decomp functions (added 2026-10-09)
// TODO(port): decomp data addresses (added 2026-10-09)
inline float _DAT_008d21ec = 0.0f;
inline float _DAT_008d21e8 = 0.0f;
inline void unk_005e3630(uint32_t arg) { (void)arg; }
inline void unk_005e37c0(int32_t arg) { (void)arg; }
// VectorSub: decomp vector subtract helper (added 2026-10-09)
inline float* VectorSub(void* out, const void* a, const void* b) {
    float* fo = (float*)out; const float* fa = (const float*)a; const float* fb = (const float*)b;
    fo[0] = fa[0] - fb[0]; fo[1] = fa[1] - fb[1]; fo[2] = fa[2] - fb[2];
    return fo;
}
// CONCAT31: decomp byte-merge macro (added 2026-10-09)
#define CONCAT31(a,b) ((uint32_t)(((uint32_t)(b) << 24) | ((uint32_t)(a) & 0x00FFFFFF)))
#define LOCK() ((void)0)   // TODO(port): decomp critical-section enter; no-op until threading lands
#define UNLOCK() ((void)0) // TODO(port): decomp critical-section leave; no-op until threading lands
// TODO(port): decomp global DAT_008a5f50; uint16 table indexed by eWeaponType, real identity pending
static uint16_t DAT_008a5f50[80]{};
static char DAT_008d21e0{}; // TODO(port): decomp global; real identity pending (added 2026-10-09)
static int32_t DAT_008d1370{}; // TODO(port): decomp global; real identity pending (added 2026-10-09)
static CVector DAT_008d232c{}; // TODO(port): decomp global used as RwMatrixRotate axis; real identity pending (added 2026-10-09)
static CVector DAT_008d13a8[64]{}; // TODO(port): decomp global CVector array; real identity pending (added 2026-10-09)
#include <algorithm> // std::min
#include <array> // std::array (DAT_00c092a8 scratch)
#include <bit> // std::bit_cast
#include <cmath> // std::abs, std::sqrt, std::floor, atan2f
#include <new> // placement new

// TODO(port): 0x822130 - pow(base, exp) helper. The decomp named it
// FxInterpInfo_c::unk_00822130 ("no distinguishing evidence" for the name);
// its body calls the pow helper, but base/exponent were passed on the FPU
// stack and not recovered. Declared unresolved; static lib needs no definition.
namespace FxInterpInfo_c { float unk_00822130(); }

// TODO(port): unresolved decomp globals.
//   DAT_00b6f03c (0xB6F03C): pointer global, null-checked; +0x30 used as a
//     position when non-null. Identity unknown.
//   DAT_00b6f02c (0xB6F02C): fallback position vector. Identity unknown.

// TODO(renderware): minimal RwEngineInstance stub (RenderWare unported).
//   Used for render-state get/set in CPed::Render.
struct RwDevice {
    void (*fpRenderStateGet)(int32_t, void*);
    void (*fpRenderStateSet)(int32_t, int32_t);
};
struct RwEngineInstanceType {
    RwDevice dOpenDevice;
};
// TODO(renderware): stub definition; null until RenderWare ports.
static RwEngineInstanceType* RwEngineInstance = nullptr;

// TODO(port): helper to access CEntity's protected m_pStreamingLink through
//   a base-class pointer (C++ only allows protected access via derived type).
//   Used for decomp's colPhysical[1].m_pStreamingLink pattern.
struct CEntityProtectedAccess : CEntity {
    static CLink<CEntity*>* GetStreamingLink(const CEntity* e) {
        return static_cast<const CEntityProtectedAccess*>(e)->m_pStreamingLink;
    }
};

// (forward declarations for RpClump/CTaskSimpleJetPack/CCoverPoint/CTaskSimpleUseGun/
//   CColLine removed 2026-10-09: the full headers are included above.)

// TODO(audio): minimal ped-speech context IDs (from gta-reversed
//   Audio/Enums/PedSpeechContexts.h). Full enum when audio batch lands.
enum : int32_t {
    CTX_GLOBAL_NO_SPEECH = 0,
    CTX_GLOBAL_DRUGGED_CHAT = 81,
    CTX_GLOBAL_PAIN_DEATH_HIGH = 342,
    CTX_GLOBAL_PAIN_LOW = 345,
};



//   Stubbed (null pointer + zero vector): blood shadows only spawn near the
//   world origin until the real globals are identified.
static uint32_t DAT_00b6f03c = 0;
static CVector DAT_00b6f02c{};

// TODO(port): DAT_00c092a8 (0xC092A8) - scratch array of 32 CColPoint used as
// the sphereCPs out-param of CCollision::ProcessColModels. Identity unknown;
// stubbed as a file-local array.
static std::array<CColPoint, 32> DAT_00c092a8{};

// TODO(port): _unk_00858ca0 (0x858CA0) - float global; added to positions,
//   compared against move blend ratios. Identity unknown.
static float _unk_00858ca0 = 0.0f;
// TODO(port): _DAT_00b6f118 (0xB6F118) - float global; used squared in a
//   distance check. Identity unknown.
static float _DAT_00b6f118 = 0.0f;
// TODO(port): DAT_00b6f081 (0xB6F081): byte global used as an index into
//   decomp float tables (stride 0x8e). Identity unknown.
static uint8_t DAT_00b6f081 = 0;
// TODO(port): DAT_00b6f32c/DAT_00b6f330 (0xB6F32C/0xB6F330): float tables indexed
//   by DAT_00b6f081*0x8e; two floats feed an atan2 heading. Identity unknown.
static float DAT_00b6f32c = 0.0f;
static float _DAT_008d21e4 = 0.0f; // TODO(port): decomp global at 0x008d21e4
static float DAT_00b6f330 = 0.0f;

void CPed::SetModelIndex(uint32_t modelIndex) {    // converted from decomp src/CPed/SetModelIndex_005e47c0.c
    SetIsVisible(true);
    CEntity::SetModelIndex(modelIndex);
    // TODO(port): RpAnimBlendClumpInit((RpClump*)GetRwObject()) (RenderWare unported).
    // TODO(port): RpAnimBlendClumpFillFrameArray((RpClump*)GetRwObject(), m_apBones) (RenderWare unported).
    // TODO(port): CPedModelInfo* mi = (CPedModelInfo*)CModelInfo::ms_modelInfoPtrs[m_nModelIndex] (CModelInfo unported).
    // TODO(port): m_pStats = &CPedStats::ms_apPedStats[mi->m_nStatType]; m_fHeadingChangeRate = m_pStats->m_fHeadingChangeRate (CPedStats unported).
    int32_t decisionMaker;
    if (m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2) {
        decisionMaker = -2;
    } else if (m_nCreatedBy == PED_MISSION) {
        decisionMaker = -1;
    } else {
        // TODO(port): decisionMaker = m_pStats->m_nDefaultDecisionMaker (CPedStats unported).
        decisionMaker = -1;
    }
    m_pIntelligence->SetPedDecisionMakerType(decisionMaker);
    // TODO(port): money count from CPopCycle::IsPedInGroup / CGeneral::GetRandomNumber (both unported).
    // Decomp logic: if (!IsPedInGroup(modelIndex, POPCYCLE_GROUP_BUSINESS) && !IsPedInGroup(modelIndex, POPCYCLE_GROUP_CASUAL_RICH))
    //                   m_nMoneyCount = rand() % 0x19;
    //               else m_nMoneyCount = (rand() % 0x32) + 0x14;
    //               if (CGeneral::GetRandomNumber() < 3) m_nMoneyCount = 400;
    // TODO(port): m_nAnimGroup = mi->m_nAnimType; CAnimManager::AddAnimation((RpClump*)GetRwObject(), m_nAnimGroup, ANIM_ID_IDLE) (unported).
    if (m_nPedState == PEDSTATE_DRIVING || m_nPedState == PEDSTATE_DRAGGED_FROM_CAR || bIsDucking) {
        // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
    }
    // TODO(port): *(CVector2D**)(*(int*)(&GetRwObject()->type + ClumpOffset) + 0xc) = &m_vecAnimMovingShiftLocal (RenderWare unported).
    // TODO(port): if (!mi->m_pHitColModel) CPedModelInfo::CreateHitColModelSkinned(mi, (RpClump*)GetRwObject()) (unported).
    // TODO(port): CEntity::UpdateRpHAnim() (signature differs locally; RenderWare unported).
}










void CPed::DeleteRwObject() {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone DeleteRwObject; body from gta-reversed Ped.cpp.
    CEntity::DeleteRwObject();
}












void CPed::ProcessControl() {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        uint8_t *puVar1;
        CWeapon *piVar2;
        uint32_t uVar3;
        short sVar4;
        short sVar5;
        CVehicle *pCVar6;
        CPlayerPedData *pCVar7;
        CEntity *pCVar8;
        uint32_t uVar9;
        uint32_t uVar10;
        uint32_t uVar11;
        CMatrixLink *pCVar12;
        uint32_t uVar13;
        float topX;
        bool bVar14;
        short sVar15;
        CTaskSimpleSwim *pCVar16;
        RpHAnimHierarchy *pRVar17;
        int iVar18;
        int iVar19;
        FxSystem_c *this_00;
        CVector *pCVar20;
        uint32_t uVar21;
        uint32_t uVar22;
        CPlayerPed *pCVar23;
        float *pfVar24;
        CSimpleTransform *pCVar25;
        uint32_t uVar26;
        uint32_t uVar27;
        uint8_t uVar28;
        uint32_t uVar29;
        uint32_t uVar30;
        uint32_t *puVar31;
        uint32_t uVar32;
        uint8_t bVar33;
        uint32_t uVar34;
        uint32_t uVar35;
        long double /* Ghidra float10 */ fVar36;
        float upDistance;
        CVector local_c;

        this->m_pedAudio.Service();
        if (((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) &&
        (piVar2 = &m_aWeapons[m_nActiveWeaponSlot], piVar2->m_Type == (eWeaponType)0x12))
        {
            if (((((bDonePositionOutOfCollision & 2) == 0) &&
            ((char)GetUsesCollision() < '\0')) &&
            (((m_nPhysicalFlags & 0x100) == 0 ||
            (pCVar16 = m_pIntelligence->GetTaskSwim(),
            pCVar16 == nullptr)))) && (this->m_pWeaponObject != nullptr)) {
                if (piVar2->m_FxSystem == 0) {
                    pRVar17 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
                    iVar18 = RpHAnimIDGetIndex(pRVar17,0x18);
                    // RpHAnimHierarchyGetMatrixArray returns RwMatrix*; keep it
                    // as intptr_t for the byte-offset arithmetic below.
                    intptr_t matrixArray = (intptr_t)RpHAnimHierarchyGetMatrixArray(pRVar17);
                    local_c.x = 0.0;
                    local_c.y = 0.0;
                    local_c.z = 0.0;
                    this_00 = g_fxMan.CreateFxSystem
                    ("molotov_flame",local_c,
                    (RwMatrix *)(matrixArray + iVar18 * 0x40),false);
                    piVar2->m_FxSystem = this_00;
                    if (this_00 != nullptr) {
                        this_00->SetLocalParticles(true);
                        this_00->CopyParentMatrix();
                        this_00->Play();
                    }
                }
            }
            else if ((FxSystem_c *)piVar2->m_FxSystem != nullptr) {
                g_fxMan.DestroyFxSystem(piVar2->m_FxSystem);
                piVar2->m_FxSystem = 0;
            }
        }
        // TODO(port): if (bit 0x200000 of CPed flags word) return; (bit mapping unverified)
        uVar26 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        if ((uVar26 & 0x1000) != 0) {
            if (CGame::currArea != 0) {
                return;
            }
            // (decomp: pCVar20 = FindPlayerCoors(&local_c,-1); adapted to CWorld.h's by-value signature)
            local_c = FindPlayerCoors(-1);
            if (950.0 < local_c.z) {
                return;
            }
        }
        // TODO(port): if ((pedTimerOffset + CTimer::m_FrameCounter & 0x1f) == 0) (CTimer unported)
        if (true) {
            PruneReferences();
        }
        iVar18 = CVisibilityPlugins::GetClumpAlpha((RpClump *)GetRwObject());
        if ((bResetWalkAnims & 8) == 0) {
            if ((iVar18 < 0xff) && (iVar18 = iVar18 + 0x10, 0xff < iVar18)) {
                iVar18 = 0xff;
            }
        }
        else {
            iVar18 = iVar18 + -8;
            if (iVar18 < 0) {
                iVar18 = 0;
            }
        }
        CVisibilityPlugins::SetClumpAlpha((RpClump *)GetRwObject(),iVar18);
        uVar34 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
        if ((((int)uVar34 < 0) &&
        ((((bIsStanding & 1) != 0 || ((uVar34 & 0x80000) != 0)) &&
        (this->m_standingOnEntity == nullptr)))) &&
        ((((uVar34 & 0x200000) == 0 && (this->field_588 == 99999.99)) &&
        (fVar36 = (long double /* Ghidra float10 */)this->m_vecMoveSpeed.Magnitude(), fVar36 < (long double /* Ghidra float10 */)0.01)))) {
            pCVar6 = this->m_pVehicle;
            if (pCVar6 == nullptr) {
                uVar34 = uVar34 & 0x7fffffff;
                bResetWalkAnims = (char)uVar34;
                bCollidedWithMyVehicle = (char)(uVar34 >> 8);
                bMiamiViceCop = (char)(uVar34 >> 0x10);
                bDontFight = (char)(uVar34 >> 0x18);
            }
            else {
                // (decomp passed CMatrix*/CColModel*; local ProcessColModels takes
                // const CMatrix&/CColModel& and std::array<CColPoint,32>&)
                iVar18 = CCollision::ProcessColModels
                (*this->m_matrix,
                *CModelInfo::ms_modelInfoPtrs[(short)this->m_nModelIndex]->m_pColModel,
                *pCVar6->m_matrix,
                *CModelInfo::ms_modelInfoPtrs[(short)pCVar6->m_nModelIndex]->m_pColModel,
                DAT_00c092a8,nullptr,nullptr,false);
                if (iVar18 == 0) {
                    uVar35 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                    uVar35 = uVar35 & 0x7fffffff;
                    bResetWalkAnims = (char)uVar35;
                    bCollidedWithMyVehicle = (char)(uVar35 >> 8);
                    bMiamiViceCop = (char)(uVar35 >> 0x10);
                    bDontFight = (char)(uVar35 >> 0x18);
                    this->m_pEntityIgnoredCollision = nullptr;
                }
            }
        }
        uVar29 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
        if (((uVar29 & 0x200000) != 0) &&
        ((this->GetFourthPedFlags() & 0x100) != 0)) {
            if (this->m_matrix == nullptr) {
                pCVar25 = &this->m_placement;
            }
            else {
                pCVar25 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            if ((pCVar25->m_vPosn).z + 1.5 < this->field_588) {
                uVar29 = uVar29 & 0xffdfffff;
                bResetWalkAnims = (char)uVar29;
                bCollidedWithMyVehicle = (char)(uVar29 >> 8);
                bMiamiViceCop = (char)(uVar29 >> 0x10);
                bDontFight = (char)(uVar29 >> 0x18);
            }
        }
        uVar21 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar22 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
        uVar26 = m_nPhysicalFlags;
        uVar32 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        uVar34 = this->GetFourthPedFlags();
        uVar21 = uVar21 & 0xfffefffd;
        uVar35 = uVar22 & 0xfff7ffff;
        bIsStanding = (char)uVar21;
        bInVehicle = (char)(uVar21 >> 8);
        bFiringWeapon = (char)(uVar21 >> 0x10);
        bNotAllowedToDuck = (char)(uVar21 >> 0x18);
        bResetWalkAnims = (char)uVar35;
        bCollidedWithMyVehicle = (char)(uVar35 >> 8);
        bMiamiViceCop = (char)(uVar35 >> 0x10);
        bDontFight = (char)(uVar35 >> 0x18);
        sVar4 = this->m_nWeaponGunflashAlphaMP1;
        uVar32 = uVar32 & 0x7ffffffe;
        m_nPhysicalFlags = uVar26 & 0xfffffeff;
        bDonePositionOutOfCollision = (char)uVar32;
        bKilledByStealth = (char)(uVar32 >> 8);
        bRightArmBlocked = (char)(uVar32 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar32 >> 0x18);
        this->SetFourthPedFlags(uVar34 & 0xffefbeff);
        this->field_588 = 99999.99;
        if (0 < sVar4) {
            sVar5 = this->m_nWeaponGunFlashAlphaProgMP1;
            iVar18 = CGeneral::GetRandomNumber();
            if ((uint32_t)(sVar5 * iVar18) < (uint32_t)(int)sVar4) {
                sVar15 = CGeneral::GetRandomNumber();
                this->m_nWeaponGunflashAlphaMP1 = sVar4 - sVar15 * sVar5;
            }
            else {
                this->m_nWeaponGunflashAlphaMP1 = 0;
            }
        }
        sVar4 = this->m_nWeaponGunflashAlphaMP2;
        if (0 < sVar4) {
            sVar5 = this->m_nWeaponGunFlashAlphaProgMP2;
            iVar18 = CGeneral::GetRandomNumber();
            if ((uint32_t)(sVar5 * iVar18) < (uint32_t)(int)sVar4) {
                sVar15 = CGeneral::GetRandomNumber();
                this->m_nWeaponGunflashAlphaMP2 = sVar4 - sVar15 * sVar5;
            }
            else {
                this->m_nWeaponGunflashAlphaMP2 = 0;
            }
        }
        if ((bIsStanding & 1) == 0) {
            uVar22 = uVar22 & 0xffd7ffff;
            bResetWalkAnims = (char)uVar22;
            bCollidedWithMyVehicle = (char)(uVar22 >> 8);
            bMiamiViceCop = (char)(uVar22 >> 0x10);
            bDontFight = (char)(uVar22 >> 0x18);
        }
        if ((this->m_nCreatedBy == PED_MISSION) &&
        (pCVar23 = FindPlayerPed(-1), pCVar23 != (CPlayerPed *)this)) {
            pCVar23 = FindPlayerPed(-1);
            // (decomp: CPedGroupMembership::IsMember(&ms_groups[...].m_groupMembership, this))
            bVar14 = CPedGroups::ms_groups[pCVar23->m_pPlayerData->m_nPlayerGroup].
            m_groupMembership.IsMember(this);
            if (bVar14) {
                // (decomp took &bHasBulletProofVest for a dword write; see below)
                this->SetFourthPedFlags(this->GetFourthPedFlags() & 0xfffffeff);
            }
        }
        ProcessBuoyancy();
        pCVar7 = this->m_pPlayerData;
        if (pCVar7 != nullptr) {
            if (((m_nPhysicalFlags & 0x100) == 0) || (pCVar7->m_nWaterCoverPerc < 0x33)
            ) {
                if ('\0' < (char)pCVar7->m_nWetness) {
                    uVar28 = pCVar7->m_nWetness + 0xff;
                    goto LAB_005e914c;
                }
            }
            else if ((char)pCVar7->m_nWetness < 'd') {
                uVar28 = pCVar7->m_nWetness + '\x01';
            LAB_005e914c:
                pCVar7->m_nWetness = uVar28;
            }
        }
        if ((((bIsStanding & 1) != 0) &&
            (CWorld::bForceProcessControl == false) &&
            (pCVar8 = this->m_standingOnEntity, pCVar8 != nullptr)) &&
            // (decomp used pCVar8->__anon0 / ->m_info.bits_m_nType; mapped to the
            //  local CEntity layout: m_nFlags is a public union member, m_info.m_nType
            //  via GetType(). pCVar8[0x19] is a suspicious decomp indexing artifact
            //  (0x19 * sizeof(CEntity) is out of bounds); kept verbatim with TODO.)
            // TODO(port): verify pCVar8[0x19].m_nFlags - decomp indexing looks wrong.
            (((pCVar8->m_nFlags >> 5 & 1) != 0) ||
             (((uint32_t)pCVar8->GetType() & 7) == 2 && (pCVar8[0x19].m_nFlags == 6)))) {
            this->m_nFlags = this->m_nFlags | 0x40;
            return;
        }
        this->m_pIntelligence->ProcessFirst();
        bVar33 = bIsStanding & 1;
        if ((bVar33 == 0) && (0.25 < (this->m_vecMoveSpeed).z)) {
            if (this->m_pPlayerData == nullptr) {
                // 0x40FEC0 verified from its decomp body: out[i] = scale * in[i]
                // (a vector scale). 0x822130 is a pow(base, exp) helper whose
                // base/exponent were passed on the FPU stack and not recovered.
                fVar36 = (long double /* Ghidra float10 */)FxInterpInfo_c::unk_00822130();
                local_c = this->m_vecMoveSpeed * (float)fVar36;
                this->m_vecMoveSpeed = local_c;
            }
            else {
                (this->m_vecMoveSpeed).z = 0.25;
            }
        }
        if (((((((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) ||
        (bVar33 == 0)) || (((this->m_vecMoveSpeed).x != 0.0 || ((this->m_vecMoveSpeed).y != 0.0))))
        || ((this->m_vecMoveSpeed).z != 0.0)) ||
        (((this->m_nMoveState != PEDMOVE_STILL && (this->m_nMoveState != PEDMOVE_NONE)) ||
        (((this->m_vecAnimMovingShiftLocal).x != 0.0 ||
        ((((this->m_vecAnimMovingShiftLocal).y != 0.0 || (this->m_nPedState == PEDSTATE_JUMP)) ||
        ((bIsInTheAir) != 0)))))))) ||
        (this->m_standingOnEntity != nullptr)) {
            CPhysical::ProcessControl(); // base-class version
        }
        else {
            CPhysical::SkipPhysics(); // base-class version
        }
        RequestDelayedWeapon();
        PlayFootSteps();
        uVar3 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        uVar3 = uVar3 & 0xffef7fff;
        bDonePositionOutOfCollision = (char)uVar3;
        bKilledByStealth = (char)(uVar3 >> 8);
        bRightArmBlocked = (char)(uVar3 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar3 >> 0x18);
        this->m_pIntelligence->Process();
        if (this->m_nPedState != PEDSTATE_DEAD) {
            CalculateNewVelocity();
        }
        UpdatePosition();
        // (decomp: (**(code **)((int)this->vtable + 0x5c))(); - vtable[0x5C] is
        // index 23, which is SetMoveAnim() in both the real and clean-room layouts)
        SetMoveAnim();
        uVar30 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        uVar10 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar30 = uVar30 & 0xfff0ffff;
        bDonePositionOutOfCollision = (char)uVar30;
        bKilledByStealth = (char)(uVar30 >> 8);
        bRightArmBlocked = (char)(uVar30 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar30 >> 0x18);
        if ((((uVar10 & 0x40000) != 0) || (this->field_72F != '\0')) &&
        ((bVar14 = CLocalisation::Blood(), bVar14 &&
        ((bInVehicle) == 0)))) {
            if (this->field_72F != '\0') {
                this->field_72F = this->field_72F + 0xff;
            }
            if ((CTimer::m_FrameCounter & 3) == 0) {
                puVar31 = (uint32_t *)(DAT_00b6f03c + 0x30);
                if (DAT_00b6f03c == 0) {
                    puVar31 = (uint32_t*)&DAT_00b6f02c;
                }
                if (this->m_matrix == nullptr) {
                    pCVar25 = &this->m_placement;
                }
                else {
                    pCVar25 = (CSimpleTransform *)&this->m_matrix->m_pos;
                }
                // (decomp: VectorSub(&local_c,pCVar25,puVar31); then
                // CVector::unk_00406da0() - a squared-distance check)
                local_c = pCVar25->m_vPosn - *reinterpret_cast<const CVector*>(puVar31);
                fVar36 = (long double /* Ghidra float10 */)local_c.SquaredMagnitude();
                if (fVar36 < (long double /* Ghidra float10 */)2500.0) {
                    uVar26 = CGeneral::GetRandomNumber();
                    pCVar12 = this->m_matrix;
                    topX = (float)(uVar26 & 0x7f) * 0.0015 + 0.15;
                    uVar26 = CGeneral::GetRandomNumber();
                    local_c.x = (float)(int)((uVar26 & 0x7f) - 0x40) * 0.007 + (pCVar12->m_pos).x;
                    pCVar12 = this->m_matrix;
                    uVar26 = CGeneral::GetRandomNumber();
                    upDistance = 1.0;
                    local_c.y = (float)(int)((uVar26 & 0x7f) - 0x40) * 0.007 + (pCVar12->m_pos).y;
                    local_c.z = (this->m_matrix->m_pos).z + 1.0;
                    uVar26 = CGeneral::GetRandomNumber();
                    CShadows::AddPermanentShadow
                    ('\x01',gpBloodPoolTex,&local_c,topX,0.0,0.0,-topX,0xff,200,'\0','\0',4.0,
                    (uVar26 & 0xfff) + 2000,upDistance);
                }
            }
        }
        uVar27 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar27 & 0x100) == 0) {
            bVar14 = false;
        }
        else {
            if (this->m_pVehicle == nullptr) {
                uVar27 = uVar27 & 0xfffffeff;
                bIsStanding = (char)uVar27;
                bInVehicle = (char)(uVar27 >> 8);
                bFiringWeapon = (char)(uVar27 >> 0x10);
                bNotAllowedToDuck = (char)(uVar27 >> 0x18);
                goto LAB_005e94a3;
            }
            bVar14 = true;
        }
        CPopulation::UpdatePedCount(this,bVar14);
    LAB_005e94a3:
        if (((this->m_nRandomSeed + CTimer::m_FrameCounter & 0x3fff) == 0) &&
        ((this->GetFourthPedFlags() & 0x400) != 0)) {
            Say((eGlobalSpeechContext)CTX_GLOBAL_DRUGGED_CHAT,0,1.0f,false,false,false);
        }
        if ((((m_aWeapons[m_nActiveWeaponSlot].m_Type == WEAPON_CHAINSAW) &&
        (this->m_nPedState != PEDSTATE_ATTACK)) &&
        ((bInVehicle) == 0)) &&
        (pCVar16 = m_pIntelligence->GetTaskSwim(),
        pCVar16 == nullptr)) {
            this->m_weaponAudio.AddAudioEvent(0x99);
        }
        this->m_weaponAudio.Service();
        return;
}










void CPed::Teleport(CVector destination,  bool resetRotation) {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): uint8_t-slice
        CEntity **entity;
        CTask *pCVar1;
        uint32_t uVar2;

        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR);
            if (pCVar1 == nullptr) goto LAB_005e4147;
        }
        this->m_pIntelligence->FlushImmediately(true);
    LAB_005e4147:
        CWorld::Remove((CEntity *)this);
        if (this->m_matrix == nullptr) {
            (this->m_placement).m_vPosn.x = destination.x;
            (this->m_placement).m_vPosn.y = destination.y;
            (this->m_placement).m_vPosn.z = destination.z;
        }
        else {
            (this->m_matrix->m_pos).x = destination.x;
            (this->m_matrix->m_pos).y = destination.y;
            (this->m_matrix->m_pos).z = destination.z;
        }
        uVar2 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar2 = uVar2 & 0xfffffffe;
        entity = &this->m_pDamageEntity;
        bIsStanding = (char)uVar2;
        bInVehicle = (char)(uVar2 >> 8);
        bFiringWeapon = (char)(uVar2 >> 0x10);
        bNotAllowedToDuck = (char)(uVar2 >> 0x18);
        if (*entity != nullptr) {
            (*entity)->CleanUpOldReference(entity);
        }
        *entity = nullptr;
        CWorld::Add((CEntity *)this);
        (this->m_vecMoveSpeed).x = 0.0;
        (this->m_vecMoveSpeed).y = 0.0;
        (this->m_vecMoveSpeed).z = 0.0;
        (this->m_vecTurnSpeed).x = 0.0;
        (this->m_vecTurnSpeed).y = 0.0;
        (this->m_vecTurnSpeed).z = 0.0;
        return;
}










void CPed::SpecialEntityPreCollisionStuff(CPhysical* colPhysical,  bool bIgnoreStuckCheck,  bool& bCollisionDisabled,  bool& bCollidedEntityCollisionIgnored,  bool& bCollidedEntityUnableToMove,  bool& bThisOrCollidedEntityStuck) {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        uint16_t uVar1;
        int iVar2;
        uint32_t uVar3;
        uint8_t bVar4;
        CTaskSimpleClimb *pCVar5;

        bVar4 = (uint32_t)colPhysical->GetType() & 7;
        // TODO(port): decomp packed 4 bools into iVar2 bytes and checked
        //   iVar2 < 0 (sign bit). With bool 0/1 the sign bit is never set,
        //   so this is always false; kept verbatim.
        iVar2 = (bResetWalkAnims ? 1 : 0) | (bCollidedWithMyVehicle ? 0x100 : 0) |
                (bMiamiViceCop ? 0x10000 : 0) | (bDontFight ? 0x1000000 : 0);
        if (((bVar4 == 2) && (iVar2 < 0)) &&
        (this->m_pVehicle == (CVehicle *)colPhysical)) {
            bCollisionDisabled = true;
            goto LAB_005e3e64;
        }
        if (((CPhysical *)this->m_pEntityIgnoredCollision == colPhysical) ||
        ((CPed *)colPhysical->m_pEntityIgnoredCollision == this)) {
            bCollidedEntityCollisionIgnored = true;
            uVar3 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            if (((uVar3 & 0x10) != 0) && (-1 < (int)uVar3)) goto LAB_005e3e64;
        }
        else {
            if ((this->m_pAttachedTo == colPhysical) || ((CPed *)colPhysical->m_pAttachedTo == this)) {
                bCollisionDisabled = true;
                goto LAB_005e3e64;
            }
            if ((this->m_pAttachedTo != nullptr) &&
            ((CPed *)colPhysical->m_pAttachedTo != nullptr)) {
                bCollisionDisabled = true;
                goto LAB_005e3e64;
            }
            uVar3 = colPhysical->m_nPhysicalFlags;
            if ((uVar3 & 0x20) == 0) {
                if ((uVar3 & 0xc0) == 0) {
                    if (bVar4 == 4) {
                        if ((((uint32_t)colPhysical[1].m_placement.m_vPosn.y & 0x100) == 0) ||
                        (0.66 <= (colPhysical->m_matrix->m_up).z)) {
                            if ((colPhysical->m_nModelIndex != 0x156) ||
                            ((this->m_matrix->m_pos).z <= (colPhysical->m_matrix->m_pos).z)) {
                                // TODO(port): decomp `colPhysical[1].m_pStreamingLink`
                                //   - the [1] indexing is suspicious (0x1 * sizeof(CPhysical)
                                //   out of bounds); kept verbatim. +0x14 reads a float
                                //   from the link; identity unknown.
                                if ((((*(float *)((int)CEntityProtectedAccess::GetStreamingLink(&colPhysical[1]) + 0x14) <= 0.0) &&
                                ((uVar3 & 4) == 0)) || (0.001 <= std::abs((colPhysical->m_vecMoveSpeed).x))) ||
                                ((0.001 <= std::abs((colPhysical->m_vecMoveSpeed).y) ||
                                (0.001 <= std::abs((colPhysical->m_vecMoveSpeed).z))))) {
                                    if ((colPhysical->m_nFlags >> 4 & 1) != 0) {
                                        bCollidedEntityUnableToMove = true;
                                    }
                                }
                                else {
                                    bCollidedEntityUnableToMove = true;
                                }
                                goto LAB_005e3e64;
                            }
                            bCollidedEntityCollisionIgnored = true;
                        }
                        else {
                            bCollidedEntityCollisionIgnored = true;
                        }
                    }
                    else {
                        uVar1 = colPhysical->m_nModelIndex;
                        if (((uVar1 != 0x1b9) && (uVar1 != 0x234)) && (uVar1 != 0x252)) {
                            if ((colPhysical->m_nFlags >> 4 & 1) != 0) {
                                bCollidedEntityUnableToMove = true;
                            }
                            goto LAB_005e3e64;
                        }
                        bCollidedEntityCollisionIgnored = true;
                    }
                }
                else if (bIgnoreStuckCheck) {
                    bCollidedEntityCollisionIgnored = true;
                }
                else if (((m_nFlags >> 4 & 1) != 0) ||
                ((colPhysical->m_nFlags >> 4 & 1) != 0)) {
                    bThisOrCollidedEntityStuck = true;
                }
            }
            else if (((uVar3 & 4) == 0) && ((uVar3 & 0x40000000) == 0)) {
                if (bIgnoreStuckCheck) {
                    bCollisionDisabled = true;
                }
                else if (((m_nFlags >> 4 & 1) != 0) ||
                ((colPhysical->m_nFlags >> 4 & 1) != 0)) {
                    bThisOrCollidedEntityStuck = true;
                }
            }
            else {
                bCollidedEntityUnableToMove = true;
            }
        }
        m_nPhysicalFlags = m_nPhysicalFlags | 0x1000;
    LAB_005e3e64:
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            pCVar5 = this->m_pIntelligence->GetTaskClimb();
            if (pCVar5 != nullptr) {
                m_nPhysicalFlags = m_nPhysicalFlags | 0x1000;
            }
        }
        return;
}










uint8_t CPed::SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep,  bool& unk2) {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
        float fVar1;
        float fVar2;
        float fVar3;
        uint8_t uVar4;
        long double /* Ghidra float10 */ fVar5;

        if ((this->m_pAttachedTo != nullptr) ||
        ((this->m_pPlayerData == nullptr &&
        (fVar1 = (this->m_vecMoveSpeed).z, fVar2 = (this->m_vecMoveSpeed).y,
        fVar3 = (this->m_vecMoveSpeed).x,
        (fVar1 * fVar1 + fVar2 * fVar2 + fVar3 * fVar3) * CTimer::ms_fTimeStep * CTimer::ms_fTimeStep
        < 0.09)))) {
            return '\x01';
        }
        fVar1 = (this->m_vecMoveSpeed).z;
        fVar2 = (this->m_vecMoveSpeed).y;
        fVar3 = (this->m_vecMoveSpeed).x;
        fVar1 = std::sqrt(fVar1 * fVar1 + fVar2 * fVar2 + fVar3 * fVar3) * CTimer::ms_fTimeStep;
        if (this->m_pPlayerData == nullptr) {
            std::floor((double)(fVar1 * 5.0));
            uVar4 = CGeneral::GetRandomNumber();
        }
        else {
            if (this->m_standingOnEntity == nullptr) {
                fVar1 = fVar1 * 3.3333333;
                fVar5 = (long double /* Ghidra float10 */)std::floor((double)fVar1);
                if (fVar5 < (long double /* Ghidra float10 */)_unk_00858ca0) {
                    uVar4 = CGeneral::GetRandomNumber();
                    goto LAB_005e3fbe;
                }
            }
            else {
                fVar1 = fVar1 * 6.6666665;
                fVar5 = (long double /* Ghidra float10 */)std::floor((double)fVar1);
                if (fVar5 < (long double /* Ghidra float10 */)4.0) {
                    uVar4 = CGeneral::GetRandomNumber();
                    goto LAB_005e3fbe;
                }
            }
            std::floor((double)fVar1);
            uVar4 = CGeneral::GetRandomNumber();
        }
    LAB_005e3fbe:
        if (this->m_pPlayerData != nullptr) {
            return uVar4;
        }
        this->m_fElasticity = this->m_fElasticity + this->m_fElasticity;
        return uVar4;
}










void CPed::PreRender() {    // converted from decomp src/CPed/*.c
        if (this->m_nPedState != PEDSTATE_DRIVING) {
            this->PreRenderAfterTest();
            return;
        }
        return;
}










void CPed::Render() {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        uint32_t uVar1;
        uint32_t uVar2;
        uint32_t uVar3;
        eVehicleType eVar4;
        RwFrame *pvVar5;
        long double /* Ghidra float10 */ fVar6;
        bool bVar7;
        CTask *pCVar8;
        uint32_t *puVar9;
        CTaskSimpleSwim *pCVar10;
        CTaskSimpleHoldEntity *pCVar11;
        RpHAnimHierarchy *pRVar12;
        int iVar13;
        RwMatrix *iVar14;
        CTaskSimpleJetPack *pCVar15;
        CSimpleTransform *pCVar16;
        long double /* Ghidra float10 */ fVar19;
        CPed *ped;
        uint32_t local_10;
        CVector uStack_c;

        local_10 = 1;
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            (*(RwEngineInstance->dOpenDevice).fpRenderStateGet)(0x1e,&local_10);
            (*(RwEngineInstance->dOpenDevice).fpRenderStateSet)(0x1e,1);
        }
        if (((bDonePositionOutOfCollision & 2) == 0) &&
        (((char)GetUsesCollision() < '\0' ||
        ((CMirrors::bRenderingReflection != false && (CMirrors::_TypeOfMirror != 2)))))) {
            uVar2 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            if (((uVar2 & 0x100) != 0) &&
            (((this->m_pVehicle != nullptr &&
            (pCVar8 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR),
            pCVar8 == nullptr)) &&
            (pCVar8 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP),
            pCVar8 == nullptr)))) {
                uVar3 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
                if ((uVar3 & 0x2000) == 0) {
                    return;
                }
                eVar4 = this->m_pVehicle->m_nVehicleType;
                if (((eVar4 != VEHICLE_TYPE_BIKE) &&
                (this->m_pVehicle->m_nVehicleSubType != VEHICLE_TYPE_QUAD)) &&
                (bVar7 = this->IsPlayer(), !bVar7)) {
                    pCVar16 = (CSimpleTransform *)&this->m_matrix->m_pos;
                    if (this->m_matrix == nullptr) {
                        pCVar16 = &this->m_placement;
                    }
                    if (DAT_00b6f03c == 0) {
                        puVar9 = (uint32_t*)&DAT_00b6f02c;
                    }
                    else {
                        puVar9 = (uint32_t *)(DAT_00b6f03c + 0x30);
                    }
                    // (decomp: VectorSub(&uStack_c,puVar9,pCVar16); then
                    // CVector::unk_00406da0() - a squared-distance check)
                    uStack_c = *reinterpret_cast<const CVector*>(puVar9) - pCVar16->m_vPosn;
                    fVar19 = (long double /* Ghidra float10 */)uStack_c.SquaredMagnitude();
                    if (eVar4 == VEHICLE_TYPE_BOAT) {
                        fVar6 = (long double /* Ghidra float10 */)40.0;
                    }
                    else {
                        fVar6 = (long double /* Ghidra float10 */)25.0;
                    }
                    if ((long double /* Ghidra float10 */)_DAT_00b6f118 * fVar6 * (long double /* Ghidra float10 */)_DAT_00b6f118 * fVar6 < fVar19) {
                        return;
                    }
                }
            }
            bVar7 = CPostEffects::IsVisionFXActive();
            if (bVar7) {
                CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects(this);
                CPostEffects::NightVisionSetLights();
                this->CEntity::Render();
                CPostEffects::InfraredVisionRestoreLightsForHeatObjects();
            }
            else {
                this->CEntity::Render();
            }
            bVar7 = true;
            if ((this->m_pPlayerData != nullptr) &&
            ((this->m_pPlayerData->m_nPlayerFlags & 0x1000) == 0)) {
                bVar7 = false;
            }
            if ((((this->m_pWeaponObject != nullptr) && (bVar7)) &&
            ((((m_nPhysicalFlags & 0x100) == 0 ||
            (pCVar10 = m_pIntelligence->GetTaskSwim(),
            pCVar10 == nullptr)) &&
            (pCVar11 = this->m_pIntelligence->GetTaskHold(false),
            pCVar11 == nullptr)))) &&
            ((CVisibilityPlugins::AddWeaponPedForPC(this), 0 < this->m_nWeaponGunflashAlphaMP1 ||
            (0 < this->m_nWeaponGunflashAlphaMP2)))) {
                this->ResetGunFlashAlpha();
            }
            if (this->m_pGogglesObject != nullptr) {
                pRVar12 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
                iVar13 = RpHAnimIDGetIndex(pRVar12, 5);
                iVar14 = RpHAnimHierarchyGetMatrixArray(pRVar12);
                // TODO(port): decomp indexed the bone-matrix array with byte
                //   arithmetic (iVar13 * 0x40 + iVar14); RwMatrix is 0x40 bytes,
                //   so pointer arithmetic is equivalent.
                RwMatrix* pBoneMatrix = iVar14 + iVar13;
                pvVar5 = (RwFrame *)(this->m_pGogglesObject->object).parent;
                // TODO(port): decomp copied the 0x40-byte bone matrix dword-by-dword
                //   into the goggles frame's modelling matrix (frame+0x10).
                pvVar5->modelling = *pBoneMatrix;
                // TODO(port): decomp set the vector components as raw dwords
                //   (x=0, y=0x3dac0831, z=0).
                uStack_c = CVector(0.0f, std::bit_cast<float>(0x3dac0831u), 0.0f);
                RwV3dTransformPoints(&uStack_c, &uStack_c, 1, pBoneMatrix);
                // TODO(port): decomp wrote the transformed vector dword-by-dword
                //   to the frame's modelling.pos (frame+0x40).
                pvVar5->modelling.pos = uStack_c;
                RwFrameUpdateObjects(pvVar5);
                RpClumpRender(this->m_pGogglesObject);
            }
            pCVar15 = this->m_pIntelligence->GetTaskJetPack();
            if (pCVar15 != nullptr) {
                ped = this;
                pCVar15 = this->m_pIntelligence->GetTaskJetPack();
                pCVar15->RenderJetPack(ped); // TODO(port): decomp static-style call
            }
            uVar1 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
            uVar1 = uVar1 | 0x20000000;
            bDonePositionOutOfCollision = (char)uVar1;
            bKilledByStealth = (char)(uVar1 >> 8);
            bRightArmBlocked = (char)(uVar1 >> 0x10);
            bWaitingForScriptBrainToLoad = (char)(uVar1 >> 0x18);
            if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
                (*(RwEngineInstance->dOpenDevice).fpRenderStateSet)(0x1e,local_10);
            }
        }
        return;
}










bool CPed::SetupLighting() {    // converted from decomp src/CPed/*.c
        bool bVar1;

        ActivateDirectional();
        CRenderer::SetupLightingForEntity((CPhysical *)this);
        return bVar1;
}










void CPed::RemoveLighting(bool bRemove) {    // converted from decomp src/CPed/*.c
        if ((m_nPhysicalFlags & 0x20000000) == 0) {
            CPointLights::RemoveLightsAffectingObject();
        }
        SetAmbientColours();
        DeActivateDirectional();
        return;
}










void CPed::FlagToDestroyWhenNextProcessed() {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
        CVehicle **entity;
        uint32_t uVar1;
        CVehicle *pCVar2;
        CPed *this_00;
        uint32_t uVar3;
        int iVar4;
        bool bVar5;
        uint32_t uVar6;

        uVar1 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        m_nFlags = m_nFlags | 0x800;
        if ((uVar1 & 0x100) != 0) {
            pCVar2 = this->m_pVehicle;
            entity = &this->m_pVehicle;
            if (pCVar2 != nullptr) {
                this_00 = pCVar2->m_pDriver;
                if (this_00 == this) {
                    if (this_00 != nullptr) {
                        ((CEntity *)this_00)->CleanUpOldReference((CEntity **)&pCVar2->m_pDriver);
                    }
                    (*entity)->m_pDriver = nullptr;
                    if (((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) &&
                    // (decomp operated on the raw m_info byte here: (m_info & 0xf8)
                    // checks the status bits (m_nStatus is bits 3-7), and
                    // m_info = (m_info & 7) | 0x20 sets status bit 2.)
                    (pCVar2 = *entity, pCVar2->GetStatus() != (eEntityStatus)0x05)) {
                        pCVar2->SetStatus((eEntityStatus)((uint32_t)pCVar2->GetStatus() | 0x04));
                    }
                }
                else {
                    pCVar2->RemovePassenger(this); // TODO(port): decomp passed the vehicle as explicit this
                }
                uVar6 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
                pCVar2 = *entity;
                uVar6 = uVar6 & 0xfffffeff;
                bIsStanding = (char)uVar6;
                bInVehicle = (char)(uVar6 >> 8);
                bFiringWeapon = (char)(uVar6 >> 0x10);
                bNotAllowedToDuck = (char)(uVar6 >> 0x18);
                bVar5 = IsVehiclePointerValid(pCVar2);
                if ((bVar5) && (*entity != nullptr)) {
                    ((CEntity *)*entity)->CleanUpOldReference((CEntity **)entity);
                }
                *entity = nullptr;
                if (this->m_nCreatedBy == PED_MISSION) {
                    if (this->m_pCoverPoint != nullptr) {
                        this->m_pCoverPoint->ReleaseCoverPointForPed(this); // TODO(port): decomp static-style call
                        this->m_pCoverPoint = nullptr;
                    }
                    uVar3 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
                    if ((uVar3 & 0x2000) != 0) {
                        // TODO(port): decomp computed the pool ref with raw pointer arithmetic
                        //   on CPools::ms_pPedPool; refactored to the pool accessor (gta-reversed Ped.cpp).
                        CRadar::ClearBlipForEntity(BLIP_CHAR, 0u /* TODO(port): GetPedPool()->GetRef(this) disabled for C1202 */);
                    }
                    this->m_nPedState = PEDSTATE_DEAD;
                    return;
                }
                this->m_nPedState = PEDSTATE_NONE;
            }
        }
        return;
}










int32_t CPed::ProcessEntityCollision(CEntity* entity,  CColPoint* colPoint) {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        CEntity **entity_00;
        uint32_t uVar1;
        uint32_t uVar2;
        uint32_t uVar3;
        uint32_t uVar4;
        float fVar5;
        float fVar6;
        uint8_t uVar7;
        eSurfaceType eVar8;
        uint8_t uVar17;
        CColSphere *pCVar9;
        CMatrixLink *pCVar10;
        CMatrixLink *pCVar11;
        uint32_t uVar12;
        uint32_t uVar13;
        uint32_t uVar14;
        uint32_t uVar15;
        uint32_t uVar16;
        bool bVar18;
        bool bVar19;
        bool bVar20;
        CTaskSimpleUseGun *pCVar21;
        CColModel *pCVar22;
        int iVar23;
        CSimpleTransform *pCVar24;
        uint32_t uVar25;
        uint32_t uVar26;
        CAnimBlendAssociation *pCVar27;
        uint32_t uVar28;
        uint32_t uVar29;
        uint32_t uVar30;
        uint32_t uVar31;
        uint8_t bVar32;
        uint32_t uVar33;
        CColPoint *pCVar34;
        int iVar35;
        int iVar36;
        CVector *pCVar37;
        long double /* Ghidra float10 */ fVar38;
        CColPoint *pCVar39;
        float *pfVar40;
        float *pfVar41;
        float local_f8; // TODO(port): decomp had this as CColPoint*; it holds a float
        CColPoint* pLocal_f8; // TODO(port): the decomp reuses local_f8's stack slot as a
                              //   CColPoint* later; split into a second variable to compile.
        float local_f4;
    uint32_t local_f4_int; // TODO(port): decomp uses this as int bits of local_f4
        CVector2D local_ec;
        CPedDamageResponseCalculator CStack_e4;
        CColModel *local_d0;
        uint8_t bStack_c9;
        float local_c8;
        float local_c4;
        float local_c0;
        float local_bc;
        CCollisionData *local_b8;
        float local_b4;
        float local_b0;
        float fStack_ac;
        CColPoint local_a8;
        float fStack_74;
        CEventDamage CStack_50;
        void *local_c;
        int iStack_4;

        iStack_4 = 0xffffffff;
        fVar5 = CTimer::ms_fTimeStep * -0.15;
        local_b4 = 1.0;
        local_b0 = 1.0;
        local_d0 = CModelInfo::ms_modelInfoPtrs[(short)this->m_nModelIndex]->m_pColModel;
        uVar17 = bKilledByStealth;
        local_b8 = local_d0->m_pColData;
        local_c8 = 0.0;
        local_c4 = 0.0;
        local_c0 = 1.0;
        bVar18 = false;
        bVar20 = false;
        local_f8 = std::bit_cast<float>(0x3f70a3d7u); // TODO(port): decomp float constant
        local_bc = -1001.0;
        if ((char)uVar17 < '\0') {
            if ((GetUsesCollision() & 1) != 0) {
                if ((((this->m_nPedType == PED_TYPE_PLAYER1) ||
                (pCVar21 = this->m_pIntelligence->GetTaskUseGun(), // TODO(port): parens for comma-expr under ||
                pCVar21 != nullptr &&
                (pCVar21 = this->m_pIntelligence->GetTaskUseGun(),
                pCVar21->m_WeaponInfo != nullptr)) &&
                (pCVar21 = this->m_pIntelligence->GetTaskUseGun(),
                (pCVar21->m_WeaponInfo->m_nFlags >> 1 & 1) != 0)))) {
                    local_bc = GetHeading(); // TODO(port): decomp static-style call
                    // TODO(port): decomp fpatan(a,b) is the two-argument arctangent (atan2).
                    fVar38 = atan2f(-(&DAT_00b6f32c)[(uint32_t)DAT_00b6f081 * 0x8e],
                    (&DAT_00b6f330)[(uint32_t)DAT_00b6f081 * 0x8e]);
                    CPlaceable::SetHeading((float)fVar38);
                }
                local_d0 = &CTempColModels::ms_colModelPed2;
                local_b8 = CTempColModels::ms_colModelPed2.m_pColData;
                goto LAB_005e267e;
            }
        LAB_005e2684:
            if ((*(uint8_t *)((int)&this->m_nPhysicalFlags + 2) & 1) == 0) {
                return 0;
            }
        }
        else {
        LAB_005e267e:
            if ((GetUsesCollision() & 1) == 0) goto LAB_005e2684;
        }
        bVar32 = (uint32_t)entity->GetType() & 7;
        if ((bVar32 == 2) && (entity[0x19].GetRwObject() == (RwObject *)0x5)) { // TODO(port): m_pRwObject is privatevate; accessor used) {
            bVar18 = true;
        }
        if ((((m_nPhysicalFlags & 0x19000) == 0) &&
        (this->m_pAttachedTo == nullptr)) && (bVar32 != 3)) {
            if ((m_nFlags >> 1 & 1) == 0) {
                m_nFlags = m_nFlags | 2;
            }
            ((local_b8->m_pLines)->m_vecStart).z = 0.0;
            ((local_b8->m_pLines)->m_vecEnd).z = -1.0;
            uVar33 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            bVar20 = true;
            if ((uVar33 >> 1 & 1) != 0) {
                ((local_b8->m_pLines)->m_vecEnd).z =
                fVar5 + ((local_b8->m_pLines)->m_vecEnd).z;
            }
            pCVar9 = local_b8->m_pSpheres;
            local_f8 = pCVar9[2].m_fRadius + pCVar9[2].m_vecCenter.z;
            if ((this->GetFourthPedFlags() & 0x100) != 0) {
                fVar5 = (pCVar9->m_vecCenter).z;
                fVar6 = pCVar9->m_fRadius;
                local_b8->m_pLines[1].m_vecStart.z = local_f8;
                fVar5 = (fVar5 - fVar6) + 1.0;
                local_b8->m_pLines[1].m_vecStart.z =
                local_b8->m_pLines[1].m_vecStart.z - fVar5;
                local_b8->m_pLines[1].m_vecEnd.z = local_f8;
                local_b8->m_pLines[1].m_vecEnd.z =
                fVar5 + local_b8->m_pLines[1].m_vecEnd.z;
            }
            if ((this->GetFourthPedFlags() & 0x100) == 0) {
                local_b8->m_nNumLines = '\x01';
                fVar5 = ((local_b8->m_pLines)->m_vecEnd).z;
                (local_d0->m_boundBox).m_vecMax.z = 0.95;
                (local_d0->m_boundSphere).m_fRadius = std::abs(fVar5);
                (local_d0->m_boundBox).m_vecMin.z = ((local_b8->m_pLines)->m_vecEnd).z;
            }
            else {
                local_b8->m_nNumLines = '\x02';
                (local_d0->m_boundSphere).m_fRadius = local_b8->m_pLines[1].m_vecEnd.z;
                (local_d0->m_boundBox).m_vecMax.z = local_b8->m_pLines[1].m_vecEnd.z;
                (local_d0->m_boundBox).m_vecMin.z = ((local_b8->m_pLines)->m_vecEnd).z;
            }
        }
        else {
            local_b8->m_nNumLines = '\0';
            (local_d0->m_boundSphere).m_fRadius = 1.0;
            (local_d0->m_boundBox).m_vecMax.z = 0.95;
            (local_d0->m_boundBox).m_vecMin.z = -1.0;
        }
        uVar33 = (uint32_t)local_ec.x >> 8;
        local_ec.x = (float)((uint32_t)local_ec.x & 0xffffff00);
        if (((m_nFlags >> 4 & 1) != 0) &&
        ((((uint32_t)entity->GetType() & 7) == 1 ||
        (((uint32_t)entity[1].m_placement.m_vPosn.y & 4) != 0)))) {
            local_ec.x = (float)(((uVar33 & 0xFFFFFFu) << 8) | 1u); // TODO(port): was CONCAT31((int3)uVar33,1)
        }
        if (entity->m_matrix == nullptr) {
            entity->AllocateMatrix();
            entity->m_placement.UpdateMatrix(entity->m_matrix);
        }
        pCVar10 = entity->m_matrix;
        pCVar11 = this->m_matrix;
        pfVar40 = &local_b4;
        pCVar34 = &local_a8;
        pCVar39 = colPoint;
        pfVar41 = reinterpret_cast<float*>(static_cast<uint32_t>(local_ec.x)); // TODO(port): decomp treats the float value as an address
        pCVar22 = entity->GetColModel(); // TODO(port): decomp static-style call
        // TODO(port): decomp passed raw pointers; adapted to the
        //   (const CMatrix&, CColModel&, ...) signature. SUB41(pfVar41,0)
        //   reads 4 bytes at pfVar41 for the bool arg.
        iVar23 = CCollision::ProcessColModels
        (*pCVar11, *local_d0, *pCVar10, *pCVar22,
        *reinterpret_cast<std::array<CColPoint, 32>*>(pCVar39),
        pCVar34, pfVar40, *reinterpret_cast<uint32_t*>(pfVar41) != 0);
        fStack_ac = (this->m_matrix->m_pos).z;
        if (!bVar20) goto LAB_005e31c6;
        local_b8->m_nNumLines = '\0';
        (local_d0->m_boundSphere).m_fRadius = 1.0;
        (local_d0->m_boundBox).m_vecMin.z = -1.0;
        (local_d0->m_boundBox).m_vecMax.z = 0.95;
        if (1.0 <= local_b4) {
            if ((((this->GetFourthPedFlags() & 0x100) != 0) &&
            (local_b0 < 1.0)) &&
            ((fStack_74 < this->field_588 &&
            (((bVar20 = entity->GetIsTypePhysical(), // TODO(port): was static-style call !bVar20 ||
            (((uint32_t)entity[1].m_placement.m_vPosn.y & 8) != 0)) &&
            (bVar32 = bIsStanding, this->field_588 = fStack_74,
            (bVar32 & 1) != 0)))))) {
                pCVar10 = this->m_matrix;
                pCVar24 = (CSimpleTransform *)&pCVar10->m_pos;
                if (pCVar10 == nullptr) {
                    pCVar24 = &this->m_placement;
                }
                if (fStack_74 < local_f8 + (pCVar24->m_vPosn).z) {
                    (pCVar10->m_pos).z = fStack_74 - local_f8;
                }
            }
            uVar3 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar3 = uVar3 & 0xfffffffd;
            bResetWalkAnims = (char)uVar3;
            bCollidedWithMyVehicle = (char)(uVar3 >> 8);
            bMiamiViceCop = (char)(uVar3 >> 0x10);
            bDontFight = (char)(uVar3 >> 0x18);
            goto LAB_005e31c6;
        }
        bStack_c9 = bIsStanding & 1;
        if (((bStack_c9 == 0) || ((this->m_matrix->m_pos).z < local_a8.m_vecPoint.z + 1.0)) ||
        ((bVar18 && ((this->m_matrix->m_pos).z < local_a8.m_vecPoint.z + 3.0)))) {
            iVar36 = 0;
            bVar20 = false;
            if (3 < iVar23) {
                iVar35 = 3;
                pfVar40 = &colPoint[1].m_vecNormal.z;
                do {
                    if (pfVar40[-0xb] < -0.867) {
                        bVar20 = true;
                    }
                    if (*pfVar40 < -0.867) {
                        bVar20 = true;
                    }
                    if (pfVar40[0xb] < -0.867) {
                        bVar20 = true;
                    }
                    if (pfVar40[0x16] < -0.867) {
                        bVar20 = true;
                    }
                    iVar35 = iVar35 + 4;
                    iVar36 = iVar36 + 4;
                    pfVar40 = pfVar40 + 0x2c;
                } while (iVar35 < iVar23);
            }
            if (iVar36 < iVar23) {
                pfVar40 = &colPoint[iVar36].m_vecNormal.z;
                iVar36 = iVar23 - iVar36;
                do {
                    if (*pfVar40 < -0.867) {
                        bVar20 = true;
                    }
                    pfVar40 = pfVar40 + 0xb;
                    iVar36 = iVar36 + -1;
                } while (iVar36 != 0);
            }
            if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
                local_ec.x = (float)(uint32_t)((uint8_t)local_a8.m_nLightingB.value >> 4);
                this->m_fContactSurfaceBrightness =
                (1.0 - CTimer::ms_fTimeStep * _DAT_008d21e4) * this->m_fContactSurfaceBrightness +
                (CCustomBuildingDNPipeline::m_fDNBalanceParam * (float)(int)local_ec.x * 0.033333335 +
                (1.0 - CCustomBuildingDNPipeline::m_fDNBalanceParam) *
                (float)((uint8_t)local_a8.m_nLightingB.value & 0xf) * 0.033333335) *
                CTimer::ms_fTimeStep * _DAT_008d21e4;
            }
            else {
                local_ec.x = (float)(uint32_t)((uint8_t)local_a8.m_nLightingB.value >> 4);
                this->m_fContactSurfaceBrightness =
                CCustomBuildingDNPipeline::m_fDNBalanceParam * (float)(int)local_ec.x * 0.033333335 +
                (1.0 - CCustomBuildingDNPipeline::m_fDNBalanceParam) *
                (float)((uint8_t)local_a8.m_nLightingB.value & 0xf) * 0.033333335;
            }
            if ((bStack_c9 == 0) &&
            ((bVar32 = (uint32_t)entity->GetType() & 7, bVar32 == 2 || (bVar32 == 4)))) {
                entity_00 = &this->m_standingOnEntity;
                *entity_00 = entity;
                entity->RegisterReference(entity_00); // TODO(port): was static-style call
                if (entity->m_matrix == nullptr) {
                    pCVar24 = &entity->m_placement;
                }
                else {
                    pCVar24 = (CSimpleTransform *)&entity->m_matrix->m_pos;
                }
                CStack_e4.m_bodyPart = (ePedPieceTypes)(local_a8.m_vecPoint.z - (pCVar24->m_vPosn).z);
                CStack_e4.m_fDamageFactor = local_a8.m_vecPoint.y - (pCVar24->m_vPosn).y;
                CStack_e4.m_pDamager = reinterpret_cast<CEntity*>(std::bit_cast<uint32_t>(local_a8.m_vecPoint.x - (pCVar24->m_vPosn).x)); // TODO(port): decomp stores float bits in pointer
                this->m_pContactEntity = entity;
                (this->field_56C).x = std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)); // TODO(port): decomp reads float bits from pointer
                (this->field_56C).y = CStack_e4.m_fDamageFactor;
                (this->field_56C).z = (float)CStack_e4.m_bodyPart;
                entity->RegisterReference(&this->m_pContactEntity); // TODO(port): was static-style call
                if ((((uint32_t)entity->GetType() & 7) == 2) &&
                (entity[0x19].GetRwObject() == (RwObject *)0x5)) { // TODO(port): m_pRwObject is private; accessor usedr used) {
                    uVar25 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                    uVar25 = uVar25 | 2;
                }
                else {
                    uVar12 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                    uVar25 = uVar12 & 0xfffffffd;
                }
                bResetWalkAnims = (char)uVar25;
                bCollidedWithMyVehicle = (char)(uVar25 >> 8);
                bMiamiViceCop = (char)(uVar25 >> 0x10);
                bDontFight = (char)(uVar25 >> 0x18);
                if (((uint32_t)entity->GetType() & 7) == 2) {
                    this->m_fContactSurfaceBrightness = *reinterpret_cast<float*>(&(*entity_00)[5].m_matrix); // TODO(port): decomp reads m_matrix field as float
                }
            }
            else {
                this->m_pContactEntity = entity;
                entity->RegisterReference(&this->m_pContactEntity); // TODO(port): was static-style call
                uVar1 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                uVar1 = uVar1 & 0xffffff7d;
                bResetWalkAnims = (char)uVar1;
                bCollidedWithMyVehicle = (char)(uVar1 >> 8);
                bMiamiViceCop = (char)(uVar1 >> 0x10);
                bDontFight = (char)(uVar1 >> 0x18);
            }
            if (((((this->GetFourthPedFlags() & 0x100) != 0) &&
            (local_b0 < 1.0)) && (fStack_74 < this->field_588)) &&
            (((bVar32 = (uint32_t)entity->GetType() & 7, bVar32 < 2 || (4 < bVar32)) ||
            (((uint32_t)entity[1].m_placement.m_vPosn.y & 8) != 0)))) {
                this->field_588 = fStack_74;
            }
            uVar13 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar33 = uVar13 >> 0x15 & 1;
            if ((((uVar33 != 0) && ((this->m_matrix->m_pos).z <= local_a8.m_vecPoint.z + 1.0)) &&
            ((((uint32_t)entity->GetType() & 7) != 1 ||
            (local_a8.m_vecNormal.x * (this->m_vecMoveSpeed).x +
            local_a8.m_vecNormal.z * (this->m_vecMoveSpeed).z +
            local_a8.m_vecNormal.y * (this->m_vecMoveSpeed).y <= 0.0)))) || (bVar20)) {
                if ((uVar33 != 0) && (((uint32_t)entity->GetType() & 7) != 2)) {
                    local_c8 = local_a8.m_vecNormal.x;
                    local_c4 = local_a8.m_vecNormal.y;
                    local_c0 = local_a8.m_vecNormal.z;
                }
            }
            else if (99999.99 <= this->field_588) {
                (this->m_matrix->m_pos).z = local_a8.m_vecPoint.z + 1.0;
                uVar26 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                if ((uVar26 & 0x200000) != 0) {
                    uVar26 = uVar26 & 0xffdfffff;
                    bResetWalkAnims = (char)uVar26;
                    bCollidedWithMyVehicle = (char)(uVar26 >> 8);
                    bMiamiViceCop = (char)(uVar26 >> 0x10);
                    bDontFight = (char)(uVar26 >> 0x18);
                }
            }
            else {
                fVar5 = local_a8.m_vecPoint.z + 1.0;
                if (this->field_588 < local_a8.m_vecPoint.z + local_f8 + 1.0) {
                    fVar6 = this->field_588 - local_f8;
                    local_c0 = local_a8.m_vecNormal.z;
                    local_c8 = local_a8.m_vecNormal.x;
                    local_c4 = local_a8.m_vecNormal.y;
                    if (fVar6 < fVar5) {
                        fVar5 = fVar6;
                    }
                }
                (this->m_matrix->m_pos).z = fVar5;
            }
            (this->field_578).x = local_a8.m_vecNormal.x;
            (this->field_578).y = local_a8.m_vecNormal.y;
            (this->field_578).z = local_a8.m_vecNormal.z;
            this->m_nContactSurface = static_cast<eSurfaceType>(local_a8.m_nSurfaceTypeB); // TODO(port): enum conversion
            bVar20 = g_surfaceInfos.IsSteepSlope((uint32_t)(uint8_t)local_a8.m_nSurfaceTypeB); // TODO(port): was static-style call
            if (bVar20) {
                uVar2 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                uVar2 = uVar2 | 0x20;
                bResetWalkAnims = (char)uVar2;
                bCollidedWithMyVehicle = (char)(uVar2 >> 8);
                bMiamiViceCop = (char)(uVar2 >> 0x10);
                bDontFight = (char)(uVar2 >> 0x18);
            }
        }
        local_ec.x = 0.33;
        local_f4 = -0.25;
        if (this->m_nPedState == PEDSTATE_IDLE) {
            local_ec.x = 0.66;
            local_f4 = -0.375;
        }
        pCVar27 = (CAnimBlendAssociation*)RpAnimBlendClumpGetAssociation((RpClump *)this->GetRwObject(),0x78); // TODO(port): GetRwObject is member
        CStack_e4.m_pDamager = reinterpret_cast<CEntity*>(std::bit_cast<uint32_t>((this->m_vecMoveSpeed).x)); // TODO(port): decomp stores float bits in pointer
        CStack_e4.m_fDamageFactor = (this->m_vecMoveSpeed).y;
        CStack_e4.m_bodyPart = (ePedPieceTypes)(this->m_vecMoveSpeed).z;
        bVar32 = (uint32_t)entity->GetType() & 7;
        if ((1 < bVar32) && (bVar32 < 5)) {
            CStack_e4.m_pDamager =
            reinterpret_cast<CEntity*>(std::bit_cast<uint32_t>(std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) - entity[1].m_placement.m_vPosn.z)); // TODO(port): decomp float-bits pun
            CStack_e4.m_fDamageFactor = CStack_e4.m_fDamageFactor - entity[1].m_placement.m_fHeading;
            CStack_e4.m_bodyPart = (ePedPieceTypes)((float)CStack_e4.m_bodyPart - std::bit_cast<float>(reinterpret_cast<uint32_t>(entity[1].m_matrix))); // TODO(port): decomp reads pointer as float
        }
        uVar14 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if (((uVar14 >> 1 & 1) != 0) ||
        ((((fVar5 = std::sqrt(std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) * std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) +
        CStack_e4.m_fDamageFactor * CStack_e4.m_fDamageFactor), fVar5 <= local_ec.x ||
        ((bGetUpAnimStarted) != 0)) &&
        (local_f4 <= (float)CStack_e4.m_bodyPart)) ||
        ((this->m_pEntityIgnoredCollision == entity || (this->m_pVehicle == (CVehicle *)entity)))))) {
            if (((uVar14 >> 1 & 1) == 0) &&
            (((pCVar27 != nullptr &&
            ((float)CStack_e4.m_bodyPart < CTimer::ms_fTimeStep * -0.016)) &&
            (this->m_pVehicle != (CVehicle *)entity)))) {
                // TODO(port): decomp emitted the constructor as an explicit call; placement new.
                new (&CStack_50) CEventDamage(entity, CTimer::m_snTimeInMilliseconds, WEAPON_FALL, PED_PIECE_TORSO, '\x02', false, false);
                iStack_4 = 2;
                bVar20 = CStack_50.AffectsPed(this); // TODO(port): decomp static-style call
                if (bVar20) {
                    // TODO(port): decomp emitted the constructor as an explicit call; placement new.
                    new (&CStack_e4) CPedDamageResponseCalculator(entity, 15.0f, WEAPON_FALL, PED_PIECE_TORSO, false);
                    iStack_4 = (iStack_4 & ~0xFFu) | ((3 & 0xFF) << 0);
                    CStack_e4.ComputeDamageResponse(this, CStack_50.m_damageResponse, true); // TODO(port): decomp static-style call
                    this->m_pIntelligence->m_eventGroup.Add(&CStack_50, false); // TODO(port): decomp static-style call
                    iStack_4 = (iStack_4 & ~0xFF) | 2; // TODO(port): was CONCAT31(iStack_4._1_3_,2)
                    goto LAB_005e3079;
                }
                goto LAB_005e3082;
            }
        }
        else {
            local_ec.x = -0.25;
            bVar20 = g_surfaceInfos.IsSoftLanding((uint32_t)(uint8_t)local_a8.m_nSurfaceTypeB);
            if (bVar20) {
                local_ec.x = local_ec.x * 1.5;
                fVar6 = 0.375;
            }
            else {
                fVar6 = 0.25;
            }
            fVar5 = fVar5 - fVar6;
            if (fVar5 < 0.0) {
                fVar5 = 0.0;
            }
            fVar6 = local_ec.x - (float)CStack_e4.m_bodyPart;
            if (fVar6 < 0.0) {
                fVar6 = 0.0;
            }
            local_f8 = (float)(fVar6 * 400.0 + fVar5 * 100.0); // TODO(port): decomp Ghidra cast noise
            if ((float)CStack_e4.m_bodyPart < -0.6) {
                local_f8 = std::bit_cast<float>(0x43fa0000u); // TODO(port): decomp float constant (500.0f)
            }
            local_f4 = std::bit_cast<float>((std::bit_cast<uint32_t>(local_f4) & ~0xFFu) | 0x02u); // TODO(port): decomp bit-ops on float
            if ((((0.01 < std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager))) || (std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) < -0.01)) ||
            (0.01 < CStack_e4.m_fDamageFactor)) || (CStack_e4.m_fDamageFactor < -0.01)) {
                local_ec.y = CStack_e4.m_fDamageFactor * -1.0;
                local_ec.x = std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) * -1.0;
                iVar36 = this->GetLocalDirection(local_ec); // TODO(port): was static-style call
                local_f4_int = (uint32_t)(uint8_t)iVar36;
            }
            // TODO(port): decomp emitted the constructor as an explicit call; placement new.
            new (&CStack_50) CEventDamage(entity, CTimer::m_snTimeInMilliseconds, WEAPON_FALL, PED_PIECE_TORSO, (uint8_t)local_f4_int, false, false);
            iStack_4 = 0;
            bVar20 = CStack_50.AffectsPed(this); // TODO(port): decomp static-style call
            if (bVar20) {
                // TODO(port): decomp emitted the constructor as an explicit call; placement new.
                new (&CStack_e4) CPedDamageResponseCalculator(entity, local_f8, WEAPON_FALL, PED_PIECE_TORSO, false);
                iStack_4 = (iStack_4 & ~0xFFu) | ((1 & 0xFF) << 0);
                CStack_e4.ComputeDamageResponse(this, CStack_50.m_damageResponse, true); // TODO(port): decomp static-style call
                this->m_pIntelligence->m_eventGroup.Add(&CStack_50, false); // TODO(port): decomp static-style call
                iStack_4 = iStack_4 & ~0xFF; // TODO(port): was (uint32_t)iStack_4._1_3_ << 8
            LAB_005e3079:
                ; // TODO(port): CPedDamageResponseCalculator_Destructor(); - decomp destructor thunk
            }
        LAB_005e3082:
            iStack_4 = 0xffffffff;
            // TODO(port): CEventDamage_Destructor(); - decomp destructor thunk
        }
        uVar28 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar28 = uVar28 | 1;
        bIsStanding = (char)uVar28;
        bInVehicle = (char)(uVar28 >> 8);
        bFiringWeapon = (char)(uVar28 >> 0x10);
        bNotAllowedToDuck = (char)(uVar28 >> 0x18);
        pCVar10 = this->m_matrix;
        (this->m_vecMoveSpeed).z = 0.0;
        if (((fStack_ac + 0.1 < (pCVar10->m_pos).z) && (local_b8->m_nNumLines == '\0')) &&
        ((this->m_nPedType == PED_TYPE_PLAYER1 || (this->m_nPedType == PED_TYPE_PLAYER2)))) {
            if (entity->m_matrix == nullptr) {
                ((CPlaceable*)entity)->AllocateMatrix(); // TODO(port): was static-style call
                entity->m_placement.UpdateMatrix(entity->m_matrix); // TODO(port): was static-style call
            }
            pCVar10 = entity->m_matrix;
            pCVar11 = this->m_matrix;
            bVar20 = false;
            pfVar40 = nullptr;
            pCVar39 = nullptr;
            pCVar34 = colPoint;
            pCVar22 = entity->GetColModel(); // TODO(port): decomp static-style call
            iVar23 = CCollision::ProcessColModels(*(CMatrix*)pCVar11, *local_d0, *(CMatrix*)pCVar10, *pCVar22, *(std::array<CColPoint,32>*)pCVar34, pCVar39, pfVar40, bVar20); // TODO(port): was pointer args
        }
    LAB_005e31c6:
        local_d0 = nullptr;
        if (0 < iVar23) {
            local_f4 = (float)(iVar23 + -1);
            local_ec.x = std::bit_cast<float>(reinterpret_cast<uint32_t>(&colPoint->m_vecNormal)); // TODO(port): decomp stores address as float
            pLocal_f8 = colPoint;
            do {
                uVar29 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
                if ((((char)((uVar29 >> 8) & 0xFF) < '\0') && ((GetUsesCollision() & 1) != 0)
                ) && (2 < pLocal_f8->m_nPieceTypeA)) {
                    bVar18 = true;
                    bVar19 = true;
                    bVar20 = true;
                    if (((uint32_t)entity->GetType() & 7) == 3) {
                        bVar19 = false;
                        bVar20 = true;
                        if (((uint32_t)entity[0x14].m_matrix & 0x100000) == 0) {
                            if (((uint32_t)entity[0x14].m_placement.m_vPosn.z & 0x4000000) != 0) {
                                bVar20 = false;
                            }
                        }
                        else {
                            bVar18 = false;
                        }
                    }
                    uVar7 = pLocal_f8->m_nPieceTypeA;
                    if (((uVar7 == '\x06') && (bVar18)) && (bVar20)) {
                        uVar29 = uVar29 | 0x10000;
                    LAB_005e32a1:
                        bDonePositionOutOfCollision = (char)uVar29;
                        bKilledByStealth = (char)(uVar29 >> 8);
                        bRightArmBlocked = (char)(uVar29 >> 0x10);
                        bWaitingForScriptBrainToLoad = (char)(uVar29 >> 0x18);
                    }
                    else {
                        if (((uVar7 == '\x05') && (bVar18)) && (bVar20)) {
                            uVar29 = uVar29 | 0x20000;
                            goto LAB_005e32a1;
                        }
                        if ((uVar7 == '\b') && (bVar18)) {
                            uVar29 = uVar29 | 0x40000;
                            goto LAB_005e32a1;
                        }
                        if ((uVar7 == '\x04') && (bVar19)) {
                            uVar29 = uVar29 | 0x80000;
                            goto LAB_005e32a1;
                        }
                    }
                    if ((int)local_d0 < (int)local_f4) {
                        iVar36 = (int)local_f4 - (int)local_d0;
                        pCVar34 = pLocal_f8;
                        do {
                            pCVar34 = pCVar34 + 1;
                            Hoodlum::unk_015637e0(pCVar34);
                            iVar36 = iVar36 + -1;
                        } while (iVar36 != 0);
                        local_d0 = (CColModel *)((int)local_d0 + -1);
                        pLocal_f8 = pLocal_f8 + -1;
                        local_ec.x = std::bit_cast<float>(reinterpret_cast<uint32_t>(&reinterpret_cast<CVector*>(static_cast<int>(local_ec.x) - 0x30)->y)); // TODO(port): decomp address arithmetic
                    }
                    iVar23 = iVar23 + -1;
                    local_f4 = (float)((int)local_f4 + -1);
                }
                else if ((((((uint32_t)entity->GetType() & 7) == 2) && (entity[0x19].GetFlags() == 6)) // TODO(port): m_nFlags is private; accessor used)
                && (((uint32_t)entity[1].m_placement.m_vPosn.y & 4) != 0)) &&
                ((pLocal_f8->m_vecNormal).z < 0.0)) {
                    (pLocal_f8->m_vecNormal).z = 0.0;
                    reinterpret_cast<CVector*>(static_cast<uint32_t>(local_ec.x))->Normalise(); // TODO(port): was static-style call
                }
                local_d0 = (CColModel *)((int)local_d0 + 1);
                pLocal_f8 = pLocal_f8 + 1;
                local_ec.x = std::bit_cast<float>(reinterpret_cast<uint32_t>(&reinterpret_cast<CVector*>(static_cast<int>(local_ec.x) + 0x24)->z)); // TODO(port): decomp address arithmetic
            } while ((int)local_d0 < iVar23);
        }
        if (-1000.0 < local_bc) {
            if (this->m_matrix == nullptr) {
                (this->m_placement).m_fHeading = local_bc;
            }
            else {
                ((CMatrix*)this->m_matrix)->SetRotateZOnly(local_bc); // TODO(port): was static-style call
            }
        }
        if ((((((uint32_t)entity->GetType() & 7) == 1) || ((entity->m_nFlags & 4) != 0)) ||
        ((entity->m_nFlags & 0x40000) != 0)) &&
        // TODO(port): decomp packed bIsStanding/bInVehicle/bFiringWeapon/
        //   bNotAllowedToDuck into uVar16 bytes and checked (uVar16>>1)&1.
        //   With bool 0/1, bit 1 is never set; likely meant bIsStanding.
        (bIsStanding && (0 < iVar23))) {
            pCVar37 = &colPoint->m_vecNormal;
            iVar36 = iVar23;
            do {
                CStack_e4.m_pDamager = reinterpret_cast<CEntity*>(std::bit_cast<uint32_t>(pCVar37->x)); // TODO(port): decomp stores float bits in pointer
                CStack_e4.m_fDamageFactor = pCVar37->y;
                CStack_e4.m_bodyPart = (ePedPieceTypes)pCVar37->z;
                if (((-0.99 <= (float)CStack_e4.m_bodyPart) || (*(uint8_t *)((int)(pCVar37 + 1) + 5) != '\x02'))
                || ((this->m_nPedType != PED_TYPE_PLAYER1 && (this->m_nPedType != PED_TYPE_PLAYER2)))) {
                    fVar5 = std::sqrt(std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) * std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) +
                    CStack_e4.m_fDamageFactor * CStack_e4.m_fDamageFactor);
                    if (fVar5 != 0.0) {
                        fVar5 = 1.0 / fVar5;
                        CStack_e4.m_pDamager = reinterpret_cast<CEntity*>(std::bit_cast<uint32_t>(std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager)) * fVar5)); // TODO(port): decomp float-bits pun
                        CStack_e4.m_fDamageFactor = CStack_e4.m_fDamageFactor * fVar5;
                    }
                }
                else {
                    fVar5 = (this->m_vecMoveSpeed).y;
                    fVar6 = (this->m_vecMoveSpeed).x;
                    fVar5 = std::sqrt(fVar5 * fVar5 + fVar6 * fVar6);
                    if (fVar5 < 0.001) {
                        fVar5 = 0.001;
                    }
                    CStack_e4.m_pDamager = reinterpret_cast<CEntity*>(std::bit_cast<uint32_t>(static_cast<float>((-1.0 / fVar5) * (this->m_vecMoveSpeed).x))); // TODO(port): decomp stores float bits in pointer
                    CStack_e4.m_fDamageFactor = (-1.0 / fVar5) * (this->m_vecMoveSpeed).y;
                    (this->m_matrix->m_pos).z = (this->m_matrix->m_pos).z - 0.05;
                    uVar30 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                    uVar30 = uVar30 | 0x200020;
                    bResetWalkAnims = (char)uVar30;
                    bCollidedWithMyVehicle = (char)(uVar30 >> 8);
                    bMiamiViceCop = (char)(uVar30 >> 0x10);
                    bDontFight = (char)(uVar30 >> 0x18);
                }
                ((CVector*)(&CStack_e4))->Normalise(); // TODO(port): was static-style call
                pCVar37->x = std::bit_cast<float>(reinterpret_cast<uint32_t>(CStack_e4.m_pDamager));
                pCVar37->y = CStack_e4.m_fDamageFactor;
                eVar8 = *(eSurfaceType *)((int)(pCVar37 + 1) + 7);
                pCVar37->z = (float)CStack_e4.m_bodyPart;
                bVar20 = g_surfaceInfos.IsSteepSlope((uint32_t)(uint8_t)eVar8);
                if (bVar20) {
                    uVar4 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
                    uVar4 = uVar4 | 0x20;
                    bResetWalkAnims = (char)uVar4;
                    bCollidedWithMyVehicle = (char)(uVar4 >> 8);
                    bMiamiViceCop = (char)(uVar4 >> 0x10);
                    bDontFight = (char)(uVar4 >> 0x18);
                }
                pCVar37 = (CVector *)((int)(pCVar37 + 3) + 8);
                iVar36 = iVar36 + -1;
            } while (iVar36 != 0);
        }
        if (local_c0 < 1.0) {
            pCVar34 = colPoint + iVar23;
            pCVar37 = &pCVar34->m_vecNormal;
            (pCVar34->m_vecNormal).y = local_c4;
            pCVar37->x = local_c8;
            (pCVar34->m_vecNormal).z = 0.0;
            (pCVar37)->Normalise(); // TODO(port): was static-style call
            CStack_e4.m_bodyPart = (ePedPieceTypes)((pCVar34->m_vecNormal).z * 0.35);
            if (this->m_matrix == nullptr) {
                pCVar24 = &this->m_placement;
            }
            else {
                pCVar24 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            local_c8 = (pCVar24->m_vPosn).x - pCVar37->x * 0.35;
            local_c4 = (pCVar24->m_vPosn).y - (pCVar34->m_vecNormal).y * 0.35;
            local_c0 = (pCVar24->m_vPosn).z - (float)CStack_e4.m_bodyPart;
            (pCVar34->m_vecPoint).x = local_c8;
            (pCVar34->m_vecPoint).y = local_c4;
            (pCVar34->m_vecPoint).z = local_c0;
            uVar31 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            iVar23 = iVar23 + 1;
            uVar31 = uVar31 | 0x20;
            bResetWalkAnims = (char)uVar31;
            bCollidedWithMyVehicle = (char)(uVar31 >> 8);
            bMiamiViceCop = (char)(uVar31 >> 0x10);
            bDontFight = (char)(uVar31 >> 0x18);
        }
        if ((0 < iVar23) || (local_b4 < 1.0)) {
            this->AddCollisionRecord(entity); // TODO(port): decomp static-style call CPhysical::AddCollisionRecord((CPhysical*)this, entity)
            if (((uint32_t)entity->GetType() & 7) != 1) {
                ((CPhysical*)entity)->AddCollisionRecord(this); // TODO(port): decomp static-style call with explicit this ptr
            }
            if ((0 < iVar23) &&
            (((((uint32_t)entity->GetType() & 7) == 1 || ((entity->m_nFlags & 4) != 0)) ||
            ((entity->m_nFlags & 0x40000) != 0)))) {
                m_nFlags = m_nFlags | 0x1000;
            }
        }
        return iVar23;
}










void CPed::SetMoveAnim() {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): uint8_t-slice
        uint16_t uVar1;
        eMoveState eVar2;
        char cVar3;
        uint32_t uVar4;
        CAnimBlendAssociation *pCVar5;
        int iVar6;
        CPed *pCVar7;
        RpClump *clump;
        AssocGroupId groupId;
        AnimationId animId;
        float clumpAssocBlendData;

        if (this->m_nPedState == PEDSTATE_DIE) {
            return;
        }
        if (this->m_nPedState == PEDSTATE_DEAD) {
            return;
        }
        uVar4 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar4 & 0x4000000) != 0) {
            return;
        }
        if (this->m_pAttachedTo != nullptr) {
            return;
        }
        eVar2 = this->m_nMoveState;
        if (this->m_nSwimmingMoveState == eVar2) {
            if ((int)eVar2 < 4) {
                return;
            }
            if (((this->m_pedIK).m_fSlopePitch <= 0.01) && (-0.01 <= (this->m_pedIK).m_fSlopePitch)) {
                return;
            }
            uVar4 = 0;
            if (eVar2 == PEDMOVE_RUN) {
                uVar4 = 1;
            }
            else if (eVar2 == PEDMOVE_SPRINT) {
                uVar4 = 2;
            }
            pCVar5 = (CAnimBlendAssociation*)RpAnimBlendClumpGetAssociation((RpClump *)this->GetRwObject(),uVar4); // TODO(port): GetRwObject is member
            if (pCVar5 == nullptr) {
                return;
            }
            if ((bMoveAnimSpeedHasBeenSetByTask & 1) != 0) {
                return;
            }
            this->SetMoveAnimSpeed(pCVar5); // TODO(port): was static-style call
            return;
        }
        if (eVar2 == PEDMOVE_NONE) {
            this->m_nSwimmingMoveState = 0;
            return;
        }
        groupId = this->m_nAnimGroup;
        this->m_nSwimmingMoveState = eVar2;
        if (((eVar2 == PEDMOVE_WALK) || (eVar2 == PEDMOVE_RUN)) || (eVar2 == PEDMOVE_SPRINT)) {
            for (pCVar5 = RpAnimBlendClumpGetFirstAssociation((RpClump *)GetRwObject(),0x10);
            pCVar5 != nullptr; pCVar5 = RpAnimBlendGetNextAssociation(pCVar5,0x10))
            {
                uVar1 = pCVar5->m_Flags;
                if ((((uint8_t)uVar1 >> 3 & 1) == 0) && ((uVar1 >> 10 & 1) == 0)) {
                    pCVar5->m_BlendDelta = -2.0;
                    pCVar5->m_Flags = uVar1 | 4;
                }
            }
            this->ClearAimFlag(); // TODO(port): was static-style call
            this->ClearLookFlag(); // TODO(port): was static-style call
        }
        switch(this->m_nMoveState) {
            case PEDMOVE_STILL:
            clump = (RpClump *)GetRwObject();
            clumpAssocBlendData = 4.0;
            animId = ANIM_ID_IDLE;
            break;
            case PEDMOVE_TURN_L:
            clumpAssocBlendData = 16.0;
            animId = ANIM_ID_TURN_L;
            groupId = ANIM_GROUP_DEFAULT;
            goto LAB_005e4c0c;
            case PEDMOVE_TURN_R:
            clump = (RpClump *)GetRwObject();
            clumpAssocBlendData = 16.0;
            animId = ANIM_ID_TURN_R;
            groupId = ANIM_GROUP_DEFAULT;
            break;
            case PEDMOVE_WALK:
            clump = (RpClump *)GetRwObject();
            clumpAssocBlendData = 1.0;
            animId = ANIM_ID_WALK;
            break;
        default:
            goto LAB_005e4c2e;
            case PEDMOVE_RUN:
            if (this->m_nPedState == PEDSTATE_FLEE_ENTITY) {
                clumpAssocBlendData = 3.0;
                animId = ANIM_ID_RUN;
                goto LAB_005e4c0c;
            }
            clump = (RpClump *)GetRwObject();
            clumpAssocBlendData = 1.0;
            animId = ANIM_ID_RUN;
            break;
            case PEDMOVE_SPRINT:
            cVar3 = CPedGroups::IsInPlayersGroup(this);
            if (cVar3 != '\0') {
                iVar6 = CPedGroups::GetPedsGroup(this);
                pCVar7 = CPedGroupMembership::GetLeader((CPedGroupMembership *)(iVar6 + 8));
                if (pCVar7 != nullptr) {
                    iVar6 = CPedGroups::GetPedsGroup(this);
                    pCVar7 = CPedGroupMembership::GetLeader((CPedGroupMembership *)(iVar6 + 8));
                    if (5 < (int)pCVar7->m_nMoveState) {
                        clump = (RpClump *)GetRwObject();
                        clumpAssocBlendData = 1.0;
                        animId = ANIM_ID_SPRINT;
                        groupId = ANIM_GROUP_PLAYER;
                        break;
                    }
                }
            }
            clumpAssocBlendData = 1.0;
            animId = ANIM_ID_SPRINT;
        LAB_005e4c0c:
            clump = (RpClump *)GetRwObject();
        }
        pCVar5 = CAnimManager::BlendAnimation(clump,groupId,animId,clumpAssocBlendData);
        if ((pCVar5 != nullptr) &&
        ((bMoveAnimSpeedHasBeenSetByTask & 1) == 0)) {
            this->SetMoveAnimSpeed(pCVar5); // TODO(port): was static-style call
        }
    LAB_005e4c2e:
        return;
}










bool CPed::Save() {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): DAT_ global
    // TODO(convert): vector ctor iterator
        uint32_t local_19c;
        uint8_t local_198 [20];
        uint8_t local_184 [376];
        void *local_c;
        uint32_t local_4;

        local_19c = 0x18c;
        // TODO(port): decomp built 13 scratch CWeapons in local_184 via _eh_vector_constructor_iterator_
        //   (MSVC CRT; ctor = CGameLogic::unk_00441e00, dtor via DAT_004411a0). Nothing in the port reads
        //   that scratch space, so the construction is dropped.
        local_4 = 0;
        CPedSaveStructure pedSave; // TODO(port): real layout from decomp
        pedSave.Construct(this);
        CGenericGameStorage::SaveDataToWorkBuffer(&local_19c,4);
        CGenericGameStorage::SaveDataToWorkBuffer(local_198,0x18c);
        // TODO(port): decomp tore down the scratch CWeapons via _eh_vector_destructor_iterator_; nothing to do
        return true;
}










bool CPed::Load() {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): DAT_ global
    // TODO(convert): vector ctor iterator
        uint8_t local_19c [4];
        uint8_t local_198 [20];
        uint8_t local_184 [376];
        void *local_c;
        uint32_t local_4;

        // TODO(port): decomp built 13 scratch CWeapons in local_184 via _eh_vector_constructor_iterator_
        //   (MSVC CRT); nothing in the port reads that scratch space, so the construction is dropped.
        local_4 = 0;
        CGenericGameStorage::LoadDataFromWorkBuffer(local_19c,4);
        CGenericGameStorage::LoadDataFromWorkBuffer(local_198,0x18c);
        CPedSaveStructure pedSave; // TODO(port): real layout from decomp
        pedSave.Extract(this);
        // TODO(port): decomp tore down the scratch CWeapons via _eh_vector_destructor_iterator_; nothing to do
        return true;
}










void* CPed::operator new(std::size_t size) {    // converted from decomp src/CPed/operator_new_005e4720.c
        void *pvVar1;

        // TODO(port): decomp CPedPool::New(); pool stand-in requires CPed default ctor (absent).
        //   Use operator new from base to avoid C1202/C2512 template issues.
        pvVar1 = ::operator new(sizeof(CPed));
        return pvVar1;
}











void* CPed::operator new(std::size_t size,  int32_t poolRef) {    // converted from decomp src/CPed/operator_new_005e4730.c
        // TODO(port): decomp allocated slot (poolRef>>8) from CPools::ms_pPedPool's
        //   object array; the real CPool<T> lands with the core batch.
        (void)size;
        CEventSoundQuiet::unk_005e0540(poolRef);
        return (CPed*)nullptr /* TODO(port): GetPedPool()->GetAt disabled for C1202 */;
}











void CPed::operator delete(void* data) {    // converted from decomp src/CPed/operator_delete_005e4760.c
        // TODO(port): decomp marked the pool slot free in CPools::ms_pPedPool's
        //   byte map and updated m_nFirstFree; the real CPool<T> lands with the core batch.
        // GetPedPool()->Free((CPed*)data); // TODO(port): disabled for C1202 (was no-op)
        return;
}











void CPed::operator delete(void* data,  int poolRef) {
        // TODO(port): simplified - decomp did manual pool slot arithmetic via ms_pPedPool
        (void)poolRef;
        // GetPedPool()->Free(static_cast<CPed*>(data)); // TODO(port): disabled for C1202 (was no-op)
        return;
}











CPed::CPed(ePedType pedType) : CPhysical() {    // converted from decomp src/CPed/Constructor_005e8030.c
    // TODO(convert): DAT_ global
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): vector ctor iterator
    // TODO(convert): vtable write
        uint32_t uVar1;
        uint32_t uVar2;
        uint8_t uVar3;
        uint32_t uVar4;
        CAcquaintance *pCVar5;
        uint32_t *puVar6;
        CPedIntelligence *pCVar7;
        CTaskComplexFacial *pCVar8;
        CTaskSimpleStandStill *pCVar9;
        CPlayerPed *ped;
        int iVar10;
        uint32_t uVar11;
        CEventAcquaintancePed local_24;
        uint32_t local_4;

        (this->m_pedAudio).m_tempSound.m_PhysicalEntity = nullptr;
        // TODO(port): decomp set vtable via PTR_UpdateParameters_0086c2a8 here; port re-constructs in place
        new (&(this->m_pedAudio)) CAEPedAudioEntity();
        (this->m_pedAudio).m_sTwinLoopSoundEntity.m_tempSound.m_PhysicalEntity = 0;
        // TODO(port): decomp set vtable to CAETwinLoopSoundEntity::vftable here; port re-constructs in place
        new (&(this->m_pedAudio).m_sTwinLoopSoundEntity) CAETwinLoopSoundEntity();
        (this->m_pedAudio).m_sTwinLoopSoundEntity.m_IsInUse = 0;
        (this->m_pedAudio).m_sTwinLoopSoundEntity.m_Sounds[0] = nullptr;
        (this->m_pedAudio).m_sTwinLoopSoundEntity.m_Sounds[1] = nullptr;
        (this->m_pedAudio).m_pPed = nullptr;
        (this->m_pedAudio).m_bCanAddEvent = false;
        (this->m_pedAudio).m_JetPackSound0 = nullptr;
        (this->m_pedAudio).m_JetPackSound1 = nullptr;
        (this->m_pedAudio).m_JetPackSound2 = nullptr;
        local_4 = 1; // was: byte0=1, bytes1-3=0 (decomp ._1_3_)
        (this->m_weaponAudio).m_tempSound.m_PhysicalEntity = 0;
        (this->m_weaponAudio).m_LastFlameThrowerFireTimeMs = 0;
        (this->m_weaponAudio).m_LastSprayCanFireTimeMs = 0;
        (this->m_weaponAudio).m_LastFireExtFireTimeMs = 0;
        (this->m_weaponAudio).m_FlameThrowerIdleGasLoopSound = nullptr;
        (this->m_weaponAudio).m_LastWeaponPlaneFrequencyIndex = '\0';
        (this->m_weaponAudio).m_LastMiniGunFireTimeMs = 0;
        (this->m_weaponAudio).m_IsMiniGunSpinActive = false;
        (this->m_weaponAudio).m_IsMiniGunFireActive = false;
        (this->m_weaponAudio).m_LastChainsawEventTimeMs = 0;
        (this->m_weaponAudio).m_LastGunFireTimeMs = 0;
        // TODO(port): decomp set vtable to CAEWeaponAudioEntity::vftable here; port re-constructs in place
        new (&(this->m_weaponAudio)) CAEPedWeaponAudioEntity();
        (this->m_weaponAudio).m_Ped = nullptr;
        (this->m_weaponAudio).m_bInitialised = false;
        (this->m_vecAnimMovingShiftLocal).x = 0.0;
        (this->m_vecAnimMovingShiftLocal).y = 0.0;
        m_acquaintance.m_nRespect = 0;
        m_acquaintance.m_nLike = 0;
        m_acquaintance.m_nIgnore = 0;
        m_acquaintance.m_nDislike = 0;
        m_acquaintance.m_nHate = 0;
        local_4 = (local_4 & ~0xFFu) | ((3 & 0xFF) << 0);
        // TODO(port): decomp explicit-ctor call CPedIK::Constructor(&m_pedIK, this); the real ctor stores the ped
        this->m_pedIK.m_pPed = this;
        this->m_fHealth = 100.0;
        this->m_fMaxHealth = 100.0;
        this->m_fArmour = 0.0;
        this->m_nPedType = pedType;
        // TODO(port): decomp re-ran the CWeapon ctor over m_aWeapons via _eh_vector_constructor_iterator_; the std::array members are already constructed, nothing to do
        uVar4 = m_nPhysicalFlags;
        local_4 = (local_4 & ~0xFFu) | ((4 & 0xFF) << 0);
        m_info.m_nType = (eEntityType)(m_info.m_nType & 0xfb | 3); // TODO(port): decomp bit-ops on entity type
        m_nPhysicalFlags = uVar4 | 0x10000010;
        this->m_nCreatedBy = PED_GAME;
        this->m_pVehicle = nullptr;
        this->m_nAntiSpazTimer = 0;
        this->m_nUnconsciousTimer = 0;
        this->m_nAttackTimer = 0;
        this->m_nLookTime = 0;
        this->m_nDeathTimeMS = 0;
        (this->m_vecAnimMovingShift).x = 0.0;
        (this->m_vecAnimMovingShift).y = 0.0;
        (this->field_56C).x = 0.0;
        (this->field_56C).y = 0.0;
        (this->field_56C).z = 0.0;
        (this->field_578).x = 0.0;
        (this->field_578).y = 0.0;
        this->m_nPedState = PEDSTATE_IDLE;
        this->m_nMoveState = PEDMOVE_STILL;
        uVar4 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        local_24.m_bValid = false;
        local_24._9_3_ = 0x3f8000;
        uVar4 = uVar4 & 0xfffe2000 | 0x2000;
        this->m_fCurrentRotation = 0.0;
        this->m_fHeadingChangeRate = 15.0;
        this->m_fMoveAnim = 0.1;
        this->m_fAimingRotation = 0.0;
        this->m_standingOnEntity = nullptr;
        // TODO(port): decomp set local_24.vtable = nullptr here; vtable managed by construction in port
        local_24.m_nTimeActive = 0;
        (this->field_578).z = 1.0;
        this->m_nWeaponShootingRate = '(';
        this->field_594 = 0;
        this->m_pEntityIgnoredCollision = nullptr;
        this->m_nSwimmingMoveState = 0;
        this->m_pFire = nullptr;
        this->m_fireDmgMult = 1.0;
        this->m_pTargetedObject = nullptr;
        this->m_pLookTarget = nullptr;
        this->m_fLookDirection = 0.0;
        this->m_pContactEntity = nullptr;
        this->field_588 = 99999.99;
        this->m_fMass = 70.0;
        this->m_fTurnMass = 100.0;
        this->m_fAirResistance = 0.0057142857;
        this->m_fElasticity = 0.05;
        this->m_nBodypartToRemove = -1;
        bIsStanding = (char)uVar4;
        bInVehicle = (char)(uVar4 >> 8);
        bFiringWeapon = (char)(uVar4 >> 0x10);
        bNotAllowedToDuck = (char)(uVar4 >> 0x18);
        uVar4 = CGeneral::GetRandomNumber();
        uVar1 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar2 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        bResetWalkAnims = '\0';
        bCollidedWithMyVehicle = '\0';
        bMiamiViceCop = '\x10';
        bDontFight = '\x06';
        uVar4 = ((uVar4 & 3) == 0 | 0xffffc000) << 0x11 | uVar1 & 0x1ffff;
        bIsStanding = (char)uVar4;
        bInVehicle = (char)(uVar4 >> 8);
        bFiringWeapon = (char)(uVar4 >> 0x10);
        bNotAllowedToDuck = (char)(uVar4 >> 0x18);
        uVar4 = uVar2 & 0x20000000 | 0x4000000;
        bDonePositionOutOfCollision = (char)uVar4;
        bKilledByStealth = (char)(uVar4 >> 8);
        bRightArmBlocked = (char)(uVar4 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar4 >> 0x18);
        this->SetFourthPedFlags(this->GetFourthPedFlags() & 0xffc21020 | 0x21000);
        CAEPedWeaponAudioEntity::Initialise(this);
        CAEPedAudioEntity::Initialise(&this->m_pedAudio,this);
        pCVar5 = CPedType::GetPedTypeAcquaintances(this->m_nPedType);
        m_acquaintance.m_nRespect = pCVar5->m_nRespect;
        m_acquaintance.m_nLike = pCVar5->m_nLike;
        m_acquaintance.m_nIgnore = pCVar5->m_nIgnore;
        m_acquaintance.m_nDislike = pCVar5->m_nDislike;
        m_acquaintance.m_nHate = pCVar5->m_nHate;
        this->m_nSavedWeapon = WEAPON_UNIDENTIFIED;
        this->m_nDelayedWeapon = WEAPON_UNIDENTIFIED;
        this->m_nActiveWeaponSlot = '\0';
        // TODO(port): decomp zeroed the 13 CWeapons via uint32 pointer arithmetic
        //   (puVar6 = (uint32_t*)(m_aWeapons + 4), 13 iterations of 5 dwords, stride 7 dwords = 0x1c).
        //   The port's m_aWeapons are real constructed CWeapons; raw zeroing would clobber vtables,
        //   so it is dropped (construction above already initialized them).
        this->m_nWeaponSkill = eWeaponSkill::STD;
        this->m_nFightingStyle = STYLE_STANDARD;
        this->m_nAllowedAttackMoves = '\0';
        this->GiveWeapon(WEAPON_UNARMED,0,true); // TODO(port): decomp static-style call GiveWeapon( ...)
        this->m_nWeaponAccuracy = '<';
        this->m_nLastWeaponDamage = -1;
        this->m_pLastEntityDamage = nullptr;
        this->field_768 = 0;
        this->m_pAttachedTo = nullptr;
        this->m_nTurretAmmo = 0;
        *(uint8_t *)&this->m_roadRageWith = 0;
        this->field_468 = 0;
        this->m_nWeaponModelId = -1;
        this->m_nMoneyCount = 0;
        this->field_72F = '\0';
        this->m_nTimeTillWeNeedThisPed = 0;
        this->m_VehDeadInFrontOf = nullptr;
        this->m_pWeaponObject = nullptr;
        this->m_pGunflashObject = nullptr;
        this->m_pGogglesObject = nullptr;
        this->m_pGogglesState = nullptr;
        this->m_nWeaponGunflashAlphaMP1 = 0;
        this->m_nWeaponGunFlashAlphaProgMP1 = 0;
        this->m_nWeaponGunflashAlphaMP2 = 0;
        this->m_nWeaponGunFlashAlphaProgMP2 = 0;
        this->m_pCoverPoint = nullptr;
        this->m_pEnex = nullptr;
        this->field_798 = -1;
        // TODO(port): decomp used operator_new(0x294) + explicit CPedIntelligence::Constructor; port uses new
        pCVar7 = new CPedIntelligence(this); // TODO(port): class has custom operator new(size_t)
        local_4 = (local_4 & ~0xFFu) | ((5 & 0xFF) << 0);
        if (pCVar7 == nullptr) {
            pCVar7 = nullptr;
        }
        this->m_pIntelligence = pCVar7;
        local_4 = (local_4 & ~0xFFu) | ((4 & 0xFF) << 0);
        uVar3 = (uint8_t)local_4;
        local_4 = (local_4 & ~0xFFu) | ((4 & 0xFF) << 0);
        this->m_pPlayerData = nullptr;
        if ((this->m_nPedType != PED_TYPE_PLAYER1) &&
        (uVar3 = (uint8_t)local_4, this->m_nPedType != PED_TYPE_PLAYER2)) {
            // TODO(port): decomp used CTask::operator_new(0x20) + explicit CTaskComplexFacial::Constructor; port uses new
            pCVar8 = new (std::nothrow) CTaskComplexFacial();
            local_4 = (local_4 & ~0xFFu) | ((6 & 0xFF) << 0);
            if (pCVar8 == nullptr) {
                pCVar8 = nullptr;
            }
            local_4 = (local_4 & ~0xFFu) | ((4 & 0xFF) << 0);
            // TODO(port): decomp static-style call with explicit this
            this->m_pIntelligence->m_TaskMgr.SetTaskSecondary((CTask *)pCVar8,TASK_SECONDARY_FACIAL_COMPLEX);
            uVar3 = (uint8_t)local_4;
        }
        local_4 = (local_4 & ~0xFFu) | ((uVar3 & 0xFF) << 0);
        // TODO(port): decomp used CTask::operator_new(0x20) + explicit CTaskSimpleStandStill::Constructor; port uses new
        pCVar9 = new (std::nothrow) CTaskSimpleStandStill(0, true, false, 8.0f);
        local_4 = (local_4 & ~0xFFu) | ((7 & 0xFF) << 0);
        if (pCVar9 == nullptr) {
            pCVar9 = nullptr;
        }
        local_4 = (local_4 & ~0xFFu) | ((4 & 0xFF) << 0);
        // TODO(port): decomp static-style call with explicit this
        this->m_pIntelligence->m_TaskMgr.SetTask((CTask *)pCVar9,TASK_PRIMARY_DEFAULT,false);
        this->m_Wobble = 0.0;
        this->m_fRemovalDistMultiplier = 1.0;
        this->m_StreamedScriptBrainToLoad = -1;
        CPopulation::UpdatePedCount(this,false);
        uVar11 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        uVar11 = uVar11 & 0xdfffffff;
        bDonePositionOutOfCollision = (char)uVar11;
        bKilledByStealth = (char)(uVar11 >> 8);
        bRightArmBlocked = (char)(uVar11 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar11 >> 0x18);
        this->SetFourthPedFlags(this->GetFourthPedFlags() & 0xffffffdf);
        if (((CCheat::m_aCheatsActive[0xf] != false) && (this->m_nPedType != PED_TYPE_PLAYER1)) &&
        (this->m_nPedType != PED_TYPE_PLAYER2)) {
            uVar4 = CPedType::GetPedFlag(PED_TYPE_PLAYER1);
            CAcquaintance::SetAsAcquaintance(&this->m_acquaintance,4,uVar4);
            ped = FindPlayerPed(-1);
            // TODO(port): decomp constructed CEventAcquaintancePed in place, then swapped its vtable to the
            //   Hate variant (twice) with an explicit ctor/dtor call at the end; the port constructs the
            //   Hate subclass directly via placement new (local_24 destroys at scope end)
            new (&local_24) CEventAcquaintancePedHate((CPed *)ped);
            local_4 = (local_4 & ~0xFFu) | ((8 & 0xFF) << 0);
            local_24.m_TaskId = 1000;
            // TODO(port): decomp static-style call with explicit this
            this->m_pIntelligence->m_eventGroup.Add((CEvent *)&local_24,false);
            local_4 = (local_4 & ~0xFF) | 4; // TODO(port): was CONCAT31(local_4._1_3_,4)
        }
        // TODO(port): decomp ctor returned `this`; C++ ctors return nothing
}












CPed::~CPed() {    // converted from decomp src/CPed/Destructor_005e8620.c
    // TODO(convert): DAT_ global
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): uint8_t-slice
    // TODO(convert): vtable write
        short sVar1;
        CPedIntelligence *this_00;
        int iVar2;
        CPed *pCVar3;
        uint32_t uVar4;
        int iStack_4;

        iStack_4 = 4;
        CReplay::RecordPedDeleted(this);
        if ((bWaitingForScriptBrainToLoad & 1) != 0) {
            CStreaming::SetMissionDoesntRequireModel
            (*(short *)(CTheScripts::ScriptsForBrains.m_aScriptForBrains +
            this->m_StreamedScriptBrainToLoad * 0x14) + 0x6676);
            uVar4 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
            sVar1 = this->m_StreamedScriptBrainToLoad;
            uVar4 = uVar4 & 0xfeffffff;
            bDonePositionOutOfCollision = (char)uVar4;
            bKilledByStealth = (char)(uVar4 >> 8);
            bRightArmBlocked = (char)(uVar4 >> 0x10);
            bWaitingForScriptBrainToLoad = (char)(uVar4 >> 0x18);
            CTheScripts::RemoveFromWaitingForScriptBrainArray(this,sVar1);
            this->m_StreamedScriptBrainToLoad = -1;
        }
        CWorld::Remove((CEntity *)this);
        CRadar::ClearBlipForEntity(BLIP_CHAR, 0u /* TODO(port): GetPedPool()->GetRef(this) disabled for C1202 */); // TODO(port): decomp pool arithmetic
        CConversations::RemoveConversationForPed(this);
        if (this->m_pVehicle != nullptr) {
            ((CEntity *)this->m_pVehicle)->CleanUpOldReference((CEntity **)&this->m_pVehicle); // TODO(port): decomp static-style call
        }
        this->m_pVehicle = nullptr;
        if (this->m_pFire != nullptr) {
            this->m_pFire->Extinguish(); // TODO(port): decomp static-style call
        }
        if (this->m_pCoverPoint != nullptr) {
            this->m_pCoverPoint->ReleaseCoverPointForPed(this); // TODO(port): decomp static-style call
            this->m_pCoverPoint = nullptr;
        }
        this->ClearWeapons(); // TODO(port): decomp static-style call
        if ((bMiamiViceCop & 1) != 0) {
            CPopulation::NumMiamiViceCops = CPopulation::NumMiamiViceCops - 1;
        }
        CPopulation::UpdatePedCount(this,true);
        // TODO(port): decomp made an explicit virtual dtor call on m_pedSpeech here; members destroy automatically
        (this->m_weaponAudio).Terminate();
        (this->m_pedAudio).Terminate();
        this_00 = this->m_pIntelligence;
        if (this_00 != nullptr) {
            // TODO(port): decomp made an explicit CPedIntelligence::Destructor call; delete runs the dtor
            delete this_00;
        }
        if (this->m_pLookTarget != nullptr) {
            CleanUpOldReference(&m_pLookTarget);
        }
        iStack_4 = (iStack_4 & ~0xFFu) | ((3 & 0xFF) << 0);
        // TODO(port): decomp ran the CWeapon dtors via _eh_vector_destructor_iterator_; members destroy automatically
        iStack_4 = (iStack_4 & ~0xFFu) | ((2 & 0xFF) << 0);
        // TODO(port): decomp reset m_weaponAudio's vtable before member teardown; automatic in port
        // TODO(port): decomp explicit CAESound dtor call; members destroy automatically
        iStack_4 = (iStack_4 & ~0xFFu) | ((1 & 0xFF) << 0);
        // TODO(port): decomp reset m_pedSpeech's vtable before member teardown; automatic in port
        // TODO(port): decomp explicit CAESound dtor call; members destroy automatically
        iStack_4 = iStack_4 & ~0xFF; // TODO(port): was (uint32_t)iStack_4._1_3_ << 8
        // TODO(port): decomp explicit CAEPedAudioEntity ctor/dtor call; m_pedAudio destroys automatically
        iStack_4 = 0xffffffff;
        // TODO(port): decomp called CPhysical::Destructor explicitly and returned this (scalar deleting
        //   dtor); the base destructor runs automatically and a C++ dtor returns nothing
}












bool CPed::PedIsInvolvedInConversation() {    // converted from decomp src/CPed/*.c
        return this == CPedToPlayerConversations::m_pPed;
}










bool CPed::PedIsReadyForConversation(bool arg0) {    // converted from decomp src/CPed/*.c
    // TODO(port): full port from src/CPed/PedIsReadyForConversation_0156cae0.c. The decomp is
    //   artifact soup (raw this+offset reads, EDX:out params via CONCAT44, LOCK/UNLOCK, int3) and the
    //   real function returns a 64-bit EDX:EAX pair; the bool here is the AL part. Stubbed until the
    //   task/event systems it depends on are ported.
    (void)arg0;
    return false;
}



bool CPed::PedCanPickUpPickUp() {    // converted from decomp src/CPed/*.c
        CTask *pCVar1;

        // TODO(port): decomp static-style call with explicit this
        pCVar1 = (CWorld::Players[0].m_pPed)->m_pIntelligence->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER);
        if (pCVar1 == nullptr) {
            // TODO(port): decomp static-style call with explicit this
            pCVar1 = (CWorld::Players[0].m_pPed)->m_pIntelligence->FindTaskByType(TASK_COMPLEX_USE_MOBILE_PHONE);
            if (pCVar1 == nullptr) {
                return true;
            }
        }
        return false;
}










void CPed::CreateDeadPedMoney() {    // converted from decomp src/CPed/*.c
        int iVar1;
        bool bVar2;

        bVar2 = CLocalisation::StealFromDeadPed();
        if (((((bVar2) && (iVar1 = *(int *)(this + 0x598), iVar1 != 6)) && (iVar1 != 0x12)) &&
        ((iVar1 != 0x13 &&
        ((*(char *)(this + 0x484) != '\x02' || ((*(uint32_t *)(this + 0x470) & 0x20000) != 0))))))
        && (((*(uint32_t *)(this + 0x46c) & 0x100) == 0 && (9 < *(uint16_t *)(this + 0x756))))) {
            LOCK();
            UNLOCK();
            return;
        }
        return;
}










void CPed::CreateDeadPedPickupCoors(float& outPickupX,  float& outPickupY,  float& outPickupZ) {    // converted from decomp src/CPed/*.c
        int iVar1;
        int iVar2;
        int iVar3;
        float *pfVar4;

        iVar3 = *(int *)((uint8_t *)this + 0x14); // TODO(port): decomp byte address
        iVar1 = iVar3 + 0x30;
        if (iVar3 == 0) {
            iVar1 = (int)((uint8_t *)this + 4); // TODO(port): decomp byte address
            iVar2 = (int)((uint8_t *)this + 4); // TODO(port): decomp byte address
            pfVar4 = (float *)((uint8_t *)this + 4); // TODO(port): decomp byte address
        }
        else {
            iVar2 = iVar3 + 0x30;
            pfVar4 = (float *)(iVar3 + 0x30);
        }
        CPickups::CreatePickupCoorsCloseToCoors
        (*pfVar4,*(float *)(iVar2 + 4),*(float *)(iVar1 + 8),outPickupX,outPickupY,outPickupZ);
        return;
}










void CPed::CreateDeadPedWeaponPickups() {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        float fVar1;
        CVector posn;
        uint32_t uVar2;
        uint32_t uVar3;
        bool bVar4;
        uint32_t uVar5;
        uint32_t uVar6;
        CWeapon *this_00;
        uint32_t uVar7;
        int local_10;
        float local_c; // TODO(port): decomp had these as raw dwords; they carry float coords
        float local_8; // TODO(port): decomp had these as raw dwords; they carry float coords
        float local_4;

        uVar5 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if (((uVar5 & 0x100) == 0) &&
        ((bIsInTheAir) == 0)) {
            this_00 = this->m_aWeapons.data(); // TODO(port): decomp decayed the array
            local_10 = 0xd;
            do {
                if (((this_00->m_Type != WEAPON_UNARMED) && (this_00->m_Type != WEAPON_DETONATOR)) &&
                ((this_00->m_TotalAmmo != 0 || (bVar4 = this_00->IsTypeMelee(), bVar4)))) { // TODO(port): decomp static-style call
                    CreateDeadPedPickupCoors(local_c,local_8,local_4); // TODO(port): decomp passed pointers; sig takes float&
                    uVar3 = local_8;
                    uVar2 = local_c;
                    fVar1 = local_4 + 0.3;
                    uVar5 = (uint32_t)(DAT_008a5f50[(size_t)this_00->m_Type] /* TODO(port): decomp byte-addressed global */ >> 1);
                    if ((int)this_00->m_TotalAmmo <
                    (int)(uint32_t)(DAT_008a5f50[(size_t)this_00->m_Type] /* TODO(port): decomp byte-addressed global */ >> 1)) {
                        uVar5 = this_00->m_TotalAmmo;
                    }
                    posn.y = (float)local_8;
                    posn.x = (float)local_c;
                    posn.z = fVar1;
                    local_4 = fVar1;
                    bVar4 = CPickups::TryToMerge_WeaponType
                    (posn,this_00->m_Type,PICKUP_ONCE_TIMEOUT,uVar5,false);
                    if (!bVar4) {
                        uVar5 = this_00->m_TotalAmmo;
                        if ((this->GetFourthPedFlags() & 0x80000) == 0) {
                            uVar6 = (uint32_t)(DAT_008a5f50[(size_t)this_00->m_Type] /* TODO(port): decomp byte-addressed global */ >> 1);
                            if ((int)uVar5 < (int)uVar6) {
                                uVar6 = uVar5;
                            }
                            uVar7 = 4;
                        }
                        else {
                            uVar6 = (uint32_t)(DAT_008a5f50[(size_t)this_00->m_Type] /* TODO(port): decomp byte-addressed global */ >> 1);
                            if ((int)uVar5 < (int)uVar6) {
                                uVar6 = uVar5;
                            }
                            uVar7 = 0x16;
                        }
                        // TODO(port): decomp passed (x,y,z,...) as 8 args; adapted to the ported 6-arg signature
                    CPickups::GenerateNewOne_WeaponType(CVector(local_c, local_8, fVar1), this_00->m_Type, (ePickupType)uVar7, uVar6, false, nullptr);
                    }
                }
                this_00 = this_00 + 1;
                local_10 = local_10 + -1;
            } while (local_10 != 0);
            this->ClearWeapons(); // TODO(port): decomp static-style call
        }
        return;
}










void CPed::Initialise() {    // converted from decomp src/CPed/*.c
        CPedType::Initialise();
        CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle();
        return;
}










void CPed::SetPedStats(ePedStats statsType) {    // converted from decomp src/CPed/*.c
        this->m_pStats = CPedStats::ms_apPedStats[(size_t)statsType]; // TODO(port): decomp rendered indexing as pointer arithmetic
        return;
}










void CPed::Update() {    // converted from decomp src/CPed/*.c
        return;
}










void CPed::SetMoveState(eMoveState moveState) {    // converted from decomp src/CPed/*.c
        this->m_nMoveState = moveState;
        return;
}










void CPed::SetMoveAnimSpeed(CAnimBlendAssociation* association) {    // converted from decomp src/CPed/*.c
        float fVar1;
        bool bVar2;

        bVar2 = 0.3 < (this->m_pedIK).m_fSlopePitch;
        if (this->m_nCreatedBy != PED_MISSION) {
            if ((bVar2) || (-0.3 <= (this->m_pedIK).m_fSlopePitch)) {
                if ((this->m_pedIK).m_fSlopePitch <= 0.3) {
                    fVar1 = (this->m_pedIK).m_fSlopePitch;
                }
                else {
                    fVar1 = 0.3;
                }
            }
            else {
                fVar1 = -0.3;
            }
            association->m_Speed = (fVar1 + 1.2) - (float)this->m_nRandomSeed * 0.4 * 3.051851e-05
            ;
            return;
        }
        if ((!bVar2) && ((this->m_pedIK).m_fSlopePitch < -0.3)) {
            association->m_Speed = 0.7;
            return;
        }
        if (0.3 < (this->m_pedIK).m_fSlopePitch) {
            association->m_Speed = 1.3;
            return;
        }
        association->m_Speed = (this->m_pedIK).m_fSlopePitch + 1.0;
        return;
}










void CPed::StopNonPartialAnims() {    // converted from decomp src/CPed/*.c
        CAnimBlendAssociation *association;

        for (association = RpAnimBlendClumpGetFirstAssociation((RpClump *)GetRwObject());
        association != nullptr;
        association = RpAnimBlendGetNextAssociation(association)) {
            if (((uint8_t)association->m_Flags >> 4 & 1) == 0) {
                association->m_Flags = association->m_Flags & 0xfffe;
            }
        }
        return;
}










void CPed::RestartNonPartialAnims() {    // converted from decomp src/CPed/*.c
        CAnimBlendAssociation *association;

        for (association = RpAnimBlendClumpGetFirstAssociation((RpClump *)GetRwObject());
        association != nullptr;
        association = RpAnimBlendGetNextAssociation(association)) {
            if (((uint8_t)association->m_Flags >> 4 & 1) == 0) {
                association->m_Flags = association->m_Flags | 1;
            }
        }
        return;
}










bool CPed::CanUseTorsoWhenLooking() const {    // converted from decomp src/CPed/*.c
        uint32_t uVar1;

        if (((this->m_nPedState != PEDSTATE_DRIVING) && (this->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR))
        && ((bIsDucking) == 0)) {
            return true;
        }
        return false;
}










void CPed::SetLookFlag(float lookHeading,  bool likeUnused,  bool arg2) {    // converted from decomp src/CPed/SetLookFlag_005dedc0.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        CEntity **entity;
        uint32_t uVar2;
        uint32_t uVar3;

        if ((this->m_nLookTime < CTimer::m_snTimeInMilliseconds) || (arg2)) {
            uVar3 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            uVar3 = uVar3 & 0xfffffff7 | 4;
            entity = &this->m_pLookTarget;
            bIsStanding = (char)uVar3;
            bInVehicle = (char)(uVar3 >> 8);
            bFiringWeapon = (char)(uVar3 >> 0x10);
            bNotAllowedToDuck = (char)(uVar3 >> 0x18);
            this->m_fLookDirection = lookHeading;
            if (*entity != nullptr) {
                (*entity)->CleanUpOldReference(entity);
            }
            *entity = nullptr;
            this->m_nLookTime = 0;
            if (((this->m_nPedState != PEDSTATE_DRIVING) && (this->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR)
            ) && ((bIsDucking) == 0))
            {
                // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
                // TODO(port): dropped (pCVar1 undeclared); original touched opaque m_pedIK.m_nFlags - see comment above
            }
        }
        return;
}












void CPed::SetLookFlag(CEntity* lookingTo,  bool likeUnused,  bool arg2) {    // converted from decomp src/CPed/SetLookFlag_005dee40.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        CEntity **entity;
        uint32_t uVar2;
        uint32_t uVar3;

        if ((this->m_nLookTime < CTimer::m_snTimeInMilliseconds) || (arg2)) {
            uVar3 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            uVar3 = uVar3 & 0xfffffff7 | 4;
            entity = &this->m_pLookTarget;
            bIsStanding = (char)uVar3;
            bInVehicle = (char)(uVar3 >> 8);
            bFiringWeapon = (char)(uVar3 >> 0x10);
            bNotAllowedToDuck = (char)(uVar3 >> 0x18);
            if (*entity != nullptr) {
                (*entity)->CleanUpOldReference(entity);
            }
            *entity = lookingTo;
            lookingTo->RegisterReference(entity); // TODO(port): decomp static-style call
            this->m_fLookDirection = 999999.0;
            this->m_nLookTime = 0;
            if (((this->m_nPedState != PEDSTATE_DRIVING) && (this->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR)
            ) && ((bIsDucking) == 0))
            {
                // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
                // TODO(port): dropped (pCVar1 undeclared); original touched opaque m_pedIK.m_nFlags - see comment above
            }
        }
        return;
}












void CPed::SetAimFlag(CEntity* aimingTo) {    // converted from decomp src/CPed/SetAimFlag_005deed0.c
    // TODO(convert): uint8_t-slice
        CEntity **entity;
        CEntity *this_00;
        uint32_t uVar1;

        uVar1 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        this_00 = this->m_pLookTarget;
        entity = &this->m_pLookTarget;
        uVar1 = uVar1 & 0xffffffdf | 0x10;
        bIsStanding = (char)uVar1;
        bInVehicle = (char)(uVar1 >> 8);
        bFiringWeapon = (char)(uVar1 >> 0x10);
        bNotAllowedToDuck = (char)(uVar1 >> 0x18);
        if (this_00 != nullptr) {
            this_00->CleanUpOldReference(entity); // TODO(port): decomp static-style call
        }
        *entity = aimingTo;
        aimingTo->RegisterReference(entity); // TODO(port): decomp static-style call
        this->m_nLookTime = 0;
        return;
}












void CPed::ClearAimFlag() {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        uint32_t uVar2;

        uVar2 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar2 & 0x10) != 0) {
            uVar2 = uVar2 & 0xffffffef | 0x20;
            bIsStanding = (char)uVar2;
            bInVehicle = (char)(uVar2 >> 8);
            bFiringWeapon = (char)(uVar2 >> 0x10);
            bNotAllowedToDuck = (char)(uVar2 >> 0x18);
            // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
            // TODO(port): dropped (pCVar1 undeclared); original touched opaque m_pedIK.m_nFlags - see comment above
            this->m_nLookTime = 0;
        }
        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_fLookPitch = 0.0;
        }
        return;
}










int32_t CPed::GetLocalDirection(const CVector2D& point) const {    // converted from decomp src/CPed/*.c
        int iVar1;
        long double /* Ghidra float10 */ fVar2;

        fVar2 = atan2(-(double)point.x, (double)point.y); // TODO(port): decomp fpatan
        fVar2 = (fVar2 - (long double /* Ghidra float10 */)this->m_fCurrentRotation) + (long double /* Ghidra float10 */)0.7853982;
        if (fVar2 < (long double /* Ghidra float10 */)0.0) {
            do {
                fVar2 = fVar2 + (long double /* Ghidra float10 */)6.2831855;
            } while (fVar2 < (long double /* Ghidra float10 */)0.0);
        }
        iVar1 = CGeneral::GetRandomNumber();
        if (3 < iVar1) {
            iVar1 = iVar1 + (-4 - (iVar1 - 4U & 0xfffffffc));
        }
        return iVar1;
}










bool CPed::IsPedShootable() const {    // converted from decomp src/CPed/*.c
        return (int)this->m_nPedState < 0x2f;
}










bool CPed::UseGroundColModel() const {    // converted from decomp src/CPed/*.c
        ePedState eVar1;

        eVar1 = this->m_nPedState;
        if ((((eVar1 != PEDSTATE_FALL) && (eVar1 != PEDSTATE_EVADE_DIVE)) && (eVar1 != PEDSTATE_DIE)) &&
        (eVar1 != PEDSTATE_DEAD)) {
            return false;
        }
        return true;
}










bool CPed::CanPedReturnToState() const {    // converted from decomp src/CPed/*.c
        ePedState eVar1;

        eVar1 = this->m_nPedState;
        if (((((int)eVar1 < 0x27) && (eVar1 != PEDSTATE_AIMGUN)) && (eVar1 != PEDSTATE_ATTACK)) &&
        (((eVar1 != PEDSTATE_FIGHT && (eVar1 != PEDSTATE_EVADE_STEP)) &&
        ((eVar1 != PEDSTATE_SNIPER_MODE && (eVar1 != PEDSTATE_LOOK_ENTITY)))))) {
            return true;
        }
        return false;
}










bool CPed::CanSetPedState() const {    // converted from decomp src/CPed/*.c
        ePedState eVar1;

        eVar1 = this->m_nPedState;
        if ((((eVar1 != PEDSTATE_DIE) && (eVar1 != PEDSTATE_DEAD)) && (eVar1 != PEDSTATE_ARRESTED)) &&
        (((eVar1 != PEDSTATE_ENTER_CAR && (eVar1 != PEDSTATE_CARJACK)) && (eVar1 != PEDSTATE_STEAL_CAR)
        ))) {
            return true;
        }
        return false;
}










bool CPed::CanBeArrested() const {    // converted from decomp src/CPed/*.c
        ePedState eVar1;

        eVar1 = this->m_nPedState;
        if ((((eVar1 != PEDSTATE_DIE) && (eVar1 != PEDSTATE_DEAD)) && (eVar1 != PEDSTATE_ARRESTED)) &&
        ((eVar1 != PEDSTATE_ENTER_CAR && (eVar1 != PEDSTATE_EXIT_CAR)))) {
            return true;
        }
        return false;
}










bool CPed::CanStrafeOrMouseControl() const {    // converted from decomp src/CPed/*.c
        ePedState eVar1;

        eVar1 = this->m_nPedState;
        if ((((((eVar1 != PEDSTATE_IDLE) && (eVar1 != PEDSTATE_FLEE_ENTITY)) &&
        (eVar1 != PEDSTATE_FLEE_POSITION)) &&
        ((eVar1 != PEDSTATE_NONE && (eVar1 != PEDSTATE_AIMGUN)))) &&
        ((eVar1 != PEDSTATE_ATTACK && ((eVar1 != PEDSTATE_FIGHT && (eVar1 != PEDSTATE_JUMP)))))) &&
        (eVar1 != PEDSTATE_ANSWER_MOBILE)) {
            return false;
        }
        return true;
}










bool CPed::CanBeDeleted() {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
        ePedCreatedBy eVar1;
        uint32_t uVar2;
        bool bVar3;
        CPlayerPed *pCVar4;

        uVar2 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar2 & 0x100) == 0) {
            pCVar4 = FindPlayerPed(-1);
            bVar3 = CPedGroupMembership::IsFollower
            (&CPedGroups::ms_groups[pCVar4->m_pPlayerData->m_nPlayerGroup].
            m_groupMembership,this);
            if ((!bVar3) &&
            ((eVar1 = this->m_nCreatedBy, eVar1 == PED_GAME ||
            ((eVar1 != PED_MISSION && (eVar1 != PED_GAME_MISSION)))))) {
                return true;
            }
        }
        return false;
}










bool CPed::CanBeDeletedEvenInVehicle() const {    // converted from decomp src/CPed/*.c
        ePedCreatedBy eVar1;

        eVar1 = this->m_nCreatedBy;
        if ((eVar1 != PED_GAME) && ((eVar1 == PED_MISSION || (eVar1 == PED_GAME_MISSION)))) {
            return false;
        }
        return true;
}










void CPed::RemoveGogglesModel() {    // converted from decomp src/CPed/*.c
        CClumpModelInfo *this_00;
        RpAtomic *pRVar1;
        int iVar2;

        if (this->m_pGogglesObject != nullptr) {
            this_00 = CVisibilityPlugins::GetClumpModelInfo(this->m_pGogglesObject);
            ((CBaseModelInfo *)this_00)->RemoveRef(); // TODO(port): decomp static-style call
            pRVar1 = GetFirstAtomic(this->m_pGogglesObject);
            if (pRVar1 != nullptr) {
                iVar2 = (RpSkinGeometryGetSkin(pRVar1->geometry) != nullptr); // TODO(port): decomp treated the RpSkin* as int
                if (iVar2 != 0) {
                    RpClumpForAllAtomics(this->m_pGogglesObject,AtomicRemoveAnimFromSkinCB,0);
                }
            }
            RpClumpDestroy(this->m_pGogglesObject);
            this->m_pGogglesObject = nullptr;
            if (this->m_pGogglesState != nullptr) {
                *this->m_pGogglesState = false;
                this->m_pGogglesState = nullptr;
            }
        }
        return;
}










int32_t CPed::GetWeaponSlot(eWeaponType weaponType) {    // converted from decomp src/CPed/*.c
        CWeaponInfo *pCVar1;

        pCVar1 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD); // TODO(port): decomp eWeaponSkill::STD enum
        return (int32_t)pCVar1->m_nSlot; // TODO(port): m_nSlot is eWeaponSlot
}










void CPed::GrantAmmo(eWeaponType weaponType,  uint32_t ammo) {    // converted from decomp src/CPed/*.c
        int iVar1;
        CWeaponInfo *pCVar2;
        int iVar3;

        pCVar2 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
        iVar1 = (int32_t)pCVar2->m_nSlot; // TODO(port): m_nSlot is eWeaponSlot
        if (iVar1 != -1) {
            iVar3 = m_aWeapons[iVar1].m_TotalAmmo + ammo;
            m_aWeapons[iVar1].m_TotalAmmo = iVar3;
            if (0x1869e < iVar3) {
                iVar3 = 99999;
            }
            m_aWeapons[iVar1].m_TotalAmmo = iVar3;
            if ((m_aWeapons[iVar1].m_State == WEAPONSTATE_OUT_OF_AMMO) && (0 < iVar3)) {
                m_aWeapons[iVar1].m_State = static_cast<eWeaponState>(0);
            }
        }
        return;
}










void CPed::SetAmmo(eWeaponType weaponType,  uint32_t ammo) {    // converted from decomp src/CPed/*.c
        int iVar1;
        CWeaponInfo *pCVar2;
        uint32_t uVar3;

        pCVar2 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
        iVar1 = (int32_t)pCVar2->m_nSlot; // TODO(port): m_nSlot is eWeaponSlot
        if (iVar1 != -1) {
            this->m_aWeapons[iVar1].m_TotalAmmo = ammo; // TODO(port): decomp byte arithmetic (offset 0xc)
            if (0x1869e < (int)ammo) {
                ammo = 99999;
            }
            this->m_aWeapons[iVar1].m_TotalAmmo = ammo; // TODO(port): decomp byte arithmetic (offset 0xc)
            uVar3 = this->m_aWeapons[iVar1].m_AmmoInClip /* TODO(port): decomp byte arithmetic (offset 8) */;
            if ((int)ammo < (int)this->m_aWeapons[iVar1].m_AmmoInClip /* TODO(port): decomp byte arithmetic (offset 8) */) {
                uVar3 = ammo;
            }
            this->m_aWeapons[iVar1].m_AmmoInClip /* TODO(port): decomp byte arithmetic (offset 8) */ = uVar3;
            if ((m_aWeapons[iVar1].m_State == WEAPONSTATE_OUT_OF_AMMO) && (0 < (int)ammo)) {
                m_aWeapons[iVar1].m_State = static_cast<eWeaponState>(0);
            }
        }
        return;
}










bool CPed::DoWeHaveWeaponAvailable(eWeaponType weaponType) {    // converted from decomp src/CPed/*.c
        CWeaponInfo *pCVar1;

        pCVar1 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD); // TODO(port): decomp eWeaponSkill::STD enum
        if (((int32_t)pCVar1->m_nSlot != -1) && /* TODO(port): m_nSlot is eWeaponSlot */
        (this->m_aWeapons[(size_t)pCVar1->m_nSlot].m_Type == weaponType)) { // TODO(port): decomp byte arithmetic on array
            return true;
        }
        return false;
}










bool CPed::DoGunFlash(int32_t lifetime,  bool bRightHand) {    // converted from decomp src/CPed/*.c
    // TODO(convert): DAT_ global
        int iVar1;

        if ((this->m_pWeaponObject != nullptr) && (this->m_pGunflashObject != nullptr)) {
            if (bRightHand) {
                this->m_nWeaponGunflashAlphaMP2 = DAT_008d1370;
                this->m_nWeaponGunFlashAlphaProgMP2 =
                (short)((int64_t)(uint64_t)DAT_008d1370 / (int64_t)lifetime);
            }
            else {
                this->m_nWeaponGunflashAlphaMP1 = DAT_008d1370;
                this->m_nWeaponGunFlashAlphaProgMP1 =
                (short)((int64_t)(uint64_t)DAT_008d1370 / (int64_t)lifetime);
            }
            iVar1 = CGeneral::GetRandomNumber();
            RwMatrixRotate(&this->m_pGunflashObject->modelling,&DAT_008d232c,
            (float)iVar1 * 3.051851e-05 * 720.0 - 360.0,(RwOpCombineType)1); // TODO(port): decomp int combine op
            return true;
        }
        return false;
}










void CPed::SetGunFlashAlpha(bool rightHand) {    // converted from decomp src/CPed/*.c
        RpAtomic *atomic;
        int alpha;
        short sVar1;

        if (rightHand) {
            sVar1 = this->m_nWeaponGunflashAlphaMP2;
        }
        else {
            sVar1 = this->m_nWeaponGunflashAlphaMP1;
        }
        if ((this->m_pGunflashObject != nullptr) &&
        ((-1 < this->m_nWeaponGunflashAlphaMP1 || (-1 < this->m_nWeaponGunflashAlphaMP2)))) {
            atomic = (RpAtomic *)GetFirstObject(this->m_pGunflashObject);
            if (atomic != nullptr) {
                alpha = CGeneral::GetRandomNumber();
                if (sVar1 < 1) {
                    alpha = 0;
                }
                CVehicle::SetComponentAtomicAlpha(atomic,alpha);
                atomic->object.flags = '\x04'; // TODO(port): decomp had (atomic->object).object.flags
            }
            if (rightHand) {
                if (this->m_nWeaponGunflashAlphaMP2 == 0) {
                    this->m_nWeaponGunflashAlphaMP2 = -1;
                }
            }
            else if (this->m_nWeaponGunflashAlphaMP1 == 0) {
                this->m_nWeaponGunflashAlphaMP1 = -1;
                return;
            }
        }
        return;
}










void CPed::ResetGunFlashAlpha() {    // converted from decomp src/CPed/*.c
        RpAtomic *atomic;

        if (this->m_pGunflashObject != nullptr) {
            atomic = (RpAtomic *)GetFirstObject(this->m_pGunflashObject);
            if (atomic != nullptr) {
                atomic->object.flags = '\0'; // TODO(port): decomp had (atomic->object).object.flags
                CVehicle::SetComponentAtomicAlpha(atomic,0);
            }
        }
        return;
}










float CPed::GetBikeRidingSkill() const {    // converted from decomp src/CPed/*.c
        float fVar1;

        fVar1 = 0.0;
        if (this->m_pPlayerData == nullptr) {
            if (this->m_nCreatedBy == PED_MISSION) {
                fVar1 = 1.0;
            }
        }
        else {
            fVar1 = CStats::GetStatValue(STAT_BIKE_SKILL);
            fVar1 = fVar1 / 1000.0;
            if (1.0 < fVar1) {
                return 1.0;
            }
        }
        return fVar1;
}










void CPed::ShoulderBoneRotation(RpClump* clump) {    // converted from decomp src/CPed/*.c
    // TODO(port): decomp body used Ghidra artifacts (RwMatrixTag, int-held RwMatrix*,
    //   explicit CMatrix ctor/dtor calls, static-style member calls, exception-state
    //   temporaries). Rewritten against the real CMatrix/RwMatrix API preserving the
    //   decomp's operation order. Decomp-local mapping: local_54=matA, local_9c=matB,
    //   local_e4=tmp, local_12c=work, fStack_138/130/134=euler x/z/y.
    RpHAnimHierarchy* pHier = GetAnimHierarchyFromSkinClump(clump);
    RwMatrix* pMatrices = RpHAnimHierarchyGetMatrixArray(pHier);

    // Copy bone 0x20's matrix over bone 0x12e's matrix (decomp: 16-float loop).
    RwMatrix* pShoulderMat = &pMatrices[RpHAnimIDGetIndex(pHier, 0x12e)];
    *pShoulderMat = pMatrices[RpHAnimIDGetIndex(pHier, 0x20)];

    CMatrix matA(&pMatrices[RpHAnimIDGetIndex(pHier, 0x1f)], false); // decomp local_54
    CMatrix work(pShoulderMat, false);                              // decomp local_12c
    CMatrix matB;                                                   // decomp local_9c
    Invert(matA, matB);                                             // decomp: Invert(&local_54,&local_9c)
    CMatrix tmp = matA * work;                                      // decomp: operator*(&local_e4,...)
    work = tmp;
    float eulX, eulY, eulZ;
    work.ConvertToEulerAngles(&eulX, &eulZ, &eulY, 0x15);
    if (DAT_008d21e0 != '\0') {
        eulX = eulX * 0.5f;
    }
    work.ConvertFromEulerAngles(eulX, eulZ, eulY, 0x15);
    tmp = matB * work;
    work = tmp;
    work.UpdateRW();

    // Copy bone 0x16's matrix over bone 0x12d's matrix (decomp: 16-float component loop).
    RwMatrix* pDstMat = &pMatrices[RpHAnimIDGetIndex(pHier, 0x12d)];
    *pDstMat = pMatrices[RpHAnimIDGetIndex(pHier, 0x16)];

    work.Attach(pDstMat, false);                                    // decomp: CMatrix::Attach(&local_12c,matrix,false)
    matB.Attach(&pMatrices[RpHAnimIDGetIndex(pHier, 0x15)], false);  // decomp: CMatrix::Attach(&local_9c,...,false)
    Invert(matB, tmp);                                              // decomp: pCVar4 = Invert(&local_e4,&local_9c)
    matA = tmp;
    tmp = matA * work;
    work = tmp;
    work.ConvertToEulerAngles(&eulX, &eulZ, &eulY, 0x15);
    if (DAT_008d21e0 != '\0') {
        eulX = eulX * 0.5f;
    }
    work.ConvertFromEulerAngles(eulX, eulZ, eulY, 0x15);
    tmp = matB * work;
    work = tmp;
    work.UpdateRW();
}










void CPed::SetLookTimer(uint32_t time) {    // converted from decomp src/CPed/*.c
        if (this->m_nLookTime < CTimer::m_snTimeInMilliseconds) {
            this->m_nLookTime = CTimer::m_snTimeInMilliseconds + time;
        }
        return;
}










bool CPed::IsPlayer() const {    // converted from decomp src/CPed/*.c
        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            return false;
        }
        return true;
}










void CPed::SetPedPositionInCar() {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): goto
        CBaseModelInfo *pCVar1;
        CBike *this_00;
        float fVar2;
        CVehicle *pCVar3;
        uint8_t auVar4 [12];
        float *pfVar5;
        CMatrixLink *pCVar6;
        RwObject *pRVar7;
        long double /* Ghidra float10 */ fVar8;
        void *local_c4;
        void *local_c0;
        void *local_bc;
        float local_b8;
        float fStack_b4;
        float fStack_b0;
        float fStack_ac;
        CMatrix local_a8;
        CMatrix local_60;
        uint8_t local_18 [12];
        void *local_c;
        uint32_t local_4;

        if (CReplay::Mode == CReplay::MODE_PLAYBACK /* TODO(port): decomp used bare MODE_PLAYBACK */) {
            return;
        }
        pCVar1 = CModelInfo::ms_modelInfoPtrs[(short)this->m_pVehicle->m_nModelIndex];
        local_a8 = *this->m_pVehicle->m_matrix; // TODO(port): decomp explicit ctor call; m_matrix is CMatrixLink*
        local_a8.m_pAttachMatrix = nullptr;
        local_a8.m_bOwnsAttachedMatrix = false;
        // TODO(port): dropped decomp bitfield local_a8._69_3_ (unknown CMatrix flag)
        this_00 = (CBike *)this->m_pVehicle;
        local_4 = 1;
        if (this == this_00->m_pDriver) {
            pRVar7 = pCVar1[2].GetRwObject();
            if (pCVar1[1].GetRwObject() != (RwObject *)0x5) {
                pRVar7 = pRVar7 + 6;
            }
            local_c4 = *(void **)pRVar7;
            local_c0 = pRVar7->parent;
            local_bc = *(void **)(pRVar7 + 1);
            if ((this_00->m_nVehicleType != VEHICLE_TYPE_BOAT) &&
            (this_00->m_nVehicleType != VEHICLE_TYPE_BIKE)) {
                local_c4 = (void *)(-(intptr_t)local_c4); // TODO(port): decomp negated a pointer-held float
            }
            if (this_00->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                local_bc = (void *)(intptr_t)((float)(intptr_t)local_bc - std::abs(0.0f) * 0.001f); // TODO(port): decomp stored float in void*
            }
        }
        else if (this == this_00->m_apPassengers[0] /* TODO(port): decomp cast array to CPed** */) {
            if ((this_00->m_nVehicleType == VEHICLE_TYPE_BIKE) ||
            (this_00->m_nVehicleSubType == VEHICLE_TYPE_QUAD)) {
            LAB_005dfa80:
                pRVar7 = pCVar1[2].GetRwObject();
                local_c4 = pRVar7[7].parent;
                local_c0 = *(void **)(pRVar7 + 8);
                local_bc = pRVar7[8].parent;
            }
            else {
            LAB_005dfa04:
                pRVar7 = pCVar1[2].GetRwObject();
                if (pCVar1[1].GetRwObject() != (RwObject *)0x5) {
                    pRVar7 = pRVar7 + 6;
                }
                local_c4 = *(void **)pRVar7;
                local_c0 = pRVar7->parent;
                local_bc = *(void **)(pRVar7 + 1);
            }
        }
        else {
            if (this != *(CPed **)(this_00->m_apPassengers.data() + 1 /* TODO(port): decomp ptr arithmetic on array */)) {
                if (this == *(CPed **)(this_00->m_apPassengers.data() + 2 /* TODO(port): decomp ptr arithmetic on array */)) goto LAB_005dfa80;
                goto LAB_005dfa04;
            }
            pRVar7 = pCVar1[2].GetRwObject();
            local_c0 = *(void **)(pRVar7 + 8);
            local_c4 = (void *)(-(intptr_t)pRVar7[7].parent); // TODO(port): decomp negated pointer-held float
            local_bc = pRVar7[8].parent;
        }
        if (this_00->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            this_00->CalculateLeanMatrix(); // TODO(port): decomp static-style call
            // TODO(port): decomp copied a CMatrix from m_pVehicle[1] at m_ScanCode offset (artifact); stubbed
            local_60 = CMatrix();
        }
        else if (this_00->m_nModelIndex == 0x214) {
            fVar2 = (this_00->m_RideAnimData).DesiredLeanAngle;
            local_b8 = 0.0;
            if (fVar2 != 0.0) {
                // TODO(port): decomp attached a matrix derived from (int)fVar2+0x10 (artifact); stubbed
            local_a8.Attach((RwMatrix *)nullptr, false);
                local_b8 = local_a8.m_pos.z;
                local_a8.Detach(); // TODO(port): decomp static-style call
            }
            // TODO(port): decomp built a CVector in auVar4 bytes via bitfields
            local_a8.SetTranslate(CVector(-local_b8, 0.0f, 0.0f)); // TODO(port): decomp shift artifact; vector was (-local_b8, 0, ?)
            // TODO(port): decomp passed m_pVehicle[1].m_autoPilot.m_nTimeToStartMission as angle (artifact); stubbed
            local_a8.RotateY(0.0f);
            local_a8.m_pos.z = local_a8.m_pos.z + local_b8;
            // TODO(port): decomp CMatrix::operator*=(&local_a8) lacked the right operand; dropped
        }
        // TODO(port): decomp Multiply3x3(local_18,&local_60,&local_c4); using CVector API
        { CVector mulTmp = local_60.TransformVector(*reinterpret_cast<CVector*>(&local_c4));
          fStack_b4 = mulTmp.x; fStack_b0 = mulTmp.y; fStack_ac = mulTmp.z; }
        local_60.m_pos.x = local_60.m_pos.x + fStack_b4;
        local_60.m_pos.y = local_60.m_pos.y + fStack_b0;
        local_60.m_pos.z = local_60.m_pos.z + fStack_ac;
        local_a8.SetUnity(); // TODO(port): decomp static-style call
        pCVar3 = this->m_pVehicle;
        if (pCVar3->m_pHandlingData->m_nAnimGroup == '\r') {
            if (this == pCVar3->m_apPassengers.data()[1] /* TODO(port): decomp ptr arithmetic */) {
                pCVar6 = pCVar3->m_matrix;
                if (pCVar6 == nullptr) {
                    fVar8 = (long double /* Ghidra float10 */)(pCVar3->m_placement).m_fHeading;
                }
                else {
                    fVar8 = atan2(-(double)((pCVar6->m_forward).x), (double)((pCVar6->m_forward)) /* TODO(port): decomp fpatan */.y);
                }
                this->m_fCurrentRotation = (float)(fVar8 - (long double /* Ghidra float10 */)1.5707964);
                local_a8.SetTranslate(/* TODO(port): decomp static-style */ (CVector)(0.0f, 0.0f, 0.0f) /* TODO(port): decomp ZEXT812 */);
                local_a8.RotateZ(-1.5707964f); // TODO(port): decomp static-style; 0xbfc90fdb = -pi/2
                local_a8.m_pos.y = local_a8.m_pos.y + 0.6;
                // TODO(port): decomp CMatrix::operator*=(&local_a8) lacked the right operand; dropped
                goto LAB_005dfc87;
            }
            pCVar6 = pCVar3->m_matrix;
            if (this == pCVar3->m_apPassengers.data()[2] /* TODO(port): decomp ptr arithmetic */) {
                if (pCVar6 == nullptr) {
                    fVar8 = (long double /* Ghidra float10 */)(pCVar3->m_placement).m_fHeading;
                }
                else {
                    fVar8 = atan2(-(double)((pCVar6->m_forward).x), (double)((pCVar6->m_forward)) /* TODO(port): decomp fpatan */.y);
                }
                this->m_fCurrentRotation = (float)(fVar8 + (long double /* Ghidra float10 */)1.5707964);
                local_a8.SetTranslate(/* TODO(port): decomp static-style */ (CVector)(0.0f, 0.0f, 0.0f) /* TODO(port): decomp ZEXT812 */);
                local_a8.RotateZ(1.5707964f); // TODO(port): decomp static-style call; 0x3fc90fdb = pi/2
                // TODO(port): decomp CMatrix::operator*=(&local_a8) lacked the right operand; dropped
                goto LAB_005dfc87;
            }
        }
        else {
            pCVar6 = pCVar3->m_matrix;
        }
        if (pCVar6 == nullptr) {
            fVar8 = (long double /* Ghidra float10 */)(pCVar3->m_placement).m_fHeading;
        }
        else {
            fVar8 = atan2(-(double)((pCVar6->m_forward).x), (double)((pCVar6->m_forward)) /* TODO(port): decomp fpatan */.y);
        }
        this->m_fCurrentRotation = (float)fVar8;
    LAB_005dfc87:
        this->m_fAimingRotation = this->m_fCurrentRotation;
        this->SetMatrix(local_60); // TODO(port): decomp static-style call
        local_4 = local_4 & 0xffffff00;
        // TODO(port): decomp explicit dtor; automatic in C++
        // TODO(port): decomp explicit dtor; automatic in C++
        return;
}










void CPed::RestoreHeadingRate() {    // converted from decomp src/CPed/*.c
        this->m_fHeadingChangeRate = this->m_pStats->m_fHeadingChangeRate;
        return;
}










void CPed::RestoreHeadingRateCB(CAnimBlendAssociation* association,  void* data) {    // converted from decomp src/CPed/*.c
        *(uint32_t *)((int)data + 0x560) = *(uint32_t *)(*(int *)((int)data + 0x59c) + 0x20);
        return;
}










void CPed::SetRadioStation() {    // converted from decomp src/CPed/*.c
        CBaseModelInfo *pCVar1;
        int iVar2;

        pCVar1 = CModelInfo::ms_modelInfoPtrs[(short)this->m_nModelIndex];
        if ((((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) &&
        (this->m_pVehicle != nullptr)) && (this->m_pVehicle->m_pDriver == this)) {
            iVar2 = CGeneral::GetRandomNumber();
            if (iVar2 < 0x3fff) {
                // TODO(port): decomp set m_vehicleAudio.m_AuSettings.RadioStation (opaque in this port); dropped
                return;
            }
            // TODO(port): decomp set m_vehicleAudio.m_AuSettings.RadioStation (opaque in this port); dropped
        }
        return;
}










void CPed::PositionAttachedPed() {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): goto
    // TODO(convert): uint8_t-slice
        uint16_t uVar1;
        CPhysical *pCVar2;
        float fVar3;
        uint8_t bVar4;
        RwMatrix *matrix; // TODO(port): decomp RwMatrixTag
        float *pfVar5;
        uint32_t uVar6;
        long double /* Ghidra float10 */ fVar7;
        float fVar8;
        float fVar9;
        bool bOwnsMatrix;
        float local_bc;
        float local_b4;
        float local_b0;
        float local_ac;
        CMatrix local_a8;
        uint8_t local_60 [12];
        CMatrix local_54;
        void *local_c;
        uint32_t local_4;

        pCVar2 = this->m_pAttachedTo;
        if (pCVar2 == nullptr) {
            return;
        }
        local_a8.m_pAttachMatrix = nullptr;
        local_a8.m_bOwnsAttachedMatrix = false;
        // TODO(port): dropped decomp bitfield local_a8._69_3_ (unknown CMatrix flag)
        local_54.m_pAttachMatrix = nullptr;
        local_54.m_bOwnsAttachedMatrix = false;
        // TODO(port): dropped decomp bitfield local_54._69_3_
        local_4 = 1;
        if (((pCVar2->m_nModelIndex != 0x220) || (fVar3 = pCVar2[5].m_vecTorque.y, fVar3 == 0.0)) ||
        (-900.0 <= (this->m_vecTurretOffset).z)) {
            if (pCVar2->m_matrix == nullptr) {
                ((CPlaceable *)pCVar2)->AllocateMatrix(); // TODO(port): decomp static-style call
                ((CSimpleTransform *)pCVar2)->UpdateMatrix(pCVar2->m_matrix); // TODO(port): decomp static-style call
            }
            local_a8 = *(CMatrix *)pCVar2->m_matrix; // TODO(port): decomp static-style call
            local_b4 = (this->m_vecTurretOffset).x;
            local_b0 = (this->m_vecTurretOffset).y;
            local_ac = (this->m_vecTurretOffset).z;
        }
        else {
            bOwnsMatrix = false;
            matrix = nullptr; // TODO(port): decomp RwFrameGetLTM(fVar3) where fVar3 was float (artifact); stubbed
            local_a8.Attach(matrix, bOwnsMatrix); // TODO(port): decomp static-style call
            local_a8.Detach(); // TODO(port): decomp static-style call
            local_b4 = (this->m_vecTurretOffset).x;
            local_b0 = (this->m_vecTurretOffset).y;
            local_ac = (this->m_vecTurretOffset).z + 1000.0;
        }
        // TODO(port): decomp Multiply3x3(local_60,&local_a8,&local_b4); using CVector API
        { CVector mulTmp2 = local_a8.TransformVector(*reinterpret_cast<CVector*>(&local_b4));
          local_a8.m_pos.z = local_a8.m_pos.z + mulTmp2.z;
          local_a8.m_pos.y = local_a8.m_pos.y + mulTmp2.y;
          local_a8.m_pos.x = local_a8.m_pos.x + mulTmp2.x; }
        fVar7 = atan2(-(double)(local_a8.m_forward.x), (double)(local_a8.m_forward.y)) /* TODO(port): decomp fpatan */;
        fVar3 = (float)fVar7;
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2))
        goto LAB_005e002e;
        uVar1 = this->m_fTurretAngleA;
        if (uVar1 == 1) {
            local_bc = fVar3 + 1.5707964;
        }
        else if (uVar1 == 2) {
            local_bc = fVar3 + 3.1415927;
        }
        else {
            local_bc = fVar3;
            if (uVar1 == 3) {
                local_bc = fVar3 - 1.5707964;
            }
        }
        fVar8 = CGeneral::LimitRadianAngle(local_bc);
        fVar9 = CGeneral::LimitRadianAngle(this->m_fCurrentRotation);
        this->m_fCurrentRotation = fVar9;
        fVar9 = fVar9 - fVar8;
        if (fVar9 <= 3.1415927) {
            if (fVar9 < -3.1415927) {
                fVar9 = fVar9 + 6.2831855;
            }
        }
        else {
            fVar9 = fVar9 - 6.2831855;
        }
        if (fVar9 <= this->m_fTurretAngleB) {
            if (fVar9 < -this->m_fTurretAngleB) {
                fVar8 = fVar8 - this->m_fTurretAngleB;
                goto LAB_005e0013;
            }
        }
        else {
            fVar8 = fVar8 + this->m_fTurretAngleB;
        LAB_005e0013:
            this->m_fCurrentRotation = fVar8;
        }
        fVar9 = CGeneral::LimitRadianAngle(this->m_fCurrentRotation);
        this->m_fCurrentRotation = fVar9;
    LAB_005e002e:
        local_54.SetRotateZ(this->m_fCurrentRotation - fVar3); // TODO(port): decomp static-style call
        // TODO(port): decomp CMatrix::operator*=(&local_54) lacked the right operand; dropped
        this->SetMatrix(local_a8); // TODO(port): decomp static-style call
        pCVar2 = this->m_pAttachedTo;
        bVar4 = (uint32_t)pCVar2->GetType() & 7;
        if ((bVar4 == 2) || (bVar4 == 4)) {
            (this->m_vecMoveSpeed).x = (pCVar2->m_vecMoveSpeed).x;
            (this->m_vecMoveSpeed).y = (pCVar2->m_vecMoveSpeed).y;
            (this->m_vecMoveSpeed).z = (pCVar2->m_vecMoveSpeed).z;
            (this->m_vecTurnSpeed).x = (pCVar2->m_vecTurnSpeed).x;
            (this->m_vecTurnSpeed).y = (pCVar2->m_vecTurnSpeed).y;
            (this->m_vecTurnSpeed).z = (pCVar2->m_vecTurnSpeed).z;
        }
        uVar6 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar6 = uVar6 | 1;
        this->m_standingOnEntity = nullptr;
        bIsStanding = (char)uVar6;
        bInVehicle = (char)(uVar6 >> 8);
        bFiringWeapon = (char)(uVar6 >> 0x10);
        bNotAllowedToDuck = (char)(uVar6 >> 0x18);
        local_4 = local_4 & 0xffffff00;
        // TODO(port): decomp explicit dtor; automatic in C++
        // TODO(port): decomp explicit dtor; automatic in C++
        return;
}










void CPed::Undress(char* modelName) {    // converted from decomp src/CPed/*.c
        int modelId;

        modelId = (int)(short)this->m_nModelIndex;
        this->DeleteRwObject(); // TODO(port): decomp vtable+0x20 call; likely DeleteRwObject
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            modelId = 0;
        }
        // TODO(port): decomp CStreaming::RequestSpecialModel (not in ported CStreaming); dropped
        CWorld::Remove((CEntity *)this);
        return;
}










void CPed::Dress() {    // converted from decomp src/CPed/*.c
        uint16_t uVar1;

        uVar1 = this->m_nModelIndex;
        this->m_nModelIndex = 0xffff;
        this->SetModelIndex(uVar1); // TODO(port): decomp vtable+0x14 call; likely SetModelIndex
        if (this->m_nPedState != PEDSTATE_DRIVING) {
            this->m_nPedState = PEDSTATE_IDLE;
        }
        CWorld::Add((CEntity *)this);
        this->m_fHeadingChangeRate = this->m_pStats->m_fHeadingChangeRate;
        return;
}










bool CPed::IsAlive() const {    // converted from decomp src/CPed/*.c
        if ((this->m_nPedState != PEDSTATE_DIE) && (this->m_nPedState != PEDSTATE_DEAD)) {
            return true;
        }
        return false;
}










void CPed::UpdateStatEnteringVehicle() {    // converted from decomp src/CPed/*.c
        return;
}










void CPed::UpdateStatLeavingVehicle() {    // converted from decomp src/CPed/*.c
        return;
}










void CPed::GetTransformedBonePosition(RwV3d& inOutPos,  eBoneTagU32 boneId,  bool updateSkinBones) {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        uint32_t uVar1;
        uint32_t uVar2;
        RpHAnimHierarchy *pRVar3;
        int iVar4;
        int iVar5;
        float *pfVar6;
        uint8_t local_c [12];

        uVar2 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        if (updateSkinBones) {
            if ((uVar2 & 0x400) == 0) {
                this->UpdateRpHAnim(); // TODO(port): decomp static-style call
                uVar1 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
                uVar1 = uVar1 | 0x400;
                bDonePositionOutOfCollision = (char)uVar1;
                bKilledByStealth = (char)(uVar1 >> 8);
                bRightArmBlocked = (char)(uVar1 >> 0x10);
                bWaitingForScriptBrainToLoad = (char)(uVar1 >> 0x18);
            }
        }
        else if ((uVar2 & 0x400) == 0) {
            // TODO(port): decomp /* TODO(port): CMatrix::Scale unported */ matrix-vector transform
            { CVector transformed = this->m_matrix->TransformPoint(DAT_008d13a8[boneId]);
              inOutPos.x = transformed.x; inOutPos.y = transformed.y; inOutPos.z = transformed.z;
              return; }
            // TODO(port): decomp inOutPos writes; handled above
        }
        pRVar3 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
        iVar4 = RpHAnimIDGetIndex(pRVar3,boneId);
        RwMatrix* pBoneMatrices = RpHAnimHierarchyGetMatrixArray(pRVar3); // TODO(port): decomp used int iVar5
        RwV3dTransformPoints(&inOutPos,&inOutPos,1,&pBoneMatrices[iVar4]); // TODO(port): decomp int arithmetic
        return;
}










void CPed::ReleaseCoverPoint() {    // converted from decomp src/CPed/*.c
        if (this->m_pCoverPoint != nullptr) {
            this->m_pCoverPoint->ReleaseCoverPointForPed(this); // TODO(port): decomp static-style call
            this->m_pCoverPoint = nullptr;
        }
        return;
}










CTaskSimpleHoldEntity* CPed::GetHoldingTask() {    // converted from decomp src/CPed/*.c
        CTaskSimpleHoldEntity *pCVar1;

        pCVar1 = (CTaskSimpleHoldEntity *)
        this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_HOLD_ENTITY);
        if (pCVar1 == nullptr) {
            pCVar1 = (CTaskSimpleHoldEntity *)
            this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PICKUP_ENTITY);
            if (pCVar1 == nullptr) {
                pCVar1 = (CTaskSimpleHoldEntity *)
                this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PUTDOWN_ENTITY);
            }
        }
        return pCVar1;
}










CEntity* CPed::GetEntityThatThisPedIsHolding() {    // converted from decomp src/CPed/*.c
        CTask *pCVar1;

        pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_HOLD_ENTITY);
        if (pCVar1 == nullptr) {
            pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PICKUP_ENTITY);
            if (pCVar1 == nullptr) {
                pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PUTDOWN_ENTITY);
            }
        }
        if (pCVar1 != nullptr) {
            // TODO(port): decomp returned pCVar1[1].vtable (artifact); needs CTaskSimpleHoldEntity
            return nullptr;
        }
        pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_GO_PICKUP_ENTITY);
        if (pCVar1 != nullptr) {
            // TODO(port): decomp returned (CEntity *)pCVar1[1].m_Parent (artifact); needs task parent
            return nullptr;
        }
        return nullptr;
}










void CPed::DropEntityThatThisPedIsHolding(bool bDeleteHeldEntity) {    // converted from decomp src/CPed/*.c
        CEntity *entity;
        CTaskSimpleHoldEntity *this_00;

        this_00 = (CTaskSimpleHoldEntity *)
        this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_HOLD_ENTITY);
        if (((this_00 != nullptr) ||
        (this_00 = (CTaskSimpleHoldEntity *)
        this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PICKUP_ENTITY),
        this_00 != nullptr)) ||
        (this_00 = (CTaskSimpleHoldEntity *)
        this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PUTDOWN_ENTITY),
        this_00 != nullptr)) {
            entity = this_00->m_pEntityToHold;
            this_00->DropEntity(this,true); // TODO(port): decomp static-style call
            if (((entity != nullptr) && (bDeleteHeldEntity)) &&
            ((((uint32_t)entity->GetType() & 7) != 4 || (*(char *)&entity[5].m_pReferences != '\x02')))
            ) {
                entity->DeleteRwObject(); // TODO(port): decomp vtable+0x20 call
                CWorld::Remove(entity);
                delete entity; // TODO(port): decomp deleting-destructor vtable call
            }
        }
        return;
}










bool CPed::CanThrowEntityThatThisPedIsHolding() {    // converted from decomp src/CPed/*.c
        bool bVar1;
        CTaskSimpleHoldEntity *this_00;

        this_00 = (CTaskSimpleHoldEntity *)
        this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_HOLD_ENTITY);
        if (this_00 == nullptr) {
            this_00 = (CTaskSimpleHoldEntity *)
            this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PICKUP_ENTITY);
            if (this_00 == nullptr) {
                this_00 = (CTaskSimpleHoldEntity *)
                this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_PUTDOWN_ENTITY);
                if (this_00 == nullptr) {
                    return false;
                }
            }
        }
        bVar1 = this_00->CanThrowEntity(); // TODO(port): decomp static-style call
        return bVar1;
}










bool CPed::IsPlayingHandSignal() {    // converted from decomp src/CPed/*.c
        CTask *pCVar1;

        pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_HANDSIGNAL_ANIM);
        return pCVar1 != nullptr;
}










void CPed::StopPlayingHandSignal() {    // converted from decomp src/CPed/*.c
        CTask *pCVar1;

        pCVar1 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_HANDSIGNAL_ANIM);
        if (pCVar1 != nullptr) {
            // TODO(port): decomp vtable+0x18 call on handsigal anim task (unidentified); dropped
        }
        return;
}










float CPed::GetWalkAnimSpeed() {    // converted from decomp src/CPed/*.c
        CAnimBlendHierarchy *hier;
        CAnimBlendSequence *pCVar1;
        void *pvVar2;
        CAnimBlendStaticAssociation *pCVar3;
        int iVar4;

        pCVar3 = CAnimManager::GetAnimAssociation(this->m_nAnimGroup,ANIM_ID_WALK);
        hier = pCVar3->m_BlendHier;
        pCVar1 = hier->m_pSequences;
        CAnimManager::UncompressAnimation(hier);
        if ((short)pCVar1->m_FramesNum < 1) {
            return 0.0;
        }
        pvVar2 = pCVar1->m_Frames;
        iVar4 = (short)pCVar1->m_FramesNum + -1;
        // (decomp: (bits_m_bHasRotation >> 1) & 1; bit 1 of the sequence flags
        //  is m_bHasTranslation per CAnimBlendSequence.h)
        if (pCVar1->m_bHasTranslation) {
            return (*(float *)(iVar4 * 0x20 + 0x18 + (int)pvVar2) - *(float *)((int)pvVar2 + 0x18)) /
            hier->m_fTotalTime;
        }
        return (*(float *)((int)pvVar2 + iVar4 * 0x14 + 0x18) - *(float *)((int)pvVar2 + 0x18)) /
        hier->m_fTotalTime;
}










void CPed::SetPedDefaultDecisionMaker() {    // converted from decomp src/CPed/*.c
        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            if (this->m_nCreatedBy != PED_MISSION) {
                // TODO(port): decomp static-style call
                this->m_pIntelligence->SetPedDecisionMakerType((int)this->m_pStats->m_nDefaultDecisionMaker);
                return;
            }
            this->m_pIntelligence->SetPedDecisionMakerType(-1);
            return;
        }
        this->m_pIntelligence->SetPedDecisionMakerType(-2);
        return;
}










bool CPed::CanSeeEntity(CEntity* entity,  float limitAngle) {    // converted from decomp src/CPed/*.c
        float fVar1;
        CMatrixLink *pCVar2;
        CSimpleTransform *pCVar3;
        CSimpleTransform *pCVar4;
        CSimpleTransform *pCVar5;
        CSimpleTransform *pCVar6;
        float fVar7;

        pCVar2 = this->m_matrix;
        pCVar5 = (CSimpleTransform *)&pCVar2->m_pos;
        if (pCVar2 == nullptr) {
            pCVar5 = &this->m_placement;
            pCVar6 = &this->m_placement;
        }
        else {
            pCVar6 = (CSimpleTransform *)&pCVar2->m_pos;
        }
        pCVar2 = entity->m_matrix;
        pCVar4 = (CSimpleTransform *)&pCVar2->m_pos;
        if (pCVar2 == nullptr) {
            pCVar4 = &entity->m_placement;
            pCVar3 = &entity->m_placement;
        }
        else {
            pCVar3 = (CSimpleTransform *)&pCVar2->m_pos;
        }
        fVar7 = CGeneral::GetAngleBetweenPoints
        ((pCVar3->m_vPosn).x,(pCVar4->m_vPosn).y,(pCVar6->m_vPosn).x,(pCVar5->m_vPosn).y
        );
        fVar7 = fVar7 * 0.017453292;
        if (fVar7 <= 6.2831855) {
            if (fVar7 < 0.0) {
                fVar7 = fVar7 + 6.2831855;
            }
        }
        else {
            fVar7 = fVar7 - 6.2831855;
        }
        fVar1 = this->m_fCurrentRotation;
        if (fVar1 <= 6.2831855) {
            if (fVar1 < 0.0) {
                fVar1 = fVar1 + 6.2831855;
            }
        }
        else {
            fVar1 = fVar1 - 6.2831855;
        }
        fVar7 = fVar7 - fVar1;
        if (fVar7 < 0.0) {
            fVar7 = -fVar7;
        }
        if ((limitAngle <= fVar7) && (fVar7 <= 6.2831855 - limitAngle)) {
            return false;
        }
        return true;
}










bool CPed::PositionPedOutOfCollision(int32_t exitDoor,  CVehicle* vehicle,  bool findClosestNode) {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        float fVar1;
        uint8_t bVar2;
        CMatrixLink *pCVar3;
        float fVar4;
        CPathNode *pCVar5;
        float fVar6;
        int iVar7;
        bool bVar8;
        CColModel *pCVar9;
        CSimpleTransform *pCVar10;
        int doorId;
        CVector *pCVar11;
        CVector *pCVar12;
        float *pfVar13;
        uint32_t uVar14;
        uint32_t uVar15;
        CVehicle *this_00;
        long double /* Ghidra float10 */ fVar16;
        bool bPlaced; // TODO(port): decomp reused the vehicle param slot for a bool flag
        int32_t iNodeIdx; // TODO(port): decomp reused the vehicle param slot for the node index
        // TODO(port): dropped unused decomp stack artifact in_stack_0000000d
        int iStack_5c;
        CVector CStack_54;
        CVector CStack_48;
        float fStack_3c;
        float fStack_38;
        float fStack_34;
        float fStack_30;
        float fStack_2c;
        float fStack_28;
        float fStack_24;
        float fStack_20;
        float fStack_1c;
        uint8_t auStack_18 [4];
        float fStack_14;
        float fStack_10;
        CVector CStack_c;

        this_00 = vehicle;
        if ((vehicle == nullptr) && (this_00 = this->m_pVehicle, this_00 == nullptr)) {
            return false;
        }
        if ((bDonePositionOutOfCollision & 1) != 0) {
            return true;
        }
        bPlaced = false; // TODO(port): decomp cleared the bool in the vehicle param slot
        pCVar9 = ((CEntity *)this_00)->GetColModel() /* TODO(port): decomp static-style */;
        if (this_00->m_matrix == nullptr) {
            pCVar10 = &this_00->m_placement;
        }
        else {
            pCVar10 = (CSimpleTransform *)&this_00->m_matrix->m_pos;
        }
        CStack_48.x = (pCVar10->m_vPosn).x;
        CStack_48.y = (pCVar10->m_vPosn).y;
        CStack_48.z = (pCVar10->m_vPosn).z;
        if (this->m_matrix == nullptr) {
            pCVar10 = &this->m_placement;
        }
        else {
            pCVar10 = (CSimpleTransform *)&this->m_matrix->m_pos;
        }
        CStack_54.x = (pCVar10->m_vPosn).x;
        CStack_54.y = (pCVar10->m_vPosn).y;
        CStack_54.z = (pCVar10->m_vPosn).z;
        bVar2 = GetUsesCollision();
        uVar15 = m_nFlags;
        uVar14 = m_nPhysicalFlags;
        fStack_3c = 0.0;
        CWorld::pIgnoreEntity = (CEntity *)this_00;
        (this->m_vecMoveSpeed).x = 0.0;
        m_nPhysicalFlags = uVar14 | 0x10000;
        fStack_38 = 0.0;
        fStack_34 = 0.0;
        (this->m_vecMoveSpeed).y = 0.0;
        (this->m_vecMoveSpeed).z = 0.0;
        m_nFlags = uVar15 & 0xfffffffe;
        fStack_30 = CStack_54.x;
        fStack_2c = CStack_54.y;
        fStack_28 = CStack_54.z;
        bVar8 = (this_00)->IsOnItsSide() /* TODO(port): decomp static-style */;
        iVar7 = exitDoor;
        if ((bVar8) && (this_00->m_nVehicleType != VEHICLE_TYPE_BIKE)) {
            CStack_54.x = CStack_48.x;
            CStack_54.y = CStack_48.y;
            CStack_54.z = CStack_48.z + (pCVar9->m_boundBox).m_vecMax.x + 1.0;
            CPlaceable::SetPosn(CStack_54) /* TODO(port): decomp passed pointer */;
            bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
        joined_r0x005e0a3c:
            if ((!bVar8) &&
            /* TODO(port): decomp passed pointers */ (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54,true,false,false,true,false,false,false), bVar8)) {
            LAB_005e0a62:
                bPlaced = true; // TODO(port): decomp set the bool in the vehicle param slot
            }
        }
        else if (exitDoor != 0) {
            CStack_c = CCarEnterExit::GetPositionToOpenCarDoor(this_00,exitDoor); pCVar11 = &CStack_c; // TODO(port): decomp used out-param
            CStack_54.x = pCVar11->x;
            CStack_54.y = pCVar11->y;
            CStack_54.z = pCVar11->z;
            CPlaceable::SetPosn(CStack_54) /* TODO(port): decomp passed pointer */;
            bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
            if ((!bVar8) &&
            /* TODO(port): decomp passed pointers */ (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54,true,false,false,true,false,false,false), bVar8))
            goto LAB_005e0a62;
            if ((this_00->m_nVehicleType == VEHICLE_TYPE_BIKE) && ((iVar7 == 10 || (iVar7 == 0xb)))) {
                doorId = 8;
                if (iVar7 == 0xb) {
                    doorId = 9;
                }
                CStack_c = CCarEnterExit::GetPositionToOpenCarDoor(this_00,doorId); pCVar11 = &CStack_c; // TODO(port): decomp used out-param
                CStack_54.x = pCVar11->x;
                CStack_54.y = pCVar11->y;
                CStack_54.z = pCVar11->z;
                CPlaceable::SetPosn(CStack_54) /* TODO(port): decomp passed pointer */;
                bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
                goto joined_r0x005e0a3c;
            }
        }
        fVar4 = -(pCVar9->m_boundBox).m_vecMin.z;
        fVar1 = (pCVar9->m_boundBox).m_vecMax.z;
        if (fVar4 < fVar1) {
            fVar4 = fVar1;
        }
        pCVar3 = this_00->m_matrix;
        exitDoor = (int)(std::abs((pCVar3->m_right).z) * fVar4 + ((pCVar9->m_boundBox).m_vecMin.x - 0.355));
        if ((iVar7 == 8) || (iVar7 == 9)) {
            exitDoor = (int)((pCVar9->m_boundBox).m_vecMax.x + 0.355);
        }
        if (bPlaced) goto LAB_005e10d6;
        fVar1 = (float)exitDoor -
        ((fStack_30 - CStack_48.x) * (pCVar3->m_right).x +
        (fStack_2c - CStack_48.y) * (pCVar3->m_right).y +
        (fStack_28 - CStack_48.z) * (pCVar3->m_right).z);
        fStack_14 = fVar1 * (pCVar3->m_right).y;
        fStack_10 = fVar1 * (pCVar3->m_right).z;
        CStack_54.x = fVar1 * (pCVar3->m_right).x + fStack_30;
        CStack_54.y = fStack_14 + fStack_2c;
        CStack_54.z = fStack_10 + fStack_28;
        fStack_24 = CStack_54.x;
        fStack_20 = CStack_54.y;
        fStack_1c = CStack_54.z;
        CPlaceable::SetPosn(CStack_54) /* TODO(port): decomp passed pointer */;
        bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
        if ((bVar8) ||
        (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false), !bVar8)) {
            fVar4 = (pCVar9->m_boundBox).m_vecMin.y;
            fVar1 = (pCVar9->m_boundBox).m_vecMax.y;
            iStack_5c = 0;
            do {
                pCVar3 = this_00->m_matrix;
                fVar6 = (float)iStack_5c * (fVar1 - fVar4) * 0.33333334 + fVar4;
                fStack_14 = fVar6 * (pCVar3->m_forward).y;
                fStack_10 = fVar6 * (pCVar3->m_forward).z;
                CStack_c.z = (float)exitDoor * (pCVar3->m_right).z;
                fStack_24 = (float)exitDoor * (pCVar3->m_right).x + CStack_48.x;
                fStack_20 = (float)exitDoor * (pCVar3->m_right).y + CStack_48.y;
                fStack_1c = CStack_c.z + CStack_48.z;
                CStack_54.x = fStack_24 + fVar6 * (pCVar3->m_forward).x;
                CStack_54.y = fStack_20 + fStack_14;
                CStack_54.z = fStack_1c + fStack_10;
                if (this->m_matrix == nullptr) {
                    (this->m_placement).m_vPosn.x = CStack_54.x;
                    (this->m_placement).m_vPosn.y = CStack_54.y;
                    (this->m_placement).m_vPosn.z = CStack_54.z;
                }
                else {
                    (this->m_matrix->m_pos).x = CStack_54.x;
                    (this->m_matrix->m_pos).y = CStack_54.y;
                    (this->m_matrix->m_pos).z = CStack_54.z;
                }
                fStack_30 = CStack_54.x;
                fStack_2c = CStack_54.y;
                fStack_28 = CStack_54.z;
                bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
                if ((!bVar8) &&
                (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false), bVar8))
                goto LAB_005e10d1;
                iStack_5c = iStack_5c + 1;
            } while (iStack_5c < 4);
            pCVar3 = this_00->m_matrix;
            fVar1 = (pCVar9->m_boundBox).m_vecMin.y - 0.355;
            fStack_14 = fVar1 * (pCVar3->m_forward).y;
            fStack_10 = fVar1 * (pCVar3->m_forward).z;
            CStack_54.x = fVar1 * (pCVar3->m_forward).x + CStack_48.x;
            CStack_54.y = fStack_14 + CStack_48.y;
            CStack_54.z = fStack_10 + CStack_48.z;
            if (this->m_matrix == nullptr) {
                (this->m_placement).m_vPosn.x = CStack_54.x;
                (this->m_placement).m_vPosn.y = CStack_54.y;
                (this->m_placement).m_vPosn.z = CStack_54.z;
            }
            else {
                (this->m_matrix->m_pos).x = CStack_54.x;
                (this->m_matrix->m_pos).y = CStack_54.y;
                (this->m_matrix->m_pos).z = CStack_54.z;
            }
            fStack_30 = CStack_54.x;
            fStack_2c = CStack_54.y;
            fStack_28 = CStack_54.z;
            bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
            if ((bVar8) ||
            (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false), !bVar8))
            {
                pCVar3 = this_00->m_matrix;
                fVar1 = (pCVar9->m_boundBox).m_vecMax.y + 0.355;
                fStack_14 = fVar1 * (pCVar3->m_forward).y;
                fStack_10 = fVar1 * (pCVar3->m_forward).z;
                CStack_54.x = fVar1 * (pCVar3->m_forward).x + CStack_48.x;
                CStack_54.y = fStack_14 + CStack_48.y;
                CStack_54.z = fStack_10 + CStack_48.z;
                if (this->m_matrix == nullptr) {
                    (this->m_placement).m_vPosn.x = CStack_54.x;
                    (this->m_placement).m_vPosn.y = CStack_54.y;
                    (this->m_placement).m_vPosn.z = CStack_54.z;
                }
                else {
                    (this->m_matrix->m_pos).x = CStack_54.x;
                    (this->m_matrix->m_pos).y = CStack_54.y;
                    (this->m_matrix->m_pos).z = CStack_54.z;
                }
                fStack_30 = CStack_54.x;
                fStack_2c = CStack_54.y;
                fStack_28 = CStack_54.z;
                bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
                if ((bVar8) ||
                (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false), !bVar8)
                ) {
                    pCVar3 = this_00->m_matrix;
                    fVar1 = (pCVar9->m_boundBox).m_vecMin.y;
                    fStack_14 = fVar1 * (pCVar3->m_forward).y;
                    fStack_10 = fVar1 * (pCVar3->m_forward).z;
                    CStack_c.z = (float)exitDoor * (pCVar3->m_right).z;
                    fStack_24 = CStack_48.x - (float)exitDoor * (pCVar3->m_right).x;
                    fStack_20 = CStack_48.y - (float)exitDoor * (pCVar3->m_right).y;
                    fStack_1c = CStack_48.z - CStack_c.z;
                    CStack_54.x = fStack_24 + fVar1 * (pCVar3->m_forward).x;
                    CStack_54.y = fStack_20 + fStack_14;
                    CStack_54.z = fStack_1c + fStack_10;
                    if (this->m_matrix == nullptr) {
                        (this->m_placement).m_vPosn.x = CStack_54.x;
                        (this->m_placement).m_vPosn.y = CStack_54.y;
                        (this->m_placement).m_vPosn.z = CStack_54.z;
                    }
                    else {
                        (this->m_matrix->m_pos).x = CStack_54.x;
                        (this->m_matrix->m_pos).y = CStack_54.y;
                        (this->m_matrix->m_pos).z = CStack_54.z;
                    }
                    fStack_30 = CStack_54.x;
                    fStack_2c = CStack_54.y;
                    fStack_28 = CStack_54.z;
                    bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
                    if ((bVar8) ||
                    (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false),
                    !bVar8)) {
                        pCVar3 = this_00->m_matrix;
                        fVar1 = (pCVar9->m_boundBox).m_vecMax.y;
                        fStack_14 = fVar1 * (pCVar3->m_forward).y;
                        fStack_10 = fVar1 * (pCVar3->m_forward).z;
                        CStack_c.z = (float)exitDoor * (pCVar3->m_right).z;
                        fStack_24 = CStack_48.x - (float)exitDoor * (pCVar3->m_right).x;
                        fStack_20 = CStack_48.y - (float)exitDoor * (pCVar3->m_right).y;
                        fStack_1c = CStack_48.z - CStack_c.z;
                        CStack_54.x = fStack_24 + fVar1 * (pCVar3->m_forward).x;
                        CStack_54.y = fStack_20 + fStack_14;
                        CStack_54.z = fStack_1c + fStack_10;
                        if (this->m_matrix == nullptr) {
                            (this->m_placement).m_vPosn.x = CStack_54.x;
                            (this->m_placement).m_vPosn.y = CStack_54.y;
                            (this->m_placement).m_vPosn.z = CStack_54.z;
                        }
                        else {
                            (this->m_matrix->m_pos).x = CStack_54.x;
                            (this->m_matrix->m_pos).y = CStack_54.y;
                            (this->m_matrix->m_pos).z = CStack_54.z;
                        }
                        fStack_30 = CStack_54.x;
                        fStack_2c = CStack_54.y;
                        fStack_28 = CStack_54.z;
                        bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
                        if ((bVar8) ||
                        (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false),
                        !bVar8)) {
                            if (this_00->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE) goto LAB_005e10d6;
                            pCVar3 = this_00->m_matrix;
                            fVar1 = (pCVar9->m_boundBox).m_vecMax.z;
                            fStack_14 = fVar1 * (pCVar3->m_up).y;
                            fStack_10 = fVar1 * (pCVar3->m_up).z;
                            CStack_54.x = fVar1 * (pCVar3->m_up).x + CStack_48.x;
                            CStack_54.y = fStack_14 + CStack_48.y;
                            fStack_28 = fStack_10 + CStack_48.z;
                            CStack_54.z = fStack_28 + 1.0;
                            if (this->m_matrix == nullptr) {
                                (this->m_placement).m_vPosn.x = CStack_54.x;
                                (this->m_placement).m_vPosn.y = CStack_54.y;
                                (this->m_placement).m_vPosn.z = CStack_54.z;
                            }
                            else {
                                (this->m_matrix->m_pos).x = CStack_54.x;
                                (this->m_matrix->m_pos).y = CStack_54.y;
                                (this->m_matrix->m_pos).z = CStack_54.z;
                            }
                            fStack_30 = CStack_54.x;
                            fStack_2c = CStack_54.y;
                            bVar8 = ((CPhysical *)this)->CheckCollision(); /* TODO(port): decomp static-style */
                            if ((bVar8) ||
                            (bVar8 = CWorld::GetIsLineOfSightClear(CStack_48,CStack_54, /* TODO(port): decomp passed pointers */true,false,false,true,false,false,false),
                            !bVar8)) goto LAB_005e10d6;
                        }
                    }
                }
            }
        }
    LAB_005e10d1:
        bPlaced = true; // TODO(port): decomp set the bool in the vehicle param slot
    LAB_005e10d6:
        CWorld::pIgnoreEntity = nullptr;
        m_nPhysicalFlags = m_nPhysicalFlags & 0xfffeffff;
        m_nFlags =
        m_nFlags ^ ((uint32_t)(bVar2 & 1) ^ m_nFlags) & 1;
        if (bPlaced) {
            uVar15 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
            (this->m_vecMoveSpeed).x = fStack_3c;
            (this->m_vecMoveSpeed).y = fStack_38;
            uVar15 = uVar15 | 1;
            bDonePositionOutOfCollision = (char)uVar15;
            bKilledByStealth = (char)(uVar15 >> 8);
            bRightArmBlocked = (char)(uVar15 >> 0x10);
            bWaitingForScriptBrainToLoad = (char)(uVar15 >> 0x18);
            (this->m_vecMoveSpeed).z = fStack_34;
            (this->m_vecTurnSpeed).x = fStack_3c;
            (this->m_vecTurnSpeed).y = fStack_38;
            (this->m_vecTurnSpeed).z = fStack_34;
            if ((this_00->m_nVehicleType != VEHICLE_TYPE_BIKE) || (findClosestNode != false)) {
                (this_00->m_vecMoveSpeed).x = fStack_3c;
                (this_00->m_vecMoveSpeed).y = fStack_38;
                (this_00->m_vecMoveSpeed).z = fStack_34;
                fVar1 = (this_00->m_vecMoveSpeed).z;
                (this_00->m_vecTurnSpeed).x = fStack_3c;
                (this_00->m_vecTurnSpeed).y = fStack_38;
                (this_00->m_vecMoveSpeed).z = fVar1 - 0.05;
                (this_00->m_vecTurnSpeed).z = fStack_34;
            }
            return true;
        }
        if (findClosestNode == false) {
            return false;
        }
        CPathFind::FindNodeClosestToCoors
        (&iNodeIdx,CStack_48.x,CStack_48.y,CStack_48.z,1,0x497423fe,0,0,0,0,0);
        CPathFind::FindNodeClosestToCoors
        (&exitDoor,CStack_48.x,CStack_48.y,CStack_48.z,0,0x497423fe,0,0,0,0,0);
        bVar8 = (short)iNodeIdx == -1;
        if (!bVar8) {
            pCVar11 = CPathNode::GetPosition(ThePaths.m_pPathNodes[(uint32_t)iNodeIdx & 0xffff] + ((uint32_t)iNodeIdx >> 0x10), &CStack_c); // TODO(port): decomp static-style
            CStack_54.x = pCVar11->x;
            CStack_54.y = pCVar11->y;
            CStack_54.z = pCVar11->z;
        }
        if ((short)exitDoor == -1) {
            if (bVar8) {
                return false;
            }
        }
        else {
            uVar15 = (uint32_t)exitDoor >> 0x10;
            pCVar5 = ThePaths.m_pPathNodes[exitDoor & 0xffff];
            pCVar11 = &CStack_48;
            pCVar12 = CPathNode::GetPosition(pCVar5 + uVar15,&CStack_c);
            pfVar13 = (float *)VectorSub(auStack_18,pCVar12,pCVar11); // TODO(port): VectorSub defined in file-local helpers
            if (std::sqrt(pfVar13[1] * pfVar13[1] + *pfVar13 * *pfVar13) <
            std::sqrt((CStack_54.x - CStack_48.x) * (CStack_54.x - CStack_48.x) +
            (CStack_54.y - CStack_48.y) * (CStack_54.y - CStack_48.y))) {
                pCVar11 = CPathNode::GetPosition(pCVar5 + uVar15,&CStack_c);
                CStack_54.x = pCVar11->x;
                CStack_54.y = pCVar11->y;
                CStack_54.z = pCVar11->z;
            }
        }
        CPedPlacement::FindZCoorForPed(&CStack_54);
        CPlaceable::SetPosn(CStack_54) /* TODO(port): decomp passed pointer */;
        pCVar3 = this_00->m_matrix;
        float _findClosestNode; // TODO(port): decomp stack local
        if (pCVar3 == nullptr) {
            _findClosestNode = (this_00->m_placement).m_fHeading;
        }
        else {
            fVar16 = atan2(-(double)((pCVar3->m_forward).x), (double)((pCVar3->m_forward)) /* TODO(port): decomp fpatan */.y);
            _findClosestNode = (float)fVar16;
        }
        if (this->m_matrix == nullptr) {
            (this->m_placement).m_fHeading = _findClosestNode;
        }
        else {
            ((CMatrix*)this->m_matrix)->SetRotateZOnly(_findClosestNode); // TODO(port): was static-style call
        }
        uVar14 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        uVar14 = uVar14 | 1;
        bDonePositionOutOfCollision = (char)uVar14;
        bKilledByStealth = (char)(uVar14 >> 8);
        bRightArmBlocked = (char)(uVar14 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar14 >> 0x18);
        (this->m_vecMoveSpeed).x = fStack_3c;
        (this->m_vecMoveSpeed).y = fStack_38;
        (this->m_vecMoveSpeed).z = fStack_34;
        (this->m_vecTurnSpeed).x = fStack_3c;
        (this->m_vecTurnSpeed).y = fStack_38;
        (this->m_vecTurnSpeed).z = fStack_34;
        (this_00->m_vecTurnSpeed).x = fStack_3c;
        (this_00->m_vecTurnSpeed).y = fStack_38;
        (this_00->m_vecTurnSpeed).z = fStack_34;
        (this_00->m_vecMoveSpeed).x = fStack_3c;
        (this_00->m_vecMoveSpeed).y = fStack_38;
        (this_00->m_vecMoveSpeed).z = fStack_34;
        (this_00->m_vecMoveSpeed).z = (this_00->m_vecMoveSpeed).z + 0.02;
        return true;
}










bool CPed::PositionAnyPedOutOfCollision() {    // converted from decomp src/CPed/*.c
        CMatrixLink *pCVar1;
        float fVar2;
        float fVar3;
        float fVar4;
        CVector sphereCenter;
        CVector sphereCenter_00;
        bool bVar5;
        bool bVar6;
        CEntity *pCVar7;
        CSimpleTransform *pCVar8;
        CEntity *this_00;
        CColModel *pCVar9;
        int iVar10;
        int local_30;
        int local_2c;
        int local_28;
        float local_24;
        float local_20;
        float local_1c;
        float local_18;
        float local_14;
        float local_10;
        float local_c;
        float local_8;
        float local_4;

        local_30 = 999;
        local_2c = 999;
        pCVar1 = this->m_matrix;
        bVar5 = false;
        bVar6 = false;
        pCVar8 = (CSimpleTransform *)&pCVar1->m_pos;
        if (pCVar1 == nullptr) {
            pCVar8 = &this->m_placement;
        }
        local_20 = (pCVar8->m_vPosn).y - 3.5;
        if (pCVar1 == nullptr) {
            pCVar8 = &this->m_placement;
        }
        else {
            pCVar8 = (CSimpleTransform *)&pCVar1->m_pos;
        }
        local_1c = (pCVar8->m_vPosn).z;
        local_28 = 0xf;
        this_00 = (CEntity *)0xf;
        do {
            if (this->m_matrix == nullptr) {
                pCVar8 = &this->m_placement;
            }
            else {
                pCVar8 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            iVar10 = 0xf;
            local_24 = (pCVar8->m_vPosn).x - 3.5;
            do {
                CPedPlacement::FindZCoorForPed(&local_24);
                sphereCenter.y = local_20;
                sphereCenter.x = local_24;
                sphereCenter.z = local_1c;
                pCVar7 = CWorld::TestSphereAgainstWorld
                (sphereCenter,0.6,(CEntity *)this,true,false,false,true,false,false);
                if (pCVar7 == nullptr) {
                    if (this->m_matrix == nullptr) {
                        pCVar8 = &this->m_placement;
                    }
                    else {
                        pCVar8 = (CSimpleTransform *)&this->m_matrix->m_pos;
                    }
                    fVar2 = local_24 - (pCVar8->m_vPosn).x;
                    fVar4 = local_20 - (pCVar8->m_vPosn).y;
                    fVar3 = local_1c - (pCVar8->m_vPosn).z;
                    fVar2 = fVar2 * fVar2 + fVar4 * fVar4 + fVar3 * fVar3;
                    sphereCenter_00.y = local_20;
                    sphereCenter_00.x = local_24;
                    sphereCenter_00.z = local_1c;
                    this_00 = CWorld::TestSphereAgainstWorld
                    (sphereCenter_00,0.6,(CEntity *)this,false,true,false,false,false,false)
                    ;
                    if (this_00 == nullptr) {
                        if (fVar2 < (float)local_30) {
                            local_30 = CGeneral::GetRandomNumber();
                            local_18 = local_24;
                            local_14 = local_20;
                            local_10 = local_1c;
                            bVar5 = true;
                        }
                    }
                    else if (fVar2 < (float)local_2c) {
                        local_2c = CGeneral::GetRandomNumber();
                        local_c = local_24;
                        local_8 = local_20;
                        local_4 = local_1c;
                        bVar6 = true;
                    }
                }
                iVar10 = iVar10 + -1;
                local_24 = local_24 + 0.5;
            } while (iVar10 != 0);
            local_20 = local_20 + 0.5;
            local_28 = local_28 + -1;
        } while (local_28 != 0);
        if (bVar5) {
            if (this->m_matrix != nullptr) {
                (this->m_matrix->m_pos).x = local_18;
                (this->m_matrix->m_pos).y = local_14;
                (this->m_matrix->m_pos).z = local_10;
                return true;
            }
            (this->m_placement).m_vPosn.x = local_18;
            (this->m_placement).m_vPosn.y = local_14;
            (this->m_placement).m_vPosn.z = local_10;
            return true;
        }
        if (!bVar6) {
            return false;
        }
        pCVar9 = this_00->GetColModel(); // TODO(port): decomp static-style
        local_4 = local_4 + (pCVar9->m_boundBox).m_vecMax.z;
        if (this->m_matrix != nullptr) {
            (this->m_matrix->m_pos).x = local_c;
            (this->m_matrix->m_pos).y = local_8;
            (this->m_matrix->m_pos).z = local_4;
            return true;
        }
        (this->m_placement).m_vPosn.z = local_4;
        (this->m_placement).m_vPosn.y = local_8;
        (this->m_placement).m_vPosn.x = local_c;
        return true;
}










bool CPed::OurPedCanSeeThisEntity(CEntity* entity,  bool isSpotted) {    // converted from decomp src/CPed/*.c
        CMatrixLink *pCVar1;
        CMatrixLink *pCVar2;
        float fVar3;
        float fVar4;
        bool bVar5;
        CSimpleTransform *pCVar6;
        CSimpleTransform *pCVar7;
        CVector local_44;
        CVector local_38;
        CColPoint local_2c;
        CEntity *pHitEntity = nullptr; // TODO(port): decomp passed &(bool)isSpotted as the hit-entity out-param

        pCVar1 = entity->m_matrix;
        pCVar7 = (CSimpleTransform *)&pCVar1->m_pos;
        if (pCVar1 == nullptr) {
            pCVar7 = &entity->m_placement;
        }
        pCVar2 = this->m_matrix;
        pCVar6 = (CSimpleTransform *)&pCVar2->m_pos;
        if (pCVar2 == nullptr) {
            pCVar6 = &this->m_placement;
        }
        fVar3 = (pCVar7->m_vPosn).x - (pCVar6->m_vPosn).x;
        pCVar7 = (CSimpleTransform *)&pCVar1->m_pos;
        if (pCVar1 == nullptr) {
            pCVar7 = &entity->m_placement;
        }
        pCVar6 = (CSimpleTransform *)&pCVar2->m_pos;
        if (pCVar2 == nullptr) {
            pCVar6 = &this->m_placement;
        }
        fVar4 = (pCVar7->m_vPosn).y - (pCVar6->m_vPosn).y;
        if ((isSpotted) ||
        ((0.0 <= fVar3 * (pCVar2->m_forward).x + fVar4 * (pCVar2->m_forward).y &&
        (std::sqrt(fVar3 * fVar3 + fVar4 * fVar4) < 40.0)))) {
            pCVar7 = (CSimpleTransform *)&pCVar2->m_pos;
            if (pCVar2 == nullptr) {
                pCVar7 = &this->m_placement;
            }
            local_38.x = (pCVar7->m_vPosn).x;
            local_38.y = (pCVar7->m_vPosn).y;
            local_38.z = (pCVar7->m_vPosn).z + 1.0;
            pCVar7 = (CSimpleTransform *)&pCVar1->m_pos;
            if (pCVar1 == nullptr) {
                pCVar7 = &entity->m_placement;
            }
            local_44.x = (pCVar7->m_vPosn).x;
            local_44.y = (pCVar7->m_vPosn).y;
            local_44.z = (pCVar7->m_vPosn).z;
            if (((uint32_t)entity->GetType() & 7) == 3) {
                local_44.z = local_44.z + 1.0;
            }
            if (isSpotted) {
                bVar5 = CWorld::ProcessLineOfSight(local_38,local_44,local_2c,pHitEntity,true,false,false,true,
                false,false,false,true);
            }
            else {
                bVar5 = CWorld::ProcessLineOfSight(local_38,local_44,local_2c,pHitEntity,true,false,false,false
                ,false,false,false,false);
            }
            if (!bVar5) {
                return true;
            }
        }
        return false;
}










void CPed::SortPeds(CPed** pedList,  int32_t arg1,  int32_t arg2) {    // converted from decomp src/CPed/*.c
        int iVar1;
        int iVar2;
        int iVar3;
        uint32_t uVar4;
        float fVar5;
        float *pfVar6;
        float *pfVar7;
        int iVar8;
        int iVar9;

        do {
            if (arg2 <= arg1) {
                return;
            }
            iVar8 = *(int *)(pedList + ((arg1 + arg2) / 2) * 4);
            iVar9 = *(int *)(iVar8 + 0x14);
            if (iVar9 == 0) {
                pfVar7 = (float *)(iVar8 + 4);
            }
            else {
                pfVar7 = (float *)(iVar9 + 0x30);
            }
            if (*(int *)(this + 0x14) == 0) {
                pfVar6 = (float *)(this + 4);
            }
            else {
                pfVar6 = (float *)(*(int *)(this + 0x14) + 0x30);
            }
            fVar5 = std::sqrt((*pfVar6 - *pfVar7) * (*pfVar6 - *pfVar7) +
            (pfVar6[1] - pfVar7[1]) * (pfVar6[1] - pfVar7[1]) +
            (pfVar6[2] - pfVar7[2]) * (pfVar6[2] - pfVar7[2]));
            iVar8 = arg2;
            iVar9 = arg1;
            do {
                iVar1 = *(int *)(this + 0x14);
                while( true ) {
                    iVar2 = *(int *)(pedList + iVar9 * 4);
                    iVar3 = *(int *)(iVar2 + 0x14);
                    if (iVar3 == 0) {
                        pfVar7 = (float *)(iVar2 + 4);
                    }
                    else {
                        pfVar7 = (float *)(iVar3 + 0x30);
                    }
                    pfVar6 = (float *)(iVar1 + 0x30);
                    if (iVar1 == 0) {
                        pfVar6 = (float *)(this + 4);
                    }
                    if (fVar5 <= std::sqrt((*pfVar6 - *pfVar7) * (*pfVar6 - *pfVar7) +
                    (pfVar6[1] - pfVar7[1]) * (pfVar6[1] - pfVar7[1]) +
                    (pfVar6[2] - pfVar7[2]) * (pfVar6[2] - pfVar7[2]))) break;
                    iVar9 = iVar9 + 1;
                }
                while( true ) {
                    iVar2 = *(int *)(pedList + iVar8 * 4);
                    iVar3 = *(int *)(iVar2 + 0x14);
                    if (iVar3 == 0) {
                        pfVar7 = (float *)(iVar2 + 4);
                    }
                    else {
                        pfVar7 = (float *)(iVar3 + 0x30);
                    }
                    pfVar6 = (float *)(iVar1 + 0x30);
                    if (iVar1 == 0) {
                        pfVar6 = (float *)(this + 4);
                    }
                    if (std::sqrt((*pfVar6 - *pfVar7) * (*pfVar6 - *pfVar7) +
                    (pfVar6[1] - pfVar7[1]) * (pfVar6[1] - pfVar7[1]) +
                    (pfVar6[2] - pfVar7[2]) * (pfVar6[2] - pfVar7[2])) <= fVar5) break;
                    iVar8 = iVar8 + -1;
                }
                if (iVar8 < iVar9) break;
                uVar4 = *(uint32_t *)(pedList + iVar9 * 4);
                *(uint32_t *)(pedList + iVar9 * 4) = *(uint32_t *)(pedList + iVar8 * 4);
                iVar9 = iVar9 + 1;
                *(uint32_t *)(pedList + iVar8 * 4) = uVar4;
                iVar8 = iVar8 + -1;
            } while (iVar9 <= iVar8);
            SortPeds(pedList,arg1,iVar8);
            arg1 = iVar9;
        } while( true );
}










void CPed::ClearLookFlag() {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        uint8_t bVar2;
        uint32_t uVar3;
        ePedState eVar4;
        uint32_t uVar5;

        bVar2 = bIsStanding;
        while( true ) {
            if ((bVar2 & 4) == 0) {
                return;
            }
            uVar5 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar3 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            uVar5 = uVar5 & 0xfffff7ff;
            bResetWalkAnims = (char)uVar5;
            bCollidedWithMyVehicle = (char)(uVar5 >> 8);
            bMiamiViceCop = (char)(uVar5 >> 0x10);
            bDontFight = (char)(uVar5 >> 0x18);
            eVar4 = this->m_nPedState;
            uVar5 = uVar3 & 0xfffffffb | 8;
            bIsStanding = (char)uVar5;
            bInVehicle = (char)(uVar5 >> 8);
            bFiringWeapon = (char)(uVar5 >> 0x10);
            bNotAllowedToDuck = (char)(uVar5 >> 0x18);
            if (((eVar4 != PEDSTATE_DRIVING) && (eVar4 != PEDSTATE_DRAGGED_FROM_CAR)) &&
            ((uVar3 & 0x4000000) == 0)) {
                // TODO(port): CPedIK opaque; original clears m_pedIK.m_nFlags bit 1. Dropped.
            }
            if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
                this->m_nLookTime = CTimer::m_snTimeInMilliseconds + 2000;
            }
            else {
                this->m_nLookTime = CTimer::m_snTimeInMilliseconds + 4000;
            }
            if ((this->m_nPedState != PEDSTATE_LOOK_HEADING) && (this->m_nPedState != PEDSTATE_LOOK_ENTITY))
            break;
            bVar2 = bIsStanding;
        }
        return;
}










float CPed::WorkOutHeadingForMovingFirstPerson(float heading) {    // converted from decomp src/CPed/*.c
        CPlayerPedData *pCVar1;
        short sVar2;
        CPad *pCVar3;
        float fVar4;

        if (((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) &&
        (this->m_pPlayerData != nullptr)) {
            pCVar3 = (CPad *)CPad::GetPad(0);
            sVar2 = pCVar3->GetPedWalkLeftRight() /* TODO(port): decomp static-style */;
            fVar4 = (float)(int)sVar2;
            pCVar3 = (CPad *)CPad::GetPad(0);
            sVar2 = pCVar3->GetPedWalkUpDown() /* TODO(port): decomp static-style */;
            if ((float)(int)sVar2 == 0.0) {
                if (fVar4 <= 0.0) {
                    if (fVar4 < 0.0) {
                        this->m_pPlayerData->m_fFPSMoveHeading = 1.5707964;
                    }
                }
                else {
                    this->m_pPlayerData->m_fFPSMoveHeading = -1.5707964;
                }
            }
            else {
                pCVar1 = this->m_pPlayerData;
                fVar4 = CGeneral::GetRadianAngleBetweenPoints(0.0,0.0,-fVar4,(float)(int)sVar2);
                pCVar1->m_fFPSMoveHeading = fVar4;
            }
            fVar4 = CGeneral::LimitRadianAngle(heading + this->m_pPlayerData->m_fFPSMoveHeading);
            return fVar4;
        }
        return 0.0;
}










void CPed::UpdatePosition() {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        int *piVar1;
        CPhysical *this_00;
        float fVar2;
        float fVar3;
        CEntity *pCVar4;
        RwObject *pRVar5;
        uint32_t uVar6;
        float fVar7;
        bool bVar8;
        CTaskSimpleSwim *pCVar9;
        CTaskSimpleJetPack *pCVar10;
        int iVar11;
        CSimpleTransform *pCVar12;
        CVector *pCVar13;
        float *pfVar14;
        uint32_t uVar15;
        CVector *pCVar16;
        long double /* Ghidra float10 */ fVar17;
        float local_48;
        float local_44;
        CVector local_3c;
        CVector local_30;
        uint8_t local_24 [12];
        uint8_t local_18 [12];
        CVector local_c;

        if (CReplay::Mode == CReplay::MODE_PLAYBACK /* TODO(port): decomp used bare MODE_PLAYBACK */) {
            return;
        }
        if ((bIsStanding & 1) == 0) {
            pCVar9 = m_pIntelligence->GetTaskSwim();
            if ((pCVar9 == nullptr) &&
            (pCVar10 = this->m_pIntelligence->GetTaskJetPack(),
            pCVar10 == nullptr)) {
                piVar1 = *(int **)((this->m_pIntelligence->m_TaskMgr).m_aPrimaryTasks + 0xc);
                if (piVar1 == nullptr) {
                    return;
                }
                iVar11 = 0; // TODO(port): decomp vtable+0x10 call (unidentified)
                if (iVar11 != 0x6c) {
                    return;
                }
            }
            if (this->m_matrix != nullptr) {
                ((CMatrix*)this->m_matrix)->SetRotateZOnly(this->m_fCurrentRotation); // TODO(port): was static-style call
                return;
            }
            (this->m_placement).m_fHeading = this->m_fCurrentRotation;
            return;
        }
        if (this->m_pAttachedTo != nullptr) {
            return;
        }
        if (this->m_matrix == nullptr) {
            (this->m_placement).m_fHeading = this->m_fCurrentRotation;
        }
        else {
            ((CMatrix*)this->m_matrix)->SetRotateZOnly(this->m_fCurrentRotation); // TODO(port): was static-style call
        }
        this_00 = (CPhysical *)this->m_standingOnEntity;
        if (this_00 == nullptr) {
            bVar8 = g_surfaceInfos.IsSteepSlope((uint32_t)(uint8_t)this->m_nContactSurface);
            if ((bVar8) && (((this->field_578).x != 0.0 || ((this->field_578).y != 0.0)))) {
                fVar3 = (this->field_578).x;
                fVar2 = (this->field_578).y;
                // TODO(port): CPathFind::unk_0044e480 unidentified decomp function; dropped
                (this->m_vecMoveSpeed).x = 0.0;
                (this->m_vecMoveSpeed).y = 0.0;
                (this->m_vecMoveSpeed).z = -0.001;
                local_3c.x = fVar3 * 0.02 + (this->m_vecAnimMovingShift).x;
                local_3c.y = fVar2 * 0.02 + (this->m_vecAnimMovingShift).y;
                fVar7 = fVar3 * local_3c.x + fVar2 * local_3c.y;
                if (fVar7 < 0.0) {
                    local_3c.x = local_3c.x - fVar3 * fVar7;
                    local_3c.y = local_3c.y - fVar7 * fVar2;
                }
            }
            else {
                local_3c.x = (this->m_vecAnimMovingShift).x - (this->m_vecMoveSpeed).x;
                local_3c.y = (this->m_vecAnimMovingShift).y - (this->m_vecMoveSpeed).y;
            }
        }
        else {
            if ((((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) ||
            (((uint32_t)this_00->GetType() & 7) != 2)) ||
            (this_00[4].m_pCollisionList.m_node != (CEntryInfoNode *)0x5)) {
                pCVar16 = &local_c; // TODO(port): CPhysical::GetSpeed signature mismatch (decomp 2-arg static-style)
                local_48 = pCVar16->x;
                local_44 = pCVar16->y;
            }
            else {
                if (this->m_matrix == nullptr) {
                    pCVar12 = &this->m_placement;
                }
                else {
                    pCVar12 = (CSimpleTransform *)&this->m_matrix->m_pos;
                }
                local_3c.x = (pCVar12->m_vPosn).x;
                local_3c.y = (pCVar12->m_vPosn).y;
                local_3c.z = (pCVar12->m_vPosn).z - 1.0;
                if (this_00->m_matrix == nullptr) {
                    pCVar12 = &this_00->m_placement;
                }
                else {
                    pCVar12 = (CSimpleTransform *)&this_00->m_matrix->m_pos;
                }
                // TODO(decomp): Ghidra rendered `CVector::unk_00406d70(pCVar12)`
                //   (0x406D70 = CVector::operator-=, body verified from decomp).
                //   The this-pointer attribution is suspect; physically local_3c
                //   must become the platform-relative vector for the
                //   CrossProduct below (tangential velocity = w x r).
                local_3c -= pCVar12->m_vPosn;
                pCVar16 = &this_00->m_vecMoveSpeed;
                local_30 = CrossProduct(this_00->m_vecTurnSpeed, local_3c); pCVar13 = &local_30; // TODO(port): decomp used out-param
                // (decomp: CVector::unk_0040fe30 @ 0x40FE30, body verified:
                //  out[i] = a[i] + b[i] - plain vector add.)
                *(CVector*)local_24 = *pCVar13 + *pCVar16;
                pfVar14 = (float*)local_24;
                local_48 = *pfVar14;
                local_44 = pfVar14[1];
                fVar2 = pfVar14[2];
                // TODO(decomp): 0x406DA0 body is empty in the decomp export;
                //   Magnitude() is the contextual inference (vector -> scalar).
                fVar17 = (long double /* Ghidra float10 */)(local_3c.Magnitude() * (double)CTimer::ms_fTimeStep);
                // TODO(decomp): Ghidra dropped arguments at 0x40FE90/0x40FEC0/
                //   0x411A00 (CVector scalar-mul, scalar-mul, operator+=; bodies
                //   verified from decomp). local_c and uVar15 are not read after
                //   this block, so the chain is dead in the decompilation;
                //   reconstructed minimally for compilability.
                *(CVector*)local_18 = local_3c * (float)(fVar17 * (long double /* Ghidra float10 */)-1.0);
                uVar15 = (uint32_t)(uintptr_t)local_18;
                local_c = *(const CVector*)(const void*)(uintptr_t)uVar15;
                uVar15 = (uint32_t)(uintptr_t)&local_c;
                *(CVector*)(void*)(uintptr_t)uVar15 += local_c;
                (this->m_vecMoveSpeed).z = fVar2;
            }
            local_3c.x = (local_48 + (this->m_vecAnimMovingShift).x) - (this->m_vecMoveSpeed).x;
            local_3c.y = (local_44 + (this->m_vecAnimMovingShift).y) - (this->m_vecMoveSpeed).y;
            // TODO(decomp): Ghidra `*(float*)&m_standingOnEntity[1].__anon1` reads
            //   a float at entity+0x44. In the real CPhysical layout that is
            //   m_vecMoveSpeed.x, but adding a linear velocity to a rotation
            //   is dimensionally suspect (likely the platform's yaw rate).
            //   Kept as the raw offset to match the decompiled address;
            //   verify against live platform-rotation behavior.
            const float platformRotRate = *(const float*)((const uint8_t*)this->m_standingOnEntity + 0x44);
            this->m_fCurrentRotation =
            CTimer::ms_fTimeStep * platformRotRate +
            this->m_fCurrentRotation;
            this->m_fAimingRotation =
            CTimer::ms_fTimeStep * platformRotRate +
            this->m_fAimingRotation;
        }
        pCVar4 = this->m_standingOnEntity;
        if (((pCVar4 == nullptr) || (pCVar4[4].m_nFlags != 0)) ||
        ((fVar2 = pCVar4[1].m_placement.m_vPosn.y, ((uint32_t)fVar2 & 4) != 0 && (((uint32_t)fVar2 & 8) == 0)))
        ) {
            uVar6 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
            if (((uVar6 & 0x100000) == 0) || (pCVar4 != nullptr)) goto LAB_005e1f85;
            fVar2 = std::sqrt(local_3c.x * local_3c.x + local_3c.y * local_3c.y);
            if (fVar2 <= CTimer::ms_fTimeStep * 0.01) goto LAB_005e1f85;
            fVar2 = (CTimer::ms_fTimeStep * 0.01) / fVar2;
        }
        else {
            fVar3 = std::sqrt(local_3c.x * local_3c.x + local_3c.y * local_3c.y);
            if (((uint32_t)pCVar4->GetType() & 7) == 2) {
                if (this->m_nPedState == PEDSTATE_DIE) {
                    fVar2 = CTimer::ms_fTimeStep * 0.002;
                }
                else {
                    // TODO(port): decomp `pCVar4[0x19].m_pRwObject` is a Ghidra artifact - a
                    // dword read at an unrecovered offset of the standing-on vehicle, compared
                    // against immediate 9. Best guess: the vehicle's RwObject.
                    // `CVector::unk_00406da0()` (0x406DA0, "no distinguishing evidence") is a
                    // squared-magnitude check (verified pattern in Render/KillPedWithCar);
                    // best guess here: the standing-on vehicle's move speed.
                    pRVar5 = pCVar4->GetRwObject();
                    if ((pRVar5 != (RwObject *)0x9) ||
                    (fVar17 = (long double /* Ghidra float10 */)((CPhysical*)pCVar4)->m_vecMoveSpeed.SquaredMagnitude(), fVar17 <= (long double /* Ghidra float10 */)0.040000003)) {
                        fVar2 = fVar3;
                        if (pRVar5 == nullptr) goto LAB_005e1f0f;
                    }
                    else {
                        fVar2 = CTimer::ms_fTimeStep * 0.00020000001;
                    }
                }
            }
            else {
            LAB_005e1f0f:
                fVar2 = CTimer::ms_fTimeStep * 0.01;
            }
            if (fVar3 <= fVar2) goto LAB_005e1f85;
            fVar2 = fVar2 / fVar3;
        }
        local_3c.x = local_3c.x * fVar2;
        local_3c.y = fVar2 * local_3c.y;
    LAB_005e1f85:
        (this->m_vecMoveSpeed).x = local_3c.x + (this->m_vecMoveSpeed).x;
        (this->m_vecMoveSpeed).y = local_3c.y + (this->m_vecMoveSpeed).y;
        return;
}










void CPed::ProcessBuoyancy() {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): goto
    // TODO(convert): uint8_t-slice
        CTaskManager *this_00;
        uint32_t uVar1;
        CEntity *pCVar2;
        float fVar3;
        CVector force;
        bool bVar4;
        CTaskSimpleSwim *pCVar5;
        CTask *pCVar6;
        CMatrix *pCVar7;
        CSimpleTransform *pCVar8;
        int iVar9;
        uint32_t uVar10;
        uint32_t uVar11;
        uint32_t uVar12;
        uint32_t uVar13;
        long double /* Ghidra float10 */ fVar14;
        CEntity *local_70;
        CVector vecLocal6c; // TODO(port): decomp local_6c (16 bytes) reused as CVector and event storage
        CVector local_5c;
        RwV3d RStack_50;
        CVector local_44;
        CColPoint local_38;
        void *local_c;
        uint32_t uStack_4;

        fVar3 = 1.1;
        uStack_4 = 0xffffffff;
        uVar1 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar1 & 0x100) != 0) {
            return;
        }
        if ((this->m_nPedState == PEDSTATE_DEAD) || (this->m_nPedState == PEDSTATE_DIE)) {
            fVar3 = 1.8;
        }
        // TODO(port): decomp static-style cBuoyancy::ProcessBuoyancy call
        bVar4 = ((CBuoyancy *)&mod_Buoyancy)->ProcessBuoyancy((CPhysical *)this, fVar3 * this->m_fMass * 0.008f, &local_44, &local_5c); /* TODO(port): decomp static-style */
        if (!bVar4) {
            m_nPhysicalFlags = m_nPhysicalFlags & 0xf7ffffff;
            pCVar5 = m_pIntelligence->GetTaskSwim();
            if (pCVar5 == nullptr) {
                return;
            }
            // TODO(port): decomp `pCVar6[10].m_Parent` (written as float 1000.0) is
            // CTaskSimpleSwim::m_fSwimStopTime (gta-reversed CPed::ProcessBuoyancy).
            // The GetSimplestActiveTask result was unused/misattributed by Ghidra.
            pCVar5->m_fSwimStopTime = 1000.0f;
            return;
        }
        // TODO(port): decomp artifacts resolved per gta-reversed CPed::ProcessBuoyancy:
        //   `(pCVar2 = x->GetSimplestActiveTask(pCVar2 != nullptr))` was a mangled comma
        //   expression `(pCVar2 = x, pCVar2 != nullptr)`;
        //   `pCVar2[0x19].m_pRwObject == (RwObject*)0x5` is CVehicle::IsBoat();
        //   `pCVar2[1]... & 0x20000000` is !physicalFlags.bRenderScorched.
        if (((((bIsStanding & 1) != 0) &&
        (pCVar2 = this->m_pContactEntity, pCVar2 != nullptr)) &&
        (pCVar2->GetIsTypeVehicle())) &&
        ((pCVar2->AsVehicle()->IsBoat() &&
        ((!pCVar2->AsVehicle()->physicalFlags.bRenderScorched))))) {
            m_nPhysicalFlags = m_nPhysicalFlags & 0xfffffeff;
            pCVar5 = m_pIntelligence->GetTaskSwim();
            if (pCVar5 == nullptr) {
                return;
            }
        LAB_005e2065:
            // TODO(port): decomp `pCVar6[10].m_Parent` (as float) is
            // CTaskSimpleSwim::m_fSwimStopTime (gta-reversed).
            pCVar5->m_fSwimStopTime = pCVar5->m_fSwimStopTime + CTimer::ms_fTimeStep;
            return;
        }
        if (this->m_pPlayerData != nullptr) {
            if (this->m_matrix == nullptr) {
                pCVar8 = &this->m_placement;
            }
            else {
                pCVar8 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            vecLocal6c.x = (pCVar8->m_vPosn).x;
            vecLocal6c.y = (pCVar8->m_vPosn).y;
            vecLocal6c.z = (pCVar8->m_vPosn).z;
            float fCheckZ = vecLocal6c.z - 3.0f;
            local_70 = nullptr;
            bVar4 = CWorld::ProcessVerticalLine
            (vecLocal6c,fCheckZ,local_38,local_70,false,true,false,
            false,false,false,nullptr);
            // TODO(port): decomp artifacts resolved per gta-reversed CPed::ProcessBuoyancy:
            // `local_70[0x19].m_pRwObject == (RwObject*)0x5` is CVehicle::IsBoat();
            // `local_70[1]... & 0x20000000` is !physicalFlags.bRenderScorched.
            if ((bVar4) && (local_70->GetIsTypeVehicle())) {
                CVehicle* pColVehicle = local_70->AsVehicle();
                if ((pColVehicle->IsBoat() &&
                ((!pColVehicle->physicalFlags.bRenderScorched) &&
                (pCVar7 = &((CPlaceable*)local_70)->GetMatrix(), 0.0 < (pCVar7->m_up).z)))) {
                    m_nPhysicalFlags = m_nPhysicalFlags & 0xfffffeff;
                    return;
                }
            }
        }
        CTimeCycle::GetAmbientRed();
        CTimeCycle::GetAmbientGreen();
        CTimeCycle::GetAmbientBlue();
        CGeneral::GetRandomNumber();
        if (((m_nPhysicalFlags & 0x8000000) == 0) &&
        ((this->m_vecMoveSpeed).z < -0.01)) {
            RStack_50.x = CTimer::ms_fTimeStep * (this->m_vecMoveSpeed).x;
            if (this->m_matrix == nullptr) {
                pCVar8 = &this->m_placement;
            }
            else {
                pCVar8 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            // TODO(port): decomp reused local_6c/local_70 stack slots as floats;
            // using explicit locals (semantics per gta-reversed CPed::ProcessBuoyancy).
            CVector vecSplashPos;
            vecSplashPos.z = CTimer::ms_fTimeStep * (this->m_vecMoveSpeed).z * 4.0f + (pCVar8->m_vPosn).z;
            vecSplashPos.y = CTimer::ms_fTimeStep * (this->m_vecMoveSpeed).y * 4.0f + (pCVar8->m_vPosn).y;
            vecSplashPos.x = RStack_50.x * 4.0f + (pCVar8->m_vPosn).x;
            float fWaterZ = 0.0f;
            bVar4 = CWaterLevel::GetWaterLevel
            (vecSplashPos.x,vecSplashPos.y,vecSplashPos.z,
            &fWaterZ,true,nullptr);
            if (bVar4) {
                vecSplashPos.z = fWaterZ;
                g_fx.TriggerWaterSplash(vecSplashPos);
                AudioEngine.ReportWaterSplash((CPhysical *)this,-100.0f,true);
            }
        }
        m_nPhysicalFlags = m_nPhysicalFlags | 0x8000100;
        force.y = local_5c.y;
        force.x = local_5c.x;
        force.z = local_5c.z;
        CPhysical::ApplyMoveForce(force);
        if (local_5c.z / this->m_fMass <= CTimer::ms_fTimeStep * 0.008) {
            if (this->m_matrix == nullptr) {
                pCVar8 = &this->m_placement;
            }
            else {
                pCVar8 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            // TODO(port): decomp _DAT_00c1c8f4 is mod_Buoyancy.m_fWaterLevel (gta-reversed).
            if (mod_Buoyancy.m_fWaterLevel <= (pCVar8->m_vPosn).z + 0.6) {
                if (((bIsStanding & 1) == 0) ||
                (pCVar5 = m_pIntelligence->GetTaskSwim(),
                pCVar5 == nullptr)) {
                    if (this->m_pPlayerData == nullptr) {
                        return;
                    }
                    RStack_50.x = 0.0;
                    RStack_50.y = 0.0;
                    RStack_50.z = 0.1;
                    GetTransformedBonePosition(RStack_50,eBoneTagU32((eBoneTag)5),false); // TODO(port): decomp static-style call
                    if (mod_Buoyancy.m_fWaterLevel <= RStack_50.z) {
                        return;
                    }
                    ((CPlayerPed*)this)->HandlePlayerBreath(true,1.0f); // TODO(port): decomp static-style call
                    return;
                }
                goto LAB_005e2065;
            }
        }
        bVar4 = false;
        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            uVar13 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            uVar11 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar13 = uVar13 & 0xfffffffe;
            bIsStanding = (char)uVar13;
            bInVehicle = (char)(uVar13 >> 8);
            bFiringWeapon = (char)(uVar13 >> 0x10);
            bNotAllowedToDuck = (char)(uVar13 >> 0x18);
            uVar11 = uVar11 | 0x80000;
            bResetWalkAnims = (char)uVar11;
            bCollidedWithMyVehicle = (char)(uVar11 >> 8);
            bMiamiViceCop = (char)(uVar11 >> 0x10);
            bDontFight = (char)(uVar11 >> 0x18);
            // TODO(port): decomp placement-constructed the event in local_6c via
            // unk_004b1370(0x3f400000 = 0.75f) then ran the dtor; ported as a plain local.
            CEventHitByWaterCannon hitByWaterCannonEvent(0.75f);
            uStack_4 = 1;
            this->m_pIntelligence->m_eventGroup.Add(&hitByWaterCannonEvent,false);
            uStack_4 = 0xffffffff;
            goto LAB_005e2425;
        }
        pCVar5 = m_pIntelligence->GetTaskSwim();
        this_00 = &this->m_pIntelligence->m_TaskMgr;
        if (pCVar5 == nullptr) {
            // TODO(port): decomp did GetSimplestActiveTask + vtable GetTaskType()
            // == 0xfe (TASK_SIMPLE_SWIM); simplified via FindActiveTaskByType since
            // the stub GetSimplestActiveTask returns the active task itself.
            if (this_00->FindActiveTaskByType(TASK_SIMPLE_SWIM) != nullptr) {
                bVar4 = true;
                goto LAB_005e23fd;
            }
            // TODO(port): decomp placement-constructed the event in local_6c via
            // unk_004b1370 then ran the dtor; ported as a plain local.
            CEventHitByWaterCannon inWaterEvent
            (local_5c.z / (CTimer::ms_fTimeStep * this->m_fMass * 0.008f));
            uStack_4 = 0;
            this->m_pIntelligence->m_eventGroup.Add(&inWaterEvent,false);
            uStack_4 = 0xffffffff;
        }
        else {
            // TODO(port): decomp `pCVar6[10].m_Parent` (as float) is
            // CTaskSimpleSwim::m_fSwimStopTime (gta-reversed).
            pCVar5->m_fSwimStopTime = 0.0f;
            bVar4 = true;
        }
    LAB_005e23fd:
        uVar12 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar10 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
        uVar12 = uVar12 & 0xfffffffe;
        uVar10 = uVar10 | 0x80000;
        bIsStanding = (char)uVar12;
        bInVehicle = (char)(uVar12 >> 8);
        bFiringWeapon = (char)(uVar12 >> 0x10);
        bNotAllowedToDuck = (char)(uVar12 >> 0x18);
        bResetWalkAnims = (char)uVar10;
        bCollidedWithMyVehicle = (char)(uVar10 >> 8);
        bMiamiViceCop = (char)(uVar10 >> 0x10);
        bDontFight = (char)(uVar10 >> 0x18);
        if (bVar4) {
            return;
        }
    LAB_005e2425:
        fVar14 = (long double /* Ghidra float10 */)FxInterpInfo_c::unk_00822130();
        (this->m_vecMoveSpeed).x = (float)(fVar14 * (long double /* Ghidra float10 */)(this->m_vecMoveSpeed).x);
        (this->m_vecMoveSpeed).y = (float)(fVar14 * (long double /* Ghidra float10 */)(this->m_vecMoveSpeed).y);
        if (0.0 <= (this->m_vecMoveSpeed).z) {
            return;
        }
        (this->m_vecMoveSpeed).z = (float)(fVar14 * (long double /* Ghidra float10 */)(this->m_vecMoveSpeed).z);
        return;
}










bool CPed::IsPedInControl() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone IsPedInControl; body from gta-reversed Ped.cpp.
    return !bIsLanding && !bIsInTheAir && IsAlive() && m_nPedState != PEDSTATE_ARRESTED;
}












void CPed::RemoveWeaponModel(int32_t modelIndex) {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
        int iVar1;
        CClumpModelInfo *pCVar2;
        CClumpModelInfo *pCVar3;
        RpAtomic *pRVar4;

        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            // TODO(port): decomp did byte arithmetic on m_aWeapons (stride 0x1c);
            // +0x18 is CWeapon::m_FxSystem.
            CWeapon& activeWeapon = this->m_aWeapons[this->m_nActiveWeaponSlot];
            if (activeWeapon.m_FxSystem != nullptr) {
                g_fxMan.DestroyFxSystem(activeWeapon.m_FxSystem);
                activeWeapon.m_FxSystem = nullptr;
            }
        }
        if (this->m_pWeaponObject != nullptr) {
            if (modelIndex != -1) {
                pCVar3 = (CClumpModelInfo *)CModelInfo::ms_modelInfoPtrs[modelIndex];
                pCVar2 = CVisibilityPlugins::GetClumpModelInfo(this->m_pWeaponObject);
                if (pCVar2 != pCVar3) goto LAB_005e3a65;
            }
            pCVar3 = CVisibilityPlugins::GetClumpModelInfo(this->m_pWeaponObject);
            pCVar3->RemoveRef(); // TODO(port): decomp static-style call
            pRVar4 = GetFirstAtomic(this->m_pWeaponObject);
            if (pRVar4 != nullptr) {
                iVar1 = (RpSkinGeometryGetSkin(pRVar4->geometry) != nullptr); // TODO(port): decomp treated the RpSkin* as int
                if (iVar1 != 0) {
                    RpClumpForAllAtomics(this->m_pWeaponObject,AtomicRemoveAnimFromSkinCB,nullptr); // TODO(port): decomp artifact
                }
            }
            RpClumpDestroy(this->m_pWeaponObject);
            this->m_pWeaponObject = nullptr;
            this->m_pGunflashObject = nullptr;
        }
    LAB_005e3a65:
        this->m_nWeaponGunflashAlphaMP1 = 0;
        this->m_nWeaponGunflashAlphaMP2 = 0;
        this->m_nWeaponModelId = -1;
        return;
}










void CPed::AddGogglesModel(int32_t modelIndex, bool& inOutGogglesState) {    // converted from decomp src/CPed/*.c
        CBaseModelInfo *this_00;
        RpClump *pRVar1;

        if (modelIndex != -1) {
            this_00 = CModelInfo::ms_modelInfoPtrs[modelIndex];
            if (this->m_pGogglesObject != nullptr) {
                RemoveGogglesModel();
            }
            // TODO(port): decomp vtable+0x2c call; best guess CBaseModelInfo::CreateInstance().
            pRVar1 = (RpClump *)this_00->CreateInstance();
            this->m_pGogglesObject = pRVar1;
            (this_00)->AddRef();
            this->m_pGogglesState = &inOutGogglesState;
            inOutGogglesState = true;
        }
        return;
}










void CPed::PutOnGoggles() {    // converted from decomp src/CPed/*.c
        eWeaponType *peVar1;
        eWeaponType weaponType;
        int modelIndex;
        CWeaponInfo *pCVar2;
        bool *inOutGogglesState;

        pCVar2 = CWeaponInfo::GetWeaponInfo(WEAPON_INFRARED, eWeaponSkill::STD);
        peVar1 = &this->m_aWeapons[(size_t)pCVar2->m_nSlot].m_Type; // TODO(port): decomp byte arithmetic on array
        if ((peVar1 != nullptr) &&
        ((weaponType = *peVar1, weaponType == WEAPON_INFRARED || (weaponType == WEAPON_NIGHTVISION))))
        {
            pCVar2 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
            modelIndex = pCVar2->m_nModelId1;
            if (*peVar1 == WEAPON_INFRARED) {
                inOutGogglesState = &CPostEffects::m_bInfraredVision;
            }
            else {
                inOutGogglesState = &CPostEffects::m_bNightVision;
            }
            AddGogglesModel(modelIndex,*inOutGogglesState); // TODO(port): decomp static-style call
            // TODO(port): decomp wrote byte at weapon+0x15; that is CWeapon::m_DontPlaceInHand.
            reinterpret_cast<CWeapon*>(peVar1)->m_DontPlaceInHand = true;
            if (peVar1 == &this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type) { // TODO(port): decomp byte arithmetic
                RemoveWeaponModel(modelIndex); // TODO(port): decomp static-style call
            }
        }
        return;
}










eWeaponSkill CPed::GetWeaponSkill(eWeaponType weaponType) {    // converted from decomp src/CPed/GetWeaponSkill_005e3b60.c
        ePedType eVar1;
        uint32_t uVar2;
        eStats stat;
        CWeaponInfo *pCVar3;
        float fVar4;

        if ((0x15 < (int)weaponType) && ((int)weaponType < 0x21)) {
            eVar1 = this->m_nPedType;
            if ((eVar1 != PED_TYPE_PLAYER1) && (eVar1 != PED_TYPE_PLAYER2)) {
                if ((weaponType == WEAPON_PISTOL) && (eVar1 == PED_TYPE_COP)) {
                    return eWeaponSkill::COP;
                }
                return this->m_nWeaponSkill;
            }
            stat = CWeaponInfo::GetSkillStatIndex(weaponType);
            pCVar3 = CWeaponInfo::GetWeaponInfo(weaponType,eWeaponSkill::PRO);
            uVar2 = pCVar3->m_nReqStatLevel;
            fVar4 = CStats::GetStatValue(stat);
            if ((float)(int)uVar2 < fVar4 != ((float)(int)uVar2 == fVar4)) {
                return eWeaponSkill::PRO;
            }
            pCVar3 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
            uVar2 = pCVar3->m_nReqStatLevel;
            fVar4 = CStats::GetStatValue(stat);
            if ((float)(int)uVar2 < fVar4 == ((float)(int)uVar2 == fVar4)) {
                return eWeaponSkill::POOR;
            }
        }
        return eWeaponSkill::STD;
}












void CPed::SetWeaponSkill(eWeaponType weaponType,  eWeaponSkill skill) {    // converted from decomp src/CPed/*.c
        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            this->m_nWeaponSkill = skill;
        }
        return;
}










void CPed::ClearLook() {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone ClearLook; body from gta-reversed Ped.cpp.
    ClearLookFlag();
}












bool CPed::TurnBody() {    // converted from decomp src/CPed/*.c
        CEntity *pCVar1;
        CMatrixLink *pCVar2;
        float fVar3;
        float fVar4;
        CSimpleTransform *pCVar5;
        CSimpleTransform *pCVar6;
        CSimpleTransform *pCVar7;
        CSimpleTransform *pCVar8;
        float fVar9;
        float fVar10;

        pCVar1 = this->m_pLookTarget;
        if (pCVar1 != nullptr) {
            pCVar2 = this->m_matrix;
            pCVar7 = (CSimpleTransform *)&pCVar2->m_pos;
            if (pCVar2 == nullptr) {
                pCVar7 = &this->m_placement;
                pCVar8 = &this->m_placement;
            }
            else {
                pCVar8 = (CSimpleTransform *)&pCVar2->m_pos;
            }
            pCVar6 = (CSimpleTransform *)&pCVar1->m_matrix->m_pos;
            if (pCVar1->m_matrix == nullptr) {
                pCVar6 = &pCVar1->m_placement;
            }
            if (pCVar1->m_matrix == nullptr) {
                pCVar5 = &pCVar1->m_placement;
            }
            else {
                pCVar5 = (CSimpleTransform *)&pCVar1->m_matrix->m_pos;
            }
            fVar9 = CGeneral::GetRadianAngleBetweenPoints
            ((pCVar5->m_vPosn).x,(pCVar6->m_vPosn).y,(pCVar8->m_vPosn).x,
            (pCVar7->m_vPosn).y);
            this->m_fLookDirection = fVar9;
        }
        fVar10 = CGeneral::LimitRadianAngle(this->m_fLookDirection);
        fVar9 = this->m_fCurrentRotation;
        if (fVar10 <= fVar9 + 3.1415927) {
            if (fVar10 < fVar9 - 3.1415927) {
                fVar10 = fVar10 + 6.2831855;
            }
        }
        else {
            fVar10 = fVar10 - 6.2831855;
        }
        fVar3 = fVar9 - fVar10;
        this->m_fAimingRotation = fVar10;
        fVar4 = fVar3;
        if (fVar3 < 0.0) {
            fVar4 = -fVar3;
        }
        if (fVar4 <= 0.05) {
            this->m_fCurrentRotation = fVar9;
            this->m_fLookDirection = fVar10;
            return true;
        }
        this->m_fCurrentRotation = fVar9 - fVar3 * 0.2;
        this->m_fLookDirection = fVar10;
        return false;
}










bool CPed::IsPointerValid() {    // converted from decomp src/CPed/*.c
        int iVar1;
        CPlayerPed *pCVar2;

        iVar1 = 0 /* TODO(port): GetPedPool()->GetIndex(this) disabled for C1202 */; // TODO(port): decomp pool arithmetic
        if ((iVar1 < 0) || (0x8b < iVar1)) {
            return false;
        }
        if (((this->m_pCollisionList).m_node == nullptr) &&
        (pCVar2 = FindPlayerPed(-1), (CPlayerPed *)this != pCVar2)) {
            return false;
        }
        return true;
}










CVector CPed::GetBonePosition(eBoneTag boneId,  bool updateSkinBones) {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        uint32_t uVar1;
        uint32_t uVar2;
        CMatrixLink *pCVar3;
        RpHAnimHierarchy *pRVar4;
        float *pfVar5;
        int iVar6;
        int iVar7;
        uint16_t in_stack_0000000a;
        uint8_t local_c [12];
        CVector outVec; // TODO(port): decomp used out-param; ported signature returns CVector

        uVar2 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        if (updateSkinBones) {
            if ((uVar2 & 0x400) == 0) {
                this->UpdateRpHAnim(); // TODO(port): decomp static-style call
                uVar1 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
                uVar1 = uVar1 | 0x400;
                bDonePositionOutOfCollision = (char)uVar1;
                bKilledByStealth = (char)(uVar1 >> 8);
                bRightArmBlocked = (char)(uVar1 >> 0x10);
                bWaitingForScriptBrainToLoad = (char)(uVar1 >> 0x18);
            }
        }
        else if ((uVar2 & 0x400) == 0) {
            // TODO(port): CMatrix::Scale signature unported; _bone -> boneId
            pfVar5 = (float *)&DAT_008d13a8[(int)boneId * 0xc]; // TODO(port): CMatrix::Scale unported (decomp matrix-vector transform)
            outVec.x = *pfVar5;
            outVec.y = pfVar5[1];
            outVec.z = pfVar5[2];
            return outVec;
        }
        pRVar4 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
        if (pRVar4 == nullptr) {
            pCVar3 = this->m_matrix;
            if (pCVar3 != nullptr) {
                outVec.x = (pCVar3->m_pos).x;
                outVec.y = (pCVar3->m_pos).y;
                outVec.z = (pCVar3->m_pos).z;
                return outVec;
            }
            outVec.x = (this->m_placement).m_vPosn.x;
            outVec.y = (this->m_placement).m_vPosn.y;
            outVec.z = (this->m_placement).m_vPosn.z;
            return outVec;
        }
        // TODO(port): decomp did raw pointer arithmetic; RwMatrix is 0x40 bytes, m_pos at +0x30.
        iVar6 = RpHAnimIDGetIndex(pRVar4,(int)boneId);
        RwMatrix* pBoneMat = (RwMatrix*)((uint8_t*)RpHAnimHierarchyGetMatrixArray(pRVar4) + iVar6 * 0x40);
        outVec.x = pBoneMat->pos.x;
        outVec.y = pBoneMat->pos.y;
        outVec.z = pBoneMat->pos.z;
        return outVec;
}










void CPed::GetBonePosition(CVector* outVec,  eBoneTag bone,  bool updateSkinBones) {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        uint32_t uVar1;
        uint32_t uVar2;
        CMatrixLink *pCVar3;
        RpHAnimHierarchy *pRVar4;
        float *pfVar5;
        int iVar6;
        int iVar7;
        uint16_t in_stack_0000000a;
        uint8_t local_c [12];

        uVar2 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        if (updateSkinBones) {
            if ((uVar2 & 0x400) == 0) {
                this->UpdateRpHAnim(); // TODO(port): decomp static-style call
                uVar1 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
                uVar1 = uVar1 | 0x400;
                bDonePositionOutOfCollision = (char)uVar1;
                bKilledByStealth = (char)(uVar1 >> 8);
                bRightArmBlocked = (char)(uVar1 >> 0x10);
                bWaitingForScriptBrainToLoad = (char)(uVar1 >> 0x18);
            }
        }
        else if ((uVar2 & 0x400) == 0) {
            pfVar5 = (float *)&DAT_008d13a8[(int)bone * 0xc]; // TODO(port): CMatrix::Scale unported (decomp matrix-vector transform)
            outVec->x = *pfVar5;
            outVec->y = pfVar5[1];
            outVec->z = pfVar5[2];
            return;
        }
        pRVar4 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
        if (pRVar4 == nullptr) {
            pCVar3 = this->m_matrix;
            if (pCVar3 != nullptr) {
                outVec->x = (pCVar3->m_pos).x;
                outVec->y = (pCVar3->m_pos).y;
                outVec->z = (pCVar3->m_pos).z;
                return;
            }
            outVec->x = (this->m_placement).m_vPosn.x;
            outVec->y = (this->m_placement).m_vPosn.y;
            outVec->z = (this->m_placement).m_vPosn.z;
            return;
        }
        // TODO(port): decomp did raw pointer arithmetic; RwMatrix is 0x40 bytes, m_pos at +0x30.
        iVar6 = RpHAnimIDGetIndex(pRVar4,(int)bone);
        RwMatrix* pBoneMat = (RwMatrix*)((uint8_t*)RpHAnimHierarchyGetMatrixArray(pRVar4) + iVar6 * 0x40);
        outVec->x = pBoneMat->pos.x;
        outVec->y = pBoneMat->pos.y;
        outVec->z = pBoneMat->pos.z;
        return;
}










void CPed::GiveObjectToPedToHold(int32_t modelIndex,  uint8_t replace) {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
        float fVar1;
        float fVar2;
        float fVar3;
        bool bVar4;
        CTask *pCVar5;
        CEntity *pCVar6;
        int iVar7;
        CSimpleTransform *pCVar8;
        CTaskSimpleHoldEntity *pCVar9;
        CVector CStack_18;
        void *local_c;
        uint32_t uStack_4;

        uStack_4 = 0xffffffff;
        pCVar5 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_HOLD_ENTITY);
        bVar4 = false;
        pCVar6 = GetEntityThatThisPedIsHolding(); // TODO(port): decomp static-style call
        if ((pCVar6 != nullptr) && (replace != '\0')) {
            DropEntityThatThisPedIsHolding(true); // TODO(port): decomp static-style call
            bVar4 = true;
        }
        if ((pCVar5 != nullptr) && (!bVar4)) {
            return;
        }
        // TODO(port): decomp operator-new + placement-new; simplified to new CObject.
        pCVar6 = new CObject(modelIndex,false);
        uStack_4 = 0;
        uStack_4 = 0xffffffff;
        if (this->m_matrix == nullptr) {
            pCVar8 = &this->m_placement;
        }
        else {
            pCVar8 = (CSimpleTransform *)&this->m_matrix->m_pos;
        }
        fVar1 = (pCVar8->m_vPosn).z;
        fVar2 = (pCVar8->m_vPosn).y;
        fVar3 = (pCVar8->m_vPosn).x;
        if (pCVar6->m_matrix == nullptr) {
            (pCVar6->m_placement).m_vPosn.x = fVar3;
            (pCVar6->m_placement).m_vPosn.y = fVar2;
            (pCVar6->m_placement).m_vPosn.z = fVar1;
        }
        else {
            (pCVar6->m_matrix->m_pos).x = fVar3;
            (pCVar6->m_matrix->m_pos).y = fVar2;
            (pCVar6->m_matrix->m_pos).z = fVar1;
        }
        CWorld::Add(pCVar6);
        CStack_18.x = 0.0;
        CStack_18.y = 0.0;
        CStack_18.z = 0.0;
        // TODO(port): decomp operator-new + placement-new; simplified to new CTaskSimpleHoldEntity.
        pCVar9 = new CTaskSimpleHoldEntity(pCVar6,&CStack_18,0x06,0x10,ANIM_ID_NO_ANIMATION_SET,ANIM_GROUP_DEFAULT,true);
        uStack_4 = 0xffffffff;
        this->m_pIntelligence->m_TaskMgr.SetTaskSecondary((CTask *)pCVar9,TASK_SECONDARY_PARTIAL_ANIM); // TODO(port): decomp static-style call
        return;
}










void CPed::SetPedState(ePedState pedState) {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
        uint32_t uVar1;
        int iVar2;

        if ((pedState == PEDSTATE_DEAD) || (pedState == PEDSTATE_DIE)) {
            if (this->m_pCoverPoint != nullptr) {
                this->m_pCoverPoint->ReleaseCoverPointForPed(this); // TODO(port): decomp static-style call
                this->m_pCoverPoint = nullptr;
            }
            uVar1 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
            if ((uVar1 & 0x2000) != 0) {
                CRadar::ClearBlipForEntity(BLIP_CHAR, 0u /* TODO(port): GetPedPool()->GetRef(this) disabled for C1202 */); // TODO(port): decomp pool arithmetic
            }
        }
        this->m_nPedState = pedState;
        return;
}










void CPed::SetCharCreatedBy(ePedCreatedBy createdBy) {    // converted from decomp src/CPed/*.c
        CPedIntelligence *pCVar1;
        int newType;

        this->m_nCreatedBy = createdBy;
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            newType = -2;
        }
        else if (createdBy == PED_MISSION) {
            newType = -1;
        }
        else {
            newType = (int)this->m_pStats->m_nDefaultDecisionMaker;
        }
        this->m_pIntelligence->SetPedDecisionMakerType(newType);
        if (this->m_nCreatedBy == PED_MISSION) {
            this->m_pIntelligence->SetSeeingRange(30.0);
            this->m_pIntelligence->SetHearingRange(30.0);
            if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
                pCVar1 = this->m_pIntelligence;
                pCVar1->m_fDmRadius = 0.0;
                pCVar1->m_nDmNumPedsToScan = 0;
            }
        }
        return;
}










void CPed::CalculateNewVelocity() {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): uint8_t-slice
        uint32_t uVar1;
        ePedState eVar2;
        ePedType eVar3;
        CMatrixLink *pCVar4;
        uint32_t uVar5;
        uint32_t uVar6;
        float fVar7;
        bool bVar8;
        CTaskSimpleUseGun *pCVar9;
        CTaskSimpleFight *pCVar10;
        long double /* Ghidra float10 */ fVar11;
        float fVar12;
        float fVar13;
        float local_10;

        uVar1 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if (((((uVar1 & 0x600) == 0) && (eVar2 = this->m_nPedState, eVar2 != PEDSTATE_DIE)) &&
        (eVar2 != PEDSTATE_DEAD)) && (eVar2 != PEDSTATE_ARRESTED)) {
            fVar13 = this->m_fAimingRotation;
            fVar7 = this->m_fHeadingChangeRate * 0.017453292 * CTimer::ms_fTimeStep;
            fVar12 = CGeneral::LimitRadianAngle(this->m_fCurrentRotation);
            this->m_fCurrentRotation = fVar12;
            fVar13 = CGeneral::LimitRadianAngle(fVar13);
            if (fVar13 <= this->m_fCurrentRotation + 3.1415927) {
                if (fVar13 < this->m_fCurrentRotation - 3.1415927) {
                    fVar13 = fVar13 + 6.2831855;
                }
            }
            else {
                fVar13 = fVar13 - 6.2831855;
            }
            fVar13 = fVar13 - this->m_fCurrentRotation;
            eVar3 = this->m_nPedType;
            if ((eVar3 == PED_TYPE_PLAYER1) || (eVar3 == PED_TYPE_PLAYER2)) {
                this->m_fMoveAnim = 1.0;
            }
            else if ((fVar13 < 0.0) || (0.0 <= this->m_fMoveAnim)) {
                if ((fVar13 < 0.0) && (0.0 < this->m_fMoveAnim)) {
                    this->m_fMoveAnim = -0.1;
                }
            }
            else {
                this->m_fMoveAnim = 0.1;
            }
            bVar8 = false;
            fVar12 = std::abs(this->m_fMoveAnim) * fVar7;
            if (fVar13 <= fVar12) {
                if (fVar12 * -1.0 <= fVar13) {
                    if (((eVar3 == PED_TYPE_PLAYER1) || (eVar3 == PED_TYPE_PLAYER2)) ||
                    (std::abs(fVar13) <= fVar7 * 0.1)) {
                        this->m_fCurrentRotation = fVar13 + this->m_fCurrentRotation;
                        fVar7 = std::abs(fVar13) / fVar7;
                        if (fVar7 < 0.1) {
                            fVar7 = 0.1;
                        }
                    }
                    else {
                        this->m_fCurrentRotation = fVar13 * 0.5 + this->m_fCurrentRotation;
                        fVar7 = this->m_fMoveAnim * 0.5;
                    }
                    this->m_fMoveAnim = fVar7;
                }
                else {
                    this->m_fCurrentRotation = this->m_fCurrentRotation - fVar12;
                    fVar7 = this->m_fMoveAnim - CTimer::ms_fTimeStep * 0.1;
                    this->m_fMoveAnim = fVar7;
                    if (fVar7 < -1.0) {
                        this->m_fMoveAnim = -1.0;
                    }
                    bVar8 = true;
                }
            }
            else {
                this->m_fCurrentRotation = fVar12 + this->m_fCurrentRotation;
                fVar7 = CTimer::ms_fTimeStep * 0.1 + this->m_fMoveAnim;
                this->m_fMoveAnim = fVar7;
                if (1.0 < fVar7) {
                    this->m_fMoveAnim = 1.0;
                }
                bVar8 = true;
            }
            if (((((eVar3 == PED_TYPE_PLAYER1) || (eVar3 == PED_TYPE_PLAYER2)) ||
            ((this->m_nMoveState != PEDMOVE_STILL && (this->m_nMoveState != PEDMOVE_NONE)))) ||
            ((!bVar8 ||
            (pCVar9 = this->m_pIntelligence->GetTaskUseGun(),
            pCVar9 != nullptr)))) ||
            (pCVar10 = this->m_pIntelligence->GetTaskFighting(), // TODO(port): decomp static-style
            pCVar10 != nullptr)) {
                if ((this->m_nMoveState == PEDMOVE_TURN_L) || (this->m_nMoveState == PEDMOVE_TURN_R)) {
                    this->m_nMoveState = PEDMOVE_STILL;
                }
            }
            else if (fVar13 <= 0.0) {
                this->m_nMoveState = PEDMOVE_TURN_R;
            }
            else {
                this->m_nMoveState = PEDMOVE_TURN_L;
            }
            (this->m_pedIK).m_fBodyRoll = 0.0;
        }
        pCVar4 = this->m_matrix;
        fVar13 = (this->field_578).x * (pCVar4->m_forward).x +
        (this->field_578).y * (pCVar4->m_forward).y + (this->field_578).z * (pCVar4->m_forward).z
        ;
        fVar7 = (this->field_578).x * (pCVar4->m_right).x +
        (this->field_578).y * (pCVar4->m_right).y + (this->field_578).z * (pCVar4->m_right).z;
        if (true) { // TODO(port): m_pedIK.m_nFlags opaque (CPedIK unported); original checked bit 3
            if ((((int)this->m_nMoveState < 4) || (this->m_nPedType == PED_TYPE_PLAYER1)) ||
            (this->m_nPedType == PED_TYPE_PLAYER2)) goto LAB_005e51a5;
            if (0.02 < std::abs((this->m_pedIK).m_fSlopeRoll)) {
                fVar11 = (long double /* Ghidra float10 */)FxInterpInfo_c::unk_00822130();
                (this->m_pedIK).m_fSlopeRoll = (float)(fVar11 * (long double /* Ghidra float10 */)(this->m_pedIK).m_fSlopeRoll);
            }
        LAB_005e519f:
            (this->m_pedIK).m_fSlopeRoll = 0.0;
        }
        else {
            local_10 = 0.0;
            if (this->m_nPedState == PEDSTATE_DIE) {
            LAB_005e509d:
                fVar11 = 0.0; // TODO(port): CGeneral::unk_00821e70 unidentified decomp function
                (this->m_pedIK).m_fSlopeRoll = (float)fVar11;
            }
            else {
                uVar5 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
                if (((uVar5 & 0x100000) == 0) &&
                ((((int)this->m_nMoveState < 4 ||
                (bVar8 = g_surfaceInfos.IsStairs((uint32_t)(uint8_t)this->m_nContactSurface) /* TODO(port): decomp static-style */,
                bVar8)) || ((this->GetFourthPedFlags() & 0x4000) != 0))))
                {
                    if (((this->m_pedIK).m_fSlopePitch != 0.0) || ((this->m_pedIK).m_fSlopeRoll != 0.0)) {
                        fVar11 = (long double /* Ghidra float10 */)FxInterpInfo_c::unk_00822130();
                        local_10 = (float)fVar11;
                    }
                    if (std::abs((this->m_pedIK).m_fSlopePitch) <= 0.01) {
                        (this->m_pedIK).m_fSlopePitch = 0.0;
                    }
                    else {
                        (this->m_pedIK).m_fSlopePitch = local_10 * (this->m_pedIK).m_fSlopePitch;
                    }
                    if (0.02 < std::abs((this->m_pedIK).m_fSlopeRoll)) {
                        (this->m_pedIK).m_fSlopeRoll = local_10 * (this->m_pedIK).m_fSlopeRoll;
                        goto LAB_005e51a5;
                    }
                    goto LAB_005e519f;
                }
                if ((this->m_nPedState == PEDSTATE_DIE) ||
                ((bIsPedDieAnimPlaying) != 0)) goto LAB_005e509d;
                (this->m_pedIK).m_fSlopeRoll = 0.0;
            }
            fVar11 = 0.0; // TODO(port): CGeneral::unk_00821e70 unidentified decomp function
            (this->m_pedIK).m_fSlopePitch =
            (float)((long double /* Ghidra float10 */)0.75 * (long double /* Ghidra float10 */)(this->m_pedIK).m_fSlopePitch +
            ((long double /* Ghidra float10 */)1.0 - (long double /* Ghidra float10 */)0.75) * fVar11);
        }
    LAB_005e51a5:
        fVar13 = 1.0 - fVar13 * fVar13;
        if (fVar13 < 0.0) {
            fVar13 = 0.0;
        }
        fVar7 = 1.0 - fVar7 * fVar7;
        if (fVar7 < 0.0) {
            fVar7 = 0.0;
        }
        (this->m_vecAnimMovingShift).x = 0.0;
        (this->m_vecAnimMovingShift).y = 0.0;
        fVar12 = std::sqrt(fVar13) * (this->m_vecAnimMovingShiftLocal).y;
        fVar13 = (this->m_matrix->m_forward).y;
        (this->m_vecAnimMovingShift).x =
        fVar12 * (this->m_matrix->m_forward).x + (this->m_vecAnimMovingShift).x;
        (this->m_vecAnimMovingShift).y = fVar12 * fVar13 + (this->m_vecAnimMovingShift).y;
        fVar7 = std::sqrt(fVar7) * (this->m_vecAnimMovingShiftLocal).x;
        fVar13 = (this->m_matrix->m_right).y;
        (this->m_vecAnimMovingShift).x =
        fVar7 * (this->m_matrix->m_right).x + (this->m_vecAnimMovingShift).x;
        (this->m_vecAnimMovingShift).y = fVar7 * fVar13 + (this->m_vecAnimMovingShift).y;
        if ((CTimer::ms_fTimeStep < 0.01) && (CTimer::bSlowMotionActive == false)) {
            (this->m_vecAnimMovingShift).x = (this->m_vecAnimMovingShift).x * 0.01;
            (this->m_vecAnimMovingShift).y = (this->m_vecAnimMovingShift).y * 0.01;
            return;
        }
        fVar13 = 1.0 / CTimer::ms_fTimeStep;
        (this->m_vecAnimMovingShift).x = fVar13 * (this->m_vecAnimMovingShift).x;
        (this->m_vecAnimMovingShift).y = fVar13 * (this->m_vecAnimMovingShift).y;
        return;
}










void CPed::CalculateNewOrientation() {    // converted from decomp src/CPed/*.c
    // TODO(convert): unknown flag mask 0x600
    // TODO(convert): uint8_t-slice
        uint32_t uVar1;
        ePedState eVar2;

        if ((((CReplay::Mode != CReplay::MODE_PLAYBACK) &&
        // TODO(port): decomp packed bIsStanding/bInVehicle/bFiringWeapon/
        //   bNotAllowedToDuck into uVar1 bytes and checked (uVar1&0x600)==0.
        //   With bool 0/1 packing, bits 9-10 are never set, so always true.
        true) &&
        (eVar2 = this->m_nPedState, eVar2 != PEDSTATE_DIE)) &&
        ((eVar2 != PEDSTATE_DEAD && (eVar2 != PEDSTATE_ARRESTED)))) {
            CPlaceable::SetOrientation(0.0,0.0,this->m_fCurrentRotation);
        }
        return;
}










void CPed::ClearAll() {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
        ePedState eVar1;
        uint32_t uVar2;
        uint32_t uVar3;

        uVar2 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((((((uVar2 & 0x600) == 0) && (eVar1 = this->m_nPedState, eVar1 != PEDSTATE_DIE)) &&
        (eVar1 != PEDSTATE_DEAD)) && (eVar1 != PEDSTATE_ARRESTED)) ||
        (this->m_nPedState == PEDSTATE_DEAD)) {
            uVar2 = uVar2 | 0x2000;
            bIsStanding = (char)uVar2;
            bInVehicle = (char)(uVar2 >> 8);
            bFiringWeapon = (char)(uVar2 >> 0x10);
            bNotAllowedToDuck = (char)(uVar2 >> 0x18);
            uVar3 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar3 = uVar3 & 0x7fffffef;
            this->m_nPedState = PEDSTATE_NONE;
            this->m_nMoveState = PEDMOVE_NONE;
            bResetWalkAnims = (char)uVar3;
            bCollidedWithMyVehicle = (char)(uVar3 >> 8);
            bMiamiViceCop = (char)(uVar3 >> 0x10);
            bDontFight = (char)(uVar3 >> 0x18);
            this->m_pEntityIgnoredCollision = nullptr;
        }
        return;
}










void CPed::DoFootLanded(bool leftFoot,  uint8_t arg1) {    // converted from decomp src/CPed/*.c
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        float fVar1;
        float fVar2;
        uint32_t uVar3;
        CMatrixLink *pCVar4;
        uint32_t uVar5;
        uint8_t bVar6;
        bool bVar7;
        uint32_t uVar8;
        float *pfVar9;
        CSimpleTransform *pCVar10;
        // TODO(port): dropped unused decomp stack artifact in_stack_00000005
        eBoneTag bone;
        CVector local_30;
        CVector local_24;
        float local_18;
        float local_14;
        float local_10;
        float local_c [2];
        float local_4;

        uVar3 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        if (((uVar3 & 0x400) != 0) && ((m_nFlags & 0x800000) == 0)) {
            if (leftFoot) {
                bone = BONE_L_FOOT;
            }
            else {
                bone = BONE_R_FOOT;
            }
            this->GetBonePosition(&local_30,bone,false); // TODO(port): decomp static-style
            pCVar4 = this->m_matrix;
            local_24.x = (pCVar4->m_forward).x;
            local_24.y = (pCVar4->m_forward).y;
            local_24.z = (pCVar4->m_forward).z;
            local_18 = (pCVar4->m_right).x;
            local_4 = local_24.z * 0.2;
            local_14 = (pCVar4->m_right).y;
            local_10 = (pCVar4->m_right).z;
            uVar5 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            local_30.x = local_24.x * 0.2 + local_30.x;
            local_30.y = local_30.y + local_24.y * 0.2;
            local_30.z = local_4 + (local_30.z - 0.1);
            if (((uVar5 & 0x10000000) != 0) && (bVar7 = CLocalisation::Blood(), bVar7)) {
                CShadows::AddPermanentShadow
                ('\x01',gpBloodPoolTex,&local_30,local_24.x * 0.26,local_24.y * 0.26,local_18 * 0.14
                ,local_14 * 0.14,0xff,200,'\0','\0',4.0,3000,1.0);
                if ((uint32_t)this->m_nDeathTimeMS < 0x15) {
                    uVar8 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
                    uVar8 = uVar8 & 0xefffffff;
                    this->m_nDeathTimeMS = 0;
                    bIsStanding = (char)uVar8;
                    bInVehicle = (char)(uVar8 >> 8);
                    bFiringWeapon = (char)(uVar8 >> 0x10);
                    bNotAllowedToDuck = (char)(uVar8 >> 0x18);
                }
                else {
                    this->m_nDeathTimeMS = this->m_nDeathTimeMS - 0x14;
                }
            }
            bVar7 = g_surfaceInfos.LeavesFootsteps((uint32_t)(uint8_t)this->m_nContactSurface); /* TODO(port): decomp static-style */
            if (bVar7) {
                pCVar10 = (CSimpleTransform *)&this->m_matrix->m_pos;
                if (this->m_matrix == nullptr) {
                    pCVar10 = &this->m_placement;
                }
                if (DAT_00b6f03c == 0) {
                    pfVar9 = (float *)&DAT_00b6f02c;
                }
                else {
                    pfVar9 = (float *)(DAT_00b6f03c + 0x30);
                }
                fVar1 = *pfVar9 - (pCVar10->m_vPosn).x;
                fVar2 = pfVar9[1] - (pCVar10->m_vPosn).y;
                if (std::sqrt(fVar1 * fVar1 + fVar2 * fVar2) < 10.0) {
                    if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
                        bVar6 = 1;
                    }
                    else {
                        bVar6 = 0;
                    }
                    CShadows::AddPermanentShadow
                    ('\x01',gpShadowPedTex,&local_30,local_24.x * -0.26,local_24.y * -0.26,
                    local_18 * -0.1,local_14 * -0.1,0x78,0xfa,0xfa,'2',4.0,
                    (-(uint32_t)bVar6 & 3000) + 2000,1.0);
                }
            }
            if ((((0.1 < CWeather::Rain) && (bVar7 = CCullZones::CamNoRain(), !bVar7)) &&
            (bVar7 = CCullZones::PlayerNoRain(), !bVar7)) && (CGame::currArea == 0)) {
                unk_005e3630(0x3e19999a);
            }
            unk_005e37c0(0);
            if (arg1 != '\0') {
                fVar1 = (this->m_vecAnimMovingShift).y;
                this->m_Wobble = 6.2831855;
                fVar2 = (this->m_vecAnimMovingShift).x;
                this->m_WobbleSpeed = (fVar1 * fVar1 + fVar2 * fVar2) * 20.0 + 0.4;
            }
            if ((m_nPhysicalFlags & 0x100) == 0) {
                bVar7 = g_surfaceInfos.IsShallowWater((uint32_t)(uint8_t)this->m_nContactSurface) /* TODO(port): decomp static-style */;
                if (!bVar7) {
                    return;
                }
                local_24.z = _DAT_008d21ec + local_30.z;
                local_24.x = local_30.x;
                local_24.y = local_30.y;
                pfVar9 = (float *)&this->GetMatrix().m_up; // TODO(port): CPlaceable::GetTopDirection unported; using matrix up
                local_4 = _DAT_008d21e8 * pfVar9[2];
                local_24.x = _DAT_008d21e8 * *pfVar9 + local_24.x;
                local_24.y = local_24.y + _DAT_008d21e8 * pfVar9[1];
                local_24.z = local_4 + local_24.z;
            }
            else {
                if (this->m_matrix == nullptr) {
                    pCVar10 = &this->m_placement;
                }
                else {
                    pCVar10 = (CSimpleTransform *)&this->m_matrix->m_pos;
                }
                local_18 = (pCVar10->m_vPosn).x;
                local_14 = (pCVar10->m_vPosn).y;
                local_10 = (pCVar10->m_vPosn).z;
                CWaterLevel::GetWaterLevel
                (local_18,local_14,local_10 + 1.5,(float *)&leftFoot,'\x01',nullptr);
                local_c[0] = CTimer::ms_fTimeStep * (this->m_vecMoveSpeed).x;
                fVar1 = CTimer::ms_fTimeStep * (this->m_vecMoveSpeed).y;
                local_24.z = leftFoot; // TODO(port): decomp _leftFoot -> leftFoot
                local_c[0] = local_c[0] + local_c[0];
                local_24.x = local_c[0] + local_18;
                local_24.y = fVar1 + fVar1 + local_14;
                if (local_10 - 0.4 <= leftFoot) { // TODO(port): decomp _leftFoot -> leftFoot
                    return;
                }
            }
            /* TODO(port): g_fx.TriggerFootSplash(local_24); - Fx_c stub lacks it */
            this->m_pedAudio.AddAudioEvent
(AE_PED_FOOTSTEP_LEFT,0.0,1.0,nullptr,SURFACE_WATER_SHALLOW
            ,0,0);
        }
        return;
}










void CPed::PlayFootSteps() {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): goto
    // TODO(convert): uint8_t-slice
        short sVar1;
        uint16_t uVar2;
        uint32_t uVar3;
        CPedStat *pCVar4;
        uint32_t uVar5;
        uint32_t uVar6;
        eMoveState eVar7;
        CPlayerPedData *pCVar8;
        uint32_t uVar9;
        long double /* Ghidra float10 */ fVar10;
        float fVar11;
        bool bVar12;
        CAnimBlendAssociation *association;
        eAdhesionGroup eVar13;
        CEventGroup *this_00; // TODO(port): decomp used CEventGlobalGroup*
        CTaskSimpleLand *this_01;
        int iVar14;
        CPedStat *pCVar15;
        uint32_t uVar16;
        CAnimBlendAssociation *pCVar17;
        CEventSoundQuiet *event;
        float fStack_50;
        float fStack_4c;
        float fStack_48;
        CVector CStack_44;
        // CEventSoundQuiet CStack_38; // TODO(port): needs default ctor; using dynamic alloc
        uint32_t uStack_4;

        uStack_4 = 0xffffffff;
        association = RpAnimBlendClumpGetFirstAssociation((RpClump *)GetRwObject());
        sVar1 = association->m_AnimId;
        pCVar17 = nullptr;
        fStack_50 = 0.0;
        fStack_4c = 0.0;
        { uint32_t u32; memcpy(&u32, &fStack_48, 4); u32 = (u32 & ~0xFFu) | 0u; memcpy(&fStack_48, &u32, 4); } // TODO(port): decomp byte-access on float
        if (((sVar1 == 0) || (sVar1 == 1)) || (sVar1 == 2)) {
            { uint32_t u32; memcpy(&u32, &fStack_48, 4); u32 = (u32 & ~0xFFu) | 1u; memcpy(&fStack_48, &u32, 4); } // TODO(port): decomp byte-access on float
        }
        pCVar4 = this->m_pStats;
        pCVar15 = CPedStats::ms_apPedStats[10]; // TODO(port): decomp byte offset 0x28
        uVar16 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((((uVar16 & 0x10000000) != 0) && (uVar5 = this->m_nDeathTimeMS, uVar5 != 0)) &&
        ((uVar5 < 300 && (this->m_nDeathTimeMS = uVar5 - 1, uVar5 - 1 == 0)))) {
            uVar16 = uVar16 & 0xefffffff;
            bIsStanding = (char)uVar16;
            bInVehicle = (char)(uVar16 >> 8);
            bFiringWeapon = (char)(uVar16 >> 0x10);
            bNotAllowedToDuck = (char)(uVar16 >> 0x18);
        }
        if ((bIsStanding & 1) == 0) {
            return;
        }
        do {
            uVar2 = association->m_Flags;
            if ((uVar2 & 0x100) == 0) {
                if ((((uVar2 & 0x400) == 0) && (association->m_AnimId != 0xdf)) &&
                ((((uint8_t)uVar2 >> 4 & 1) != 0 ||
                ((bIsDucking) == 0)))) {
                    fStack_50 = association->m_BlendAmount + fStack_50;
                }
            }
            else {
                fStack_4c = association->m_BlendAmount + fStack_4c;
                pCVar17 = association;
            }
            association = RpAnimBlendGetNextAssociation(association);
        } while (association != nullptr);
        if (((pCVar17 == nullptr) || (fStack_4c <= 0.5)) || (1.0 <= fStack_50))
        goto LAB_004072b1;
        uVar6 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        fVar11 = pCVar17->m_BlendHier->m_fTotalTime * 0.06666667;
        uVar16 = uVar6 >> 0x1a & 1;
        fStack_4c = pCVar17->m_BlendHier->m_fTotalTime * 0.5 + fVar11;
        if (uVar16 != 0) {
            fVar11 = fVar11 + 0.2;
            fStack_4c = fStack_4c + 0.2;
        }
        if (pCVar4 == pCVar15) {
            fStack_50 = 0.53333336;
            if (pCVar17->m_AnimId != 0) {
                fStack_50 = 0.33333334;
            }
            eVar13 = g_surfaceInfos.GetAdhesionGroup((eColSurfaceType)(uint8_t)this->m_nContactSurface) /* TODO(port): decomp static-style */;
            if (eVar13 == ADHESION_GROUP_LOOSE) {
                uVar16 = CGeneral::GetRandomNumber();
                if ((uVar16 & 0x7f) != 0) {
                    (this->m_vecAnimMovingShiftLocal).x = (this->m_vecAnimMovingShiftLocal).x * 0.5;
                    (this->m_vecAnimMovingShiftLocal).y = (this->m_vecAnimMovingShiftLocal).y * 0.5;
                }
                fStack_4c = 0.5;
            }
            else {
                if (eVar13 == ADHESION_GROUP_SAND) {
                    uVar16 = CGeneral::GetRandomNumber();
                    if ((uVar16 & 0x3f) != 0) {
                        (this->m_vecAnimMovingShiftLocal).x = (this->m_vecAnimMovingShiftLocal).x * 0.2;
                        (this->m_vecAnimMovingShiftLocal).y = (this->m_vecAnimMovingShiftLocal).y * 0.2;
                    }
                    goto LAB_004072b1;
                }
                if (eVar13 == ADHESION_GROUP_WET) {
                    (this->m_vecAnimMovingShiftLocal).x = (this->m_vecAnimMovingShiftLocal).x * 0.3;
                    (this->m_vecAnimMovingShiftLocal).y = (this->m_vecAnimMovingShiftLocal).y * 0.3;
                    goto LAB_004072b1;
                }
                fStack_4c = 1.0;
            }
            if ((pCVar17->m_CurrentTime <= 0.0) ||
            (fVar11 = pCVar17->m_CurrentTime - pCVar17->m_TimeStep, fVar11 < 0.0 == (fVar11 == 0.0))) {
                if ((0.2 < fStack_4c) &&
                (((fStack_50 < pCVar17->m_CurrentTime &&
                (fVar11 = pCVar17->m_CurrentTime - pCVar17->m_TimeStep,
                fVar11 < fStack_50 != (fVar11 == fStack_50))) &&
                ((this->m_pedAudio).m_bCanAddEvent != false)))) {
                    fStack_48 = 1.0;
                    if (pCVar17->m_AnimId == 0) {
                        fStack_48 = 0.75;
                    }
                    fVar10 = (long double /* Ghidra float10 */)log2((long double /* Ghidra float10 */)fStack_4c);
                    this->m_pedAudio.AddAudioEvent
(AE_PED_SKATE_RIGHT,
                    (float)((long double /* Ghidra float10 */)0.3010299956639812 * fVar10 * (long double /* Ghidra float10 */)20.0),fStack_48,
                    nullptr,SURFACE_DEFAULT,0,0);
                }
            }
            else if ((this->m_pedAudio).m_bCanAddEvent != false) {
                fStack_48 = 1.0;
                if (pCVar17->m_AnimId == 0) {
                    fStack_48 = 0.75;
                }
                fVar10 = (long double /* Ghidra float10 */)log2((long double /* Ghidra float10 */)fStack_4c);
                this->m_pedAudio.AddAudioEvent
(AE_PED_SKATE_LEFT,
                (float)((long double /* Ghidra float10 */)0.3010299956639812 * fVar10 * (long double /* Ghidra float10 */)20.0),fStack_48,
                nullptr,SURFACE_DEFAULT,0,0);
            }
            goto LAB_004072b1;
        }
        if ((fVar11 < pCVar17->m_CurrentTime == (fVar11 == pCVar17->m_CurrentTime)) ||
        (fVar11 <= pCVar17->m_CurrentTime - pCVar17->m_TimeStep)) {
            if ((pCVar17->m_CurrentTime < fStack_4c) ||
            (fStack_4c <= pCVar17->m_CurrentTime - pCVar17->m_TimeStep)) goto LAB_004072b1;
            fStack_50 = 0.0;
            if (uVar16 == 0) {
                if (this->m_nMoveState == PEDMOVE_RUN) {
                    fStack_50 = -6.0;
                    fStack_4c = 1.1;
                }
                else if (this->m_nMoveState == PEDMOVE_SPRINT) {
                    fStack_4c = 1.2;
                }
                else {
                    fStack_50 = -12.0;
                    fStack_4c = 0.9;
                }
                if (this->m_nAnimGroup == ANIM_GROUP_PLAYERSNEAK) {
                    fStack_50 = fStack_50 - 6.0;
                    fStack_4c = fStack_4c - 0.1;
                }
            }
            else {
                fStack_50 = -18.0;
                fStack_4c = 0.8;
            }
            if ((this->m_pedAudio).m_bCanAddEvent != false) {
                this->m_pedAudio.AddAudioEvent
(AE_PED_FOOTSTEP_RIGHT,fStack_50,fStack_4c,nullptr,
                SURFACE_DEFAULT,0,0);
            }
            bVar12 = false;
        }
        else {
            bVar12 = this->IsPlayer();
            if ((bVar12) && (this->m_pPlayerData != nullptr)) {
                bVar12 = this->m_pPlayerData->m_pPedClothesDesc->GetIsWearingBalaclava(); // TODO(port): decomp static-style call
                bVar12 = !bVar12;
                eVar7 = this->m_nMoveState;
                if (4 < (int)eVar7) {
                    if ((int)eVar7 < 7) {
                        pCVar8 = this->m_pPlayerData;
                        if (_unk_00858ca0 <= pCVar8->m_fMoveBlendRatio) {
                            if (bVar12) {
                                fStack_50 = 45.0;
                            }
                            else {
                                fStack_50 = 55.0;
                            }
                        }
                        else {
                            if ((bVar12) || (pCVar8->m_fMoveBlendRatio <= 1.1)) {
                                if (pCVar8->m_fMoveBlendRatio <= 1.5) goto LAB_005e5c8d;
                                fStack_50 = (pCVar8->m_fMoveBlendRatio - 1.0) * 15.0;
                            }
                            else {
                                fStack_50 = (pCVar8->m_fMoveBlendRatio - 1.0) * 20.0;
                            }
                            fStack_50 = fStack_50 + 30.0;
                            if (fStack_50 <= 0.0) goto LAB_005e5c8d;
                        }
                    }
                    else {
                        if (eVar7 != PEDMOVE_SPRINT) goto LAB_005e5c8d;
                        fStack_50 = 55.0;
                        if (!bVar12) {
                            fStack_50 = 65.0;
                        }
                    }
                    CStack_44.x = 0.0;
                    CStack_44.y = 0.0;
                    CStack_44.z = 0.0;
                    event = new CEventSoundQuiet((CEntity *)this,fStack_50,0xffffffff,CStack_44); // TODO(port): decomp used placement-new on stack
                    bVar12 = false;
                    uStack_4 = 0;
                    this_00 = GetEventGlobalGroup();
                    this_00->Add((CEvent *)event,bVar12);
                    uStack_4 = 0xffffffff;
                    delete event; event = nullptr; // TODO(port): decomp used explicit dtor on stack object
                }
            }
        LAB_005e5c8d:
            uVar9 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            fStack_4c = 0.0;
            if ((uVar9 & 0x4000000) == 0) {
                if (this->m_nMoveState == PEDMOVE_RUN) {
                    fStack_4c = -6.0;
                    fStack_50 = 1.1;
                }
                else if (this->m_nMoveState == PEDMOVE_SPRINT) {
                    fStack_50 = 1.2;
                }
                else {
                    fStack_4c = -12.0;
                    fStack_50 = 0.9;
                }
                if (this->m_nAnimGroup == ANIM_GROUP_PLAYERSNEAK) {
                    fStack_4c = fStack_4c - 6.0;
                    fStack_50 = fStack_50 - 0.1;
                }
            }
            else {
                fStack_4c = -18.0;
                fStack_50 = 0.8;
            }
            if ((this->m_pedAudio).m_bCanAddEvent != false) {
                this->m_pedAudio.AddAudioEvent
(AE_PED_FOOTSTEP_LEFT,fStack_4c,fStack_50,nullptr,
                SURFACE_DEFAULT,0,0);
            }
            bVar12 = true;
        }
        DoFootLanded(bVar12,(uint8_t)fStack_48); // TODO(port): decomp static-style call; fStack_48_int -> fStack_48
    LAB_004072b1:
        uVar3 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar3 & 0x400) != 0) {
            // TODO(port): decomp dropped GetSimplestActiveTask call; vtable+0x10 is GetTaskType() == 0xf2.
            this_01 = (CTaskSimpleLand *)this->m_pIntelligence->m_TaskMgr.GetSimplestActiveTask();
            if (this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType((eTaskType)TASK_SIMPLE_LAND) != nullptr) {
                bVar12 = this_01->RightFootLanded();
                if (bVar12) {
                    bVar12 = false;
                }
                else {
                    bVar12 = this_01->LeftFootLanded();
                    if (!bVar12) {
                        return;
                    }
                    bVar12 = true;
                }
                DoFootLanded(bVar12,1);
            }
        }
        return;
}










void CPed::AddWeaponModel(int32_t modelIndex) {    // converted from decomp src/CPed/*.c
        CWeapon *piVar1; // TODO(port): decomp used int* for struct arithmetic
        CBaseModelInfo *this_00;
        RpClump *clump;
        RwFrame *pRVar2;
        RpHAnimHierarchy *pRVar3;
        int iVar4;
        int iVar5;
        FxSystem_c *this_01;
        CVector CStack_c;

        if ((modelIndex != -1) &&
        (!this->m_aWeapons[this->m_nActiveWeaponSlot].m_DontPlaceInHand)) { // TODO(port): decomp byte arithmetic; +0x15 is m_DontPlaceInHand
            this_00 = CModelInfo::ms_modelInfoPtrs[modelIndex];
            if (this->m_pWeaponObject != nullptr) {
                RemoveWeaponModel(-1);
            }
            clump = (RpClump *)this_00->CreateInstance(); // TODO(port): decomp vtable+0x2c
            this->m_pWeaponObject = clump;
            if (clump == nullptr) {
                this->m_pGunflashObject = nullptr;
            }
            else {
                pRVar2 = CClumpModelInfo::GetFrameFromName(clump,"gunflash");
                this->m_pGunflashObject = pRVar2;
            }
            (this_00)->AddRef();
            this->m_nWeaponModelId = modelIndex;
            if ((((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) &&
            (piVar1 = &m_aWeapons[m_nActiveWeaponSlot],
            piVar1->m_Type == (eWeaponType)0x12)) && ((modelIndex == 0x158 && (piVar1->m_FxSystem == nullptr)))) { // TODO(port): decomp int arithmetic
                pRVar3 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
                iVar4 = RpHAnimIDGetIndex(pRVar3,0x18);
                RwMatrix* pFxMatrices = RpHAnimHierarchyGetMatrixArray(pRVar3); // TODO(port): decomp used int iVar5
                CStack_c.x = 0.0;
                CStack_c.y = 0.0;
                CStack_c.z = 0.0;
                this_01 = g_fxMan.CreateFxSystem("molotov_flame",CStack_c,&pFxMatrices[iVar4],false); // TODO(port): decomp static-style call
                piVar1->m_FxSystem = this_01;
                if (this_01 != nullptr) {
                    this_01->SetLocalParticles(true); // TODO(port): decomp static-style call
                    this_01->CopyParentMatrix(); // TODO(port): decomp static-style call
                    this_01->Play(); // TODO(port): decomp static-style call
                }
            }
        }
        return;
}










void CPed::TakeOffGoggles() {    // converted from decomp src/CPed/*.c
        eWeaponType *peVar1;
        eWeaponType weaponType;
        int modelIndex;
        CWeaponInfo *pCVar2;

        pCVar2 = CWeaponInfo::GetWeaponInfo(WEAPON_INFRARED, eWeaponSkill::STD);
        peVar1 = &this->m_aWeapons[(size_t)pCVar2->m_nSlot].m_Type; // TODO(port): decomp byte arithmetic on array
        if ((peVar1 != nullptr) &&
        ((weaponType = *peVar1, weaponType == WEAPON_INFRARED || (weaponType == WEAPON_NIGHTVISION))))
        {
            pCVar2 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
            modelIndex = pCVar2->m_nModelId1;
            RemoveGogglesModel(); // TODO(port): decomp static-style call
            // TODO(port): decomp wrote byte at weapon+0x15; that is CWeapon::m_DontPlaceInHand.
            reinterpret_cast<CWeapon*>(peVar1)->m_DontPlaceInHand = false;
            if (peVar1 == &this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type) { // TODO(port): decomp byte arithmetic
                AddWeaponModel(modelIndex); // TODO(port): decomp static-style call
            }
        }
        return;
}










eWeaponSlot CPed::GiveWeapon(eWeaponType weaponType,  uint32_t ammo,  bool likeUnused) {    // converted from decomp src/CPed/GiveWeapon_005e6080.c; cross-checked vs gta-reversed Entity/Ped/Ped.cpp
    const CWeaponInfo* givenWepInfo = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
    const eWeaponSlot wepSlot = givenWepInfo->m_nSlot;
    CWeapon& wepInSlot = GetWeaponInSlot(wepSlot);

    if (wepInSlot.m_Type != weaponType) {
        // Another weapon occupies the slot: remove it, then set the new one.
        if (wepInSlot.m_Type != WEAPON_UNARMED) {
            // Shotgun/SMG/Rifle ammo carries over to the replacement weapon.
            switch (wepSlot) {
            case eWeaponSlot::SHOTGUN:
            case eWeaponSlot::SMG:
            case eWeaponSlot::RIFLE:
                ammo += wepInSlot.m_TotalAmmo;
                break;
            default:
                break;
            }
            RemoveWeaponModel(wepInSlot.GetWeaponInfo().m_nModelId1);
            if (givenWepInfo->m_nSlot == CWeaponInfo::GetWeaponInfo(WEAPON_INFRARED, eWeaponSkill::STD)->m_nSlot) {
                RemoveGogglesModel();
            }
            wepInSlot.Shutdown();
        }
        wepInSlot.Initialise(weaponType, ammo, this);
        if (givenWepInfo->m_nSlot == static_cast<eWeaponSlot>(m_nActiveWeaponSlot) && !bInVehicle) {
            AddWeaponModel(givenWepInfo->m_nModelId1);
        }
    } else {
        // Same weapon already in the slot: top up ammo and reload.
        if (wepSlot == eWeaponSlot::GIFT) { // Gifts have no ammo
            return eWeaponSlot::GIFT;
        }
        wepInSlot.m_TotalAmmo = std::min(99999u, wepInSlot.m_TotalAmmo + ammo);
        wepInSlot.Reload(this);
        if (wepInSlot.m_State == WEAPONSTATE_OUT_OF_AMMO && wepInSlot.m_TotalAmmo > 0) {
            wepInSlot.m_State = WEAPONSTATE_READY;
        }
    }
    if (wepInSlot.m_State != WEAPONSTATE_OUT_OF_AMMO) {
        wepInSlot.m_State = WEAPONSTATE_READY;
    }
    return wepSlot;
}












void CPed::GiveWeaponSet1() {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone; body from gta-reversed Ped.cpp.
    GiveWeapon(WEAPON_BRASSKNUCKLE, 1, true);
    GiveWeapon(WEAPON_BASEBALLBAT, 1, true);
    GiveWeapon(WEAPON_MOLOTOV, 10, true);
    GiveWeapon(WEAPON_PISTOL, 100, true);
    GiveWeapon(WEAPON_SHOTGUN, 50, true);
    GiveWeapon(WEAPON_MICRO_UZI, 150, true);
    GiveWeapon(WEAPON_AK47, 120, true);
    GiveWeapon(WEAPON_COUNTRYRIFLE, 25, true);
    GiveWeapon(WEAPON_RLAUNCHER, 200, true);
    GiveWeapon(WEAPON_SPRAYCAN, 200, true);
}












void CPed::GiveWeaponSet2() {    // no decomp; adapted from gta-reversed
    GiveWeapon(WEAPON_KNIFE, 0, true);
    GiveWeapon(WEAPON_GRENADE, 10, true);
    GiveWeapon(WEAPON_DESERT_EAGLE, 40, true);
    GiveWeapon(WEAPON_SAWNOFF_SHOTGUN, 40, true);
    GiveWeapon(WEAPON_TEC9, 150, true);
    GiveWeapon(WEAPON_M4, 150, true);
    GiveWeapon(WEAPON_SNIPERRIFLE, 21, true);
    GiveWeapon(WEAPON_FLAMETHROWER, 500, true);
    GiveWeapon(WEAPON_EXTINGUISHER, 200, true);
}












void CPed::GiveWeaponSet3() {    // no decomp; adapted from gta-reversed
    GiveWeapon(WEAPON_CHAINSAW, 0, true);
    GiveWeapon(WEAPON_REMOTE_SATCHEL_CHARGE, 5, true);
    GiveWeapon(WEAPON_PISTOL_SILENCED, 40, true);
    GiveWeapon(WEAPON_SPAS12_SHOTGUN, 30, true);
    GiveWeapon(WEAPON_MP5, 100, true);
    GiveWeapon(WEAPON_M4, 150, true);
    GiveWeapon(WEAPON_RLAUNCHER_HS, 200, true);
}












void CPed::GiveWeaponSet4() {    // no decomp; adapted from gta-reversed
    GiveWeapon(WEAPON_MINIGUN, 500, true);
    GiveWeapon(WEAPON_DILDO2, 0, true);
}












void CPed::SetCurrentWeapon(int32_t slot) {    // converted from decomp src/CPed/SetCurrentWeapon_005e61f0.c
        CWeaponInfo *pCVar1;

        if (slot != -1) {
            if (this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type !=
            WEAPON_UNARMED) {
                pCVar1 = CWeaponInfo::GetWeaponInfo
                (*(eWeaponType *)
                &this->m_aWeapons[this->m_nActiveWeaponSlot], eWeaponSkill::STD);
                RemoveWeaponModel(pCVar1->m_nModelId1);
            }
            this->m_nActiveWeaponSlot = (uint8_t)slot;
            if (this->m_pPlayerData != nullptr) {
                this->m_pPlayerData->m_nChosenWeapon = (uint8_t)slot;
            }
            if ((int)this->m_aWeapons[slot].m_Type != 0) {
                pCVar1 = CWeaponInfo::GetWeaponInfo
                (*(eWeaponType *)
                &this->m_aWeapons[this->m_nActiveWeaponSlot], eWeaponSkill::STD);
                AddWeaponModel(pCVar1->m_nModelId1);
            }
        }
        return;
}












void CPed::SetCurrentWeapon(eWeaponType weaponType) {    // converted from decomp src/CPed/SetCurrentWeapon_005e6280.c
        CWeaponInfo *pCVar1;

        pCVar1 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD); // TODO(port): decomp eWeaponSkill::STD enum
        this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
        return;
}












void CPed::ClearWeapon(eWeaponType weaponType) {    // converted from decomp src/CPed/*.c
        int iVar1;
        CWeaponInfo *pCVar2;

        pCVar2 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
        iVar1 = (int32_t)pCVar2->m_nSlot; // TODO(port): m_nSlot is eWeaponSlot
        if ((iVar1 != -1) && ((&m_aWeapons[iVar1])->m_Type == weaponType)) {
            if ((char)this->m_nActiveWeaponSlot == iVar1) {
                pCVar2 = CWeaponInfo::GetWeaponInfo(WEAPON_UNARMED, eWeaponSkill::STD);
                this->SetCurrentWeapon((int32_t)pCVar2->m_nSlot); // TODO(port): decomp static-style call
            }
            m_aWeapons[iVar1].Shutdown(); // TODO(port): decomp static-style call
            if ((weaponType == WEAPON_NIGHTVISION) || (weaponType == WEAPON_INFRARED)) {
                RemoveGogglesModel(); // TODO(port): decomp static-style call
            }
        }
        return;
}










void CPed::ClearWeapons() {    // converted from decomp src/CPed/*.c
        CWeaponInfo *pCVar1;
        int iVar2;
        CWeapon *this_00;

        RemoveWeaponModel(-1);
        RemoveGogglesModel(); // TODO(port): decomp static-style call
        this_00 = this->m_aWeapons.data(); // TODO(port): decomp cast array to pointer
        iVar2 = 0xd;
        do {
            this_00->Shutdown(); // TODO(port): decomp static-style call
            this_00 = this_00 + 1;
            iVar2 = iVar2 + -1;
        } while (iVar2 != 0);
        pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_UNARMED, eWeaponSkill::STD);
        this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
        return;
}










void CPed::RemoveWeaponWhenEnteringVehicle(int32_t arg0) {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
        CPlayerInfo *pCVar2;
        int iVar3;
        CWeaponInfo *pCVar4;

        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_nPlayerFlags |= 0x800;
        }
        if (this->m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
            return;
        }
        if (((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) ||
        (pCVar2 = ((CPlayerPed *)this)->GetPlayerInfoForThisPlayerPed(), /* TODO(port): decomp static-style call */
        pCVar2->m_bCanDoDriveBy == false)) goto LAB_005e6461;
        iVar3 = (int)this->m_aWeapons[4].m_Type;
        if ((iVar3 == 0x1c) || (iVar3 == 0x20)) {
        LAB_005e63d7:
            if (0 < (int)this->m_aWeapons[4].m_TotalAmmo) {
                iVar3 = 4;
                goto LAB_005e641d;
            }
        LAB_005e63e8:
            if (arg0 != 1) goto LAB_005e6461;
        }
        else if (arg0 != 1) {
            if (iVar3 == 0x1d) goto LAB_005e63d7;
            goto LAB_005e63e8;
        }
        if ((((int)this->m_aWeapons[3].m_Type == 0x1a) && (0 < (int)this->m_aWeapons[3].m_TotalAmmo)) ||
        ((arg0 == 1 &&
        (((int)this->m_aWeapons[2].m_Type == 0x16 && (0 < (int)this->m_aWeapons[2].m_TotalAmmo)))))) {
            iVar3 = 2;
        LAB_005e641d:
            if (this->m_nSavedWeapon == WEAPON_UNIDENTIFIED) {
                this->m_nSavedWeapon =
                this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type;
            }
            pCVar4 = CWeaponInfo::GetWeaponInfo(m_aWeapons[iVar3].m_Type, eWeaponSkill::STD);
            this->SetCurrentWeapon((int32_t)pCVar4->m_nSlot); // TODO(port): decomp static-style call
            return;
        }
    LAB_005e6461:
        pCVar4 = CWeaponInfo::GetWeaponInfo
        (this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type,
        eWeaponSkill::STD);
        RemoveWeaponModel(pCVar4->m_nModelId1);
        return;
}










void CPed::ReplaceWeaponWhenExitingVehicle() {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
        CWeaponInfo *pCVar2;

        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_nPlayerFlags &= 0xfffff7ff;
        }
        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            pCVar2 = CWeaponInfo::GetWeaponInfo
            (this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type,
            eWeaponSkill::STD);
            AddWeaponModel(pCVar2->m_nModelId1);
            return;
        }
        if (this->m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
            pCVar2 = CWeaponInfo::GetWeaponInfo(this->m_nSavedWeapon, eWeaponSkill::STD);
            this->SetCurrentWeapon((int32_t)pCVar2->m_nSlot); // TODO(port): decomp static-style call
            this->m_nSavedWeapon = WEAPON_UNIDENTIFIED;
            return;
        }
        pCVar2 = CWeaponInfo::GetWeaponInfo
        (this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type,
        eWeaponSkill::STD);
        AddWeaponModel(pCVar2->m_nModelId1);
        return;
}










void CPed::ReplaceWeaponForScriptedCutscene() {    // converted from decomp src/CPed/*.c
        this->m_nSavedWeapon = this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type
        ;
        this->SetCurrentWeapon(0); // TODO(port): decomp static-style call
        return;
}










void CPed::RemoveWeaponForScriptedCutscene() {    // converted from decomp src/CPed/*.c
        CWeaponInfo *pCVar1;

        if (this->m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
            pCVar1 = CWeaponInfo::GetWeaponInfo(this->m_nSavedWeapon, eWeaponSkill::STD);
            this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
            this->m_nSavedWeapon = WEAPON_UNIDENTIFIED;
        }
        return;
}










eWeaponSkill CPed::GetWeaponSkill() {    // converted from decomp src/CPed/GetWeaponSkill_005e6580.c
        eWeaponSkill eVar1;

        eVar1 = GetWeaponSkill(this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type); // TODO(port): decomp static-style call
        return eVar1;
}












void CPed::PreRenderAfterTest() {    // converted from decomp src/CPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        CTaskManager *this_00;
        uint32_t uVar2;
        uint8_t bVar3;
        uint8_t uVar18;
        uint32_t uVar4;
        eVehicleType eVar5;
        void *pvVar6;
        uint32_t uVar7;
        uint32_t uVar8;
        CVehicle *pCVar9;
        CMatrixLink *pCVar10;
        uint8_t uVar19;
        uint32_t uVar11;
        CPedModelInfo *this_01;
        CColModel *pCVar12;
        ePedState eVar13;
        CCollisionData *pCVar14;
        CColSphere *pCVar15;
        CPlayerPedData *pCVar16;
        uint32_t uVar17;
        bool bVar20;
        char cVar21;
        bool bVar22;
        CTaskSimpleSwim *pCVar23;
        CTaskSimpleInAir *pCVar24;
        RwFrame *pRVar25;
        CTask *pCVar26;
        int iVar27;
        float *pfVar28;
        RpHAnimHierarchy *pRVar29;
        uint32_t uVar30;
        int iVar31;
        CTaskSimpleJetPack *pCVar32;
        uint32_t *puVar33;
        CVector *pCVar34;
        CAnimBlendAssociation *pCVar35;
        CSimpleTransform *pCVar36;
        CTask *pCVar37;
        bool bVar38;
        long double /* Ghidra float10 */ fVar39;
        float fVar40;
        CPed *pCVar41;
        float fStack_6c;
        float fStack_68;
        float fStack_64;
        float fStack_60;
        float fStack_5c;
        uint32_t uStack_58;
        uint32_t uStack_54;
        uint32_t uStack_50;
        float fStack_4c;
        float fStack_48;
        float fStack_44;
        CVector CStack_40;
        CVector CStack_34;
        CVector CStack_28;
        FxPrtMult_c FStack_1c;

        pCVar23 = m_pIntelligence->GetTaskSwim();
        if (pCVar23 == nullptr) {
            pCVar32 = this->m_pIntelligence->GetTaskJetPack();
            if (pCVar32 != nullptr) {
                pCVar41 = this;
                pCVar32 = this->m_pIntelligence->GetTaskJetPack();
                /* TODO(port): pCVar32->ApplyRollAndPitch(pCVar41); - CTaskSimpleJetPack stub lacks it */
                goto LAB_005e65ee;
            }
        }
        else {
            pCVar41 = this;
            pCVar23 = m_pIntelligence->GetTaskSwim();
            /* TODO(port): pCVar23->ApplyRollAndPitch(pCVar41); - CTaskSimpleSwim stub lacks it */
        LAB_005e65ee:
            ; // null statement (label requires a statement)
            // TODO(port): CPedIK opaque (see CPed.h); original: m_pedIK.m_nFlags &= ~8.
            // /* pCVar1->m_nFlags */ 0 = /* pCVar1->m_nFlags */ 0 & 0xfffffff7;
        }
        pCVar24 = this->m_pIntelligence->GetTaskInAir() /* TODO(port): decomp static-style call */;
        if (pCVar24 == nullptr) {
            if (/* TODO(port): CPedIK opaque; original checked (m_pedIK.m_nFlags>>3)&1 */ false ||
            (((this->m_nPedType != PED_TYPE_PLAYER1 && (this->m_nPedType != PED_TYPE_PLAYER2)) &&
            ((this->m_pedIK).m_fSlopePitch != 0.0)))) {
                /* TODO(port): CPedIK::PitchForSlope(&this->m_pedIK); - CPedIK opaque */
            }
        }
        else {
            pCVar41 = this;
            this->m_pIntelligence->GetTaskInAir() /* TODO(port): decomp static-style call */;
            // TODO(port): CPedIK opaque (see CPed.h); original: m_pedIK.m_nFlags &= ~8.
            // /* pCVar1->m_nFlags */ 0 = /* pCVar1->m_nFlags */ 0 & 0xfffffff7;
        }
        uVar2 = (bDonePositionOutOfCollision ? 1u : 0u) | (bKilledByStealth ? 0x100u : 0u) | (bRightArmBlocked ? 0x10000u : 0u) | (bWaitingForScriptBrainToLoad ? 0x1000000u : 0u);
        uVar2 = uVar2 | 0x400;
        bDonePositionOutOfCollision = (char)uVar2;
        bKilledByStealth = (char)(uVar2 >> 8);
        bRightArmBlocked = (char)(uVar2 >> 0x10);
        bWaitingForScriptBrainToLoad = (char)(uVar2 >> 0x18);
        this->UpdateRpHAnim(); // TODO(port): decomp static-style call
        if (((CTimer::bSkipProcessThisFrame == false) && (this->m_pWeaponObject != nullptr)) &&
        ((this->m_pPlayerData != nullptr &&
        ((m_aWeapons[m_nActiveWeaponSlot].m_Type == (eWeaponType)0x26 && // TODO(port): decomp int comparison
        (pRVar25 = CClumpModelInfo::GetFrameFromName(this->m_pWeaponObject,"minigun2"),
        pRVar25 != nullptr)))))) {
            /* TODO(port): RwMatrixRotate(&pRVar25->modelling,&DAT_008d232c,
            CTimer::ms_fTimeStep * this->m_pPlayerData->m_fGunSpinSpeed * 57.295776,1); - signature mismatch */
        }
        if (((char)GetUsesCollision() < '\0') &&
        (true /* TODO(port): CTimeCycle::m_CurrentColours.m_nShadowStrength != 0 */)) {
            uVar30 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            pCVar37 = nullptr;
            bVar20 = false;
            bVar22 = false;
            this_00 = &this->m_pIntelligence->m_TaskMgr;
            if ((uVar30 & 0x100) == 0) {
                pCVar37 = this_00->FindActiveTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER);
                if (pCVar37 != nullptr) {
                    bVar20 = true;
                }
            }
            else {
                pCVar26 = this_00->FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR);
                if (pCVar26 == nullptr) {
                    pCVar26 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_DRAG_PED_FROM_CAR);
                    if (pCVar26 != nullptr) {
                        bVar22 = true;
                    }
                }
                else {
                    bVar22 = true;
                }
            }
            iVar27 = 2; /* TODO(port): g_fx.GetFxQuality(); - Fx_c stub lacks it */
            if ((iVar27 == 3) ||
            ((iVar27 = 2 /* TODO(port): g_fx.GetFxQuality(); - Fx_c stub lacks it */, iVar27 == 2 && (this->m_nPedType == PED_TYPE_PLAYER1)))) {
                bVar38 = true;
            }
            else {
                bVar38 = false;
            }
            if (bVar38) {
                GetBonePosition(&CStack_34,BONE_ROOT,false) /* TODO(port): decomp static-style call */;
                if (DAT_00b6f03c == 0) {
                    pfVar28 = (float *)&DAT_00b6f02c;
                }
                else {
                    pfVar28 = (float *)(DAT_00b6f03c + 0x30);
                }
                fVar40 = (CStack_34.x - *pfVar28) * (CStack_34.x - *pfVar28) +
                (CStack_34.y - pfVar28[1]) * (CStack_34.y - pfVar28[1]);
                if ((fVar40 < _MAX_DISTANCE_PED_SHADOWS_SQR == (fVar40 == _MAX_DISTANCE_PED_SHADOWS_SQR)) ||
                (bVar38 = true, (m_nPhysicalFlags & 0x100) != 0)) goto LAB_005e6908;
                uVar4 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
                if ((uVar4 & 0x100) != 0) {
                    bVar38 = false;
                    if ((this->m_pVehicle != nullptr) &&
                    (((eVar5 = this->m_pVehicle->m_nVehicleSubType, eVar5 == VEHICLE_TYPE_BMX ||
                    (eVar5 == VEHICLE_TYPE_BIKE)) || (eVar5 == VEHICLE_TYPE_QUAD)))) {
                        bVar38 = true;
                    }
                }
                if (bVar20) {
                    bVar38 = false;
                    // TODO(port): decomp `pCVar37[1].m_Parent` reads the enter-car task's
                    // target vehicle (CTaskComplexEnterCar::m_Car); `[0x77].vtable` compared
                    // m_nVehicleSubType (Ghidra artifact; 0xa/0x9/0x2 = BMX/BIKE/QUAD).
                    // Offset 0xC = CTask(0x8) + m_pSubTask(0x4) per gta-reversed layout.
                    // Replace with real member access when the task batch lands.
                    CVehicle* pTargetCar = *reinterpret_cast<CVehicle**>(reinterpret_cast<uint8_t*>(pCVar37) + 0xC);
                    if ((pTargetCar != nullptr) &&
                    (((pTargetCar->m_nVehicleSubType == VEHICLE_TYPE_BMX ||
                    (pTargetCar->m_nVehicleSubType == VEHICLE_TYPE_BIKE)) || (pTargetCar->m_nVehicleSubType == VEHICLE_TYPE_QUAD)))) {
                        bVar38 = true;
                    }
                }
                GetBonePosition(&CStack_28,BONE_SPINE1,false) /* TODO(port): decomp static-style call */;
                bVar20 = IsAlive(); // TODO(port): decomp static-style call
                if (bVar20) {
                    if (this->m_matrix == nullptr) {
                        pCVar36 = &this->m_placement;
                    }
                    else {
                        pCVar36 = (CSimpleTransform *)&this->m_matrix->m_pos;
                    }
                    if (CStack_28.z < (pCVar36->m_vPosn).z - 0.2) goto LAB_005e6892;
                    bVar38 = !bVar38;
                }
                else {
                LAB_005e6892:
                    uVar7 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
                    bVar38 = (uVar7 & 0x4000000) == 0;
                }
                if ((bVar38) ||
                (false /* TODO(port): g_realTimeShadowMan.DoShadowThisFrame - not ported */, 
                this->m_pShadowData != nullptr)) goto LAB_005e6908;
            }
            uVar8 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            if (((uVar8 & 0x100) == 0) || (bVar22)) {
                CShadows::StoreShadowForPedObject
                (this,0.0f /* TODO(port): CTimeCycle not ported */,
                0.0f /* TODO(port): CTimeCycle not ported */,
                0.0f /* TODO(port): CTimeCycle not ported */,
                0.0f /* TODO(port): CTimeCycle not ported */,
                0.0f /* TODO(port): CTimeCycle not ported */,
                0.0f /* TODO(port): CTimeCycle not ported */);
            }
        }
    LAB_005e6908:
        if (this->m_nModelIndex == 0) {
            ShoulderBoneRotation((RpClump *)GetRwObject());
            m_nFlags = m_nFlags | 0x800000;
        }
        pRVar29 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
        bVar20 = false;
        bVar22 = false;
        fStack_68 = 1.0;
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            if (this->m_matrix == nullptr) {
                pCVar34 = &(this->m_placement).m_vPosn;
            }
            else {
                pCVar34 = &this->m_matrix->m_pos;
            }
            bVar38 = false; /* TODO(port): CWindModifiers::FindWindModifier(*pCVar34,&fStack_68,&fStack_68); - not ported */
            if ((bVar38) && (bVar38 = CCullZones::PlayerNoRain(), !bVar38)) {
                bVar22 = true;
            }
        }
        if (((this->m_nPedState == PEDSTATE_DRIVING) &&
        (pCVar9 = this->m_pVehicle, pCVar9 != nullptr)) &&
        ((pCVar9->m_nVehicleType == VEHICLE_TYPE_BIKE ||
        ((pCVar9->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE &&
        (cVar21 = 0 /* TODO(port): vtable+0x9c call - not ported */, cVar21 != '\0')))))) {
            bVar20 = true;
        }
        if (((this->m_pPlayerData == nullptr) ||
        ((uVar2 = *(uint32_t *)this->m_pPlayerData->m_pPedClothesDesc->m_anModelKeys,
        uVar30 = 0 /* TODO(port): CKeyGen::GetUppercaseKey(\"vest\") - not ported */, uVar2 != uVar30 &&
        (uVar2 = *(uint32_t *)this->m_pPlayerData->m_pPedClothesDesc->m_anModelKeys,
        uVar30 = 0 /* TODO(port): CKeyGen::GetUppercaseKey(\"torso\") - not ported */, uVar2 != uVar30)))) && ((bVar22 || (bVar20)))) {
            fStack_6c = 0.0;
            if (bVar20) {
                pCVar9 = this->m_pVehicle;
                pCVar10 = pCVar9->m_matrix;
                fStack_6c = (pCVar9->m_vecMoveSpeed).x * (pCVar10->m_forward).x +
                (pCVar9->m_vecMoveSpeed).y * (pCVar10->m_forward).y +
                (pCVar9->m_vecMoveSpeed).z * (pCVar10->m_forward).z;
            }
            if ((bVar22) && (fStack_6c < std::abs(fStack_68 - 1.0))) {
                fStack_6c = std::abs(fStack_68 - 1.0);
            }
            fStack_5c = _DAT_008d1378 * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d1378 * fStack_6c;
            iVar27 = CGeneral::GetRandomNumber();
            fStack_4c = (fStack_5c - fStack_64) * (float)iVar27 * 3.051851e-05 + fStack_64;
            fStack_60 = _DAT_008d1378 * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d1378 * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_48 = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            fStack_60 = _DAT_008d1378 * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d1378 * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_44 = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            iVar27 = RpHAnimIDGetIndex(pRVar29,4);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29);
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */
            (void)iVar31; (void)iVar27;
            fStack_60 = _DAT_008d137c * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d137c * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_4c = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            fStack_60 = _DAT_008d137c * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d137c * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_48 = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            fStack_60 = _DAT_008d137c * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d137c * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_44 = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x1f);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29);
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */
            (void)iVar31; (void)iVar27;
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x15);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29);
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */
            (void)iVar31; (void)iVar27;
            if ((bVar20) ||
            (pCVar32 = this->m_pIntelligence->GetTaskJetPack(),
            pCVar32 == nullptr)) {
                iVar27 = RpHAnimIDGetIndex(pRVar29,3);
                iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29);
                /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */
                (void)iVar31; (void)iVar27;
            }
            fStack_60 = _DAT_008d1380 * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d1380 * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_4c = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            fStack_60 = _DAT_008d1380 * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d1380 * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_48 = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            fStack_60 = _DAT_008d1380 * fStack_6c + 1.0;
            fStack_64 = 1.0 - _DAT_008d1380 * fStack_6c;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            fStack_44 = (fStack_60 - fStack_64) * (float)(int)fStack_5c * 3.051851e-05 + fStack_64;
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x20);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29);
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */
            (void)iVar31; (void)iVar27;
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x16);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29);
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */
            (void)iVar31; (void)iVar27;
        }
        uVar18 = bInVehicle;
        if (((char)uVar18 < '\0') && (this->m_nBodypartToRemove == '\x02')) {
            iVar27 = RpHAnimIDGetIndex(pRVar29,5);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            CStack_40.x = 0.0;
            CStack_40.y = 0.0;
            CStack_40.z = 0.0;
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */

            (void)iVar31;
            iVar27 = RpHAnimIDGetIndex(pRVar29,8);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */

            (void)iVar31;
            iVar27 = RpHAnimIDGetIndex(pRVar29,6);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */

            (void)iVar31;
            iVar27 = RpHAnimIDGetIndex(pRVar29,7);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixScale not ported; original scaled bone matrix */

            (void)iVar31;
        }
        uStack_58 = 0;
        uStack_54 = 0;
        uStack_50 = 0x3f800000;
        if (0.0 < this->m_Wobble) {
            fVar39 = (long double /* Ghidra float10 */)sinf((long double /* Ghidra float10 */)this->m_Wobble);
            fVar40 = (float)-(fVar39 * (long double /* Ghidra float10 */)_DAT_008d21f0);
            this->m_Wobble = this->m_Wobble - CTimer::ms_fTimeStep * this->m_WobbleSpeed;
            fStack_5c = fVar40;
            if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
                iVar27 = RpHAnimIDGetIndex(pRVar29,0x12e);
                iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
                /* TODO(port): RwMatrixRotate not ported */
                iVar27 = RpHAnimIDGetIndex(pRVar29,0x12d);
                iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
                /* TODO(port): RwMatrixRotate not ported */
            }
            iVar27 = RpHAnimIDGetIndex(pRVar29,0xc9);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
        }
        if (0.0 < CWeather::Earthquake) {
            fStack_64 = -CWeather::Earthquake;
            fStack_60 = CWeather::Earthquake;
            iVar27 = CGeneral::GetRandomNumber();
            fStack_5c = ((fStack_60 - fStack_64) * (float)iVar27 * 3.051851e-05 + fStack_64) * 0.0025;
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x2a);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            fVar40 = fStack_5c;
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x34);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x21);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x17);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x20);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x16);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x2b);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x35);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x22);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,0x18);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
            iVar27 = RpHAnimIDGetIndex(pRVar29,5);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            /* TODO(port): RwMatrixRotate not ported */
        }
        uVar19 = bInVehicle;
        if (((((char)uVar19 < '\0') && (this->m_nBodypartToRemove == '\x02')) &&
        (this->m_nPedState != PEDSTATE_DEAD)) &&
        (((this->GetFourthPedFlags() & 0x20) == 0 &&
        (3 < ((uint8_t)CTimer::m_FrameCounter & 7))))) {
            pRVar29 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
            iVar27 = RpHAnimIDGetIndex(pRVar29,5);
            iVar31 = (int)RpHAnimHierarchyGetMatrixArray(pRVar29); // TODO(port): decomp int arithmetic
            iVar31 = iVar31 + iVar27 * 0x40;
            CStack_34.x = *(float *)(iVar31 + 0x30);
            CStack_34.y = *(float *)(iVar31 + 0x34);
            CStack_34.z = *(float *)(iVar31 + 0x38);
            /* TODO(port): CVector::unk_0040fe90(&CStack_28,0x3f19999a,&this->m_matrix->m_up); - unknown */
            /* TODO(port): g_fx.AddBlood(&CStack_34,&CStack_28,0x10,this->m_fContactSurfaceBrightness); - Fx_c stub lacks it */
        }
        if (((CWeather::Rain <= 0.3) || (_DAT_00b6f180 <= 15.0)) ||
        (((bInVehicle) != 0 ||
        (CGame::currArea != 0)))) goto LAB_005e74f3;
        if (this->m_matrix == nullptr) {
            pCVar36 = &this->m_placement;
        }
        else {
            pCVar36 = (CSimpleTransform *)&this->m_matrix->m_pos;
        }
        if ((900.0 <= (pCVar36->m_vPosn).z) || (bVar20 = CCullZones::CamNoRain(), bVar20))
        goto LAB_005e74f3;
        pCVar36 = (CSimpleTransform *)&this->m_matrix->m_pos;
        if (this->m_matrix == nullptr) {
            pCVar36 = &this->m_placement;
        }
        if (DAT_00b6f03c == 0) {
            puVar33 = (uint32_t*)&DAT_00b6f02c; // TODO(port): decomp type pun
        }
        else {
            puVar33 = (uint32_t *)(DAT_00b6f03c + 0x30);
        }
        pCVar34 = (CVector *)VectorSub(&CStack_28,puVar33,pCVar36);
        fVar40 = pCVar34->Magnitude(); // TODO(port): decomp static-style call
        if (25.0 <= fVar40) goto LAB_005e74f3;
        this_01 = (CPedModelInfo *)CModelInfo::ms_modelInfoPtrs[(short)this->m_nModelIndex];
        this_01->AnimatePedColModelSkinnedWorld((RpClump *)GetRwObject());
        pCVar12 = this_01->m_pHitColModel;
        bVar20 = true;
        { static CVector s_vec34; s_vec34 = FindPlayerSpeed(-1); pCVar34 = &s_vec34; } // TODO(port): decomp type mismatch
        fStack_4c = pCVar34->x;
        fStack_48 = pCVar34->y;
        fStack_44 = pCVar34->z;
        fVar40 = fStack_4c;
        if (fStack_4c < 0.0) {
            fVar40 = -fStack_4c;
        }
        if (0.05 < fVar40) {
        LAB_005e73b3:
            bVar20 = false;
        }
        else {
            fVar40 = fStack_48;
            if (fStack_48 < 0.0) {
                fVar40 = -fStack_48;
            }
            if ((((0.05 < fVar40) || (eVar13 = this->m_nPedState, eVar13 == PEDSTATE_FALL)) ||
            (eVar13 == PEDSTATE_DIE)) ||
            (((eVar13 == PEDSTATE_DEAD || (eVar13 == PEDSTATE_ATTACK)) ||
            ((eVar13 == PEDSTATE_FIGHT ||
            ((bVar22 = this->IsPedHeadAbovePos(0.3), !bVar22 ||
            (pCVar35 = RpAnimBlendClumpGetAssociation((RpClump *)GetRwObject(),10),
            pCVar35 != nullptr)))))))) goto LAB_005e73b3;
        }
        pCVar14 = pCVar12->m_pColData;
        fVar40 = (float)(int)(short)pCVar14->m_nNumSpheres;
        if ((bVar20) && (0 < (int)fVar40)) {
            iVar27 = 0;
            fStack_64 = fVar40;
            do {
                pCVar15 = pCVar14->m_pSpheres;
                bVar3 = (&(pCVar15->m_Surface).m_nPiece)[iVar27];
                pfVar28 = (float *)((int)&(pCVar15->m_vecCenter).x + iVar27);
                if ((4 < bVar3) && ((bVar3 < 7 || (bVar3 == 9)))) {
                    FxPrtMult_c::FxPrtMult_c
                    (0x3f800000,0x3f800000,0x3f800000,0x3eb33333,0x3c23d70a,0,0x3cf5c28f);
                    CStack_40.x = *pfVar28;
                    CStack_40.y = pfVar28[1];
                    CStack_40.z = pfVar28[2];
                    fVar39 = (long double /* Ghidra float10 */)CGeneral::GetRandomNumberInRange(0xbda3d70a,0x3da3d70a);
                    CStack_40.x = (float)(fVar39 + (long double /* Ghidra float10 */)CStack_40.x);
                    fVar39 = (long double /* Ghidra float10 */)CGeneral::GetRandomNumberInRange(0xbda3d70a,0x3da3d70a);
                    CStack_40.y = (float)(fVar39 + (long double /* Ghidra float10 */)CStack_40.y);
                    CStack_40.z = pfVar28[3] * 0.75 + CStack_40.z;
                    CStack_34.x = fStack_4c * 50.0;
                    CStack_34.y = fStack_48 * 50.0;
                    CStack_34.z = fStack_44 * 50.0;
                    CStack_28.x = CStack_34.x;
                    CStack_28.y = CStack_34.y;
                    CStack_28.z = CStack_34.z;
                    FxSystem_c::AddParticle
                    (DAT_00a9ae30,&CStack_40,&CStack_28,0.0,&FStack_1c,-1.0,1.2,0.6,false);
                }
                iVar27 = iVar27 + 0x14;
                fStack_64 = (float)((int)fStack_64 + -1);
            } while (fStack_64 != 0.0);
        }
    LAB_005e74f3:
        pCVar16 = this->m_pPlayerData;
        if (((pCVar16 != nullptr) && ('\0' < (char)pCVar16->m_nWetness)) &&
        (pCVar16->m_nWaterCoverPerc < 0x1e)) {
            FxPrtMult_c::FxPrtMult_c(0x3f800000,0x3f800000,0x3f800000,0x3e4ccccd,0x3e19999a,0,0x3dcccccd);
            if (this->m_matrix == nullptr) {
                pCVar36 = &this->m_placement;
            }
            else {
                pCVar36 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            CStack_40.x = (pCVar36->m_vPosn).x;
            CStack_40.y = (pCVar36->m_vPosn).y;
            CStack_40.z = (pCVar36->m_vPosn).z;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            CStack_40.x = ((float)(int)fStack_5c * 3.051851e-05 * 0.6 + CStack_40.x) - 0.3;
            fStack_5c = (float)CGeneral::GetRandomNumber();
            CStack_40.y = ((float)(int)fStack_5c * 3.051851e-05 * 0.6 + CStack_40.y) - 0.3;
            iVar27 = CGeneral::GetRandomNumber();
            CStack_28.x = 0.0;
            CStack_28.y = 0.0;
            CStack_28.z = 0.0;
            CStack_40.z = ((float)iVar27 * 3.051851e-05 * 1.0 + CStack_40.z) - 0.8;
            fStack_5c = (float)(int)(char)this->m_pPlayerData->m_nWetness;
            FStack_1c.m_Color.alpha = (float)(int)fStack_5c * FStack_1c.m_Color.alpha * 0.01;
            FxSystem_c::AddParticle(DAT_00a9ae38,&CStack_40,&CStack_28,0.0,&FStack_1c,-1.0,1.2,0.6,false);
        }
        uVar17 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if (((uVar17 & 0x100) != 0) && (this->m_pVehicle != nullptr)) {
            this->m_fContactSurfaceBrightness = this->m_pVehicle->m_fContactSurfaceBrightness;
        }
        return;
}










void CPed::SetIdle() {    // converted from decomp src/CPed/*.c
        ePedState eVar1;

        eVar1 = this->m_nPedState;
        if (((eVar1 != PEDSTATE_IDLE) && (eVar1 != PEDSTATE_MUG)) && (eVar1 != PEDSTATE_FLEE_ENTITY)) {
            if (eVar1 == PEDSTATE_AIMGUN) {
                this->m_nPedState = PEDSTATE_IDLE;
            }
            this->m_nMoveState = PEDMOVE_STILL;
        }
        return;
}










void CPed::SetLook(float heading) {    // converted from decomp src/CPed/SetLook_005e79b0.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        ePedState eVar2;
        CEntity *this_00;
        uint32_t uVar3;
        uint32_t uVar4;

        uVar4 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if (((((uVar4 & 0x600) == 0) && (eVar2 = this->m_nPedState, eVar2 != PEDSTATE_DIE)) &&
        (eVar2 != PEDSTATE_DEAD)) &&
        ((eVar2 != PEDSTATE_ARRESTED &&
        (this->m_nPedState = PEDSTATE_LOOK_HEADING, this->m_nLookTime < CTimer::m_snTimeInMilliseconds
        )))) {
            this_00 = this->m_pLookTarget;
            uVar4 = uVar4 & 0xfffffff7 | 4;
            bIsStanding = (char)uVar4;
            bInVehicle = (char)(uVar4 >> 8);
            bFiringWeapon = (char)(uVar4 >> 0x10);
            bNotAllowedToDuck = (char)(uVar4 >> 0x18);
            this->m_fLookDirection = heading;
            if (this_00 != nullptr) {
                this_00->CleanUpOldReference(&this->m_pLookTarget); // TODO(port): decomp static-style call
            }
            this->m_pLookTarget = nullptr;
            this->m_nLookTime = 0;
            if (((this->m_nPedState != PEDSTATE_DRIVING) && (this->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR)
            ) && ((bIsDucking) == 0))
            {
                // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
                m_pedIK.m_nFlags = m_pedIK.m_nFlags & 0xfffffffd; // TODO(port): was decomp comment-artifact
            }
        }
        return;
}












void CPed::SetLook(CEntity* entity) {    // converted from decomp src/CPed/SetLook_005e7a60.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        CEntity **entity_00;
        ePedState eVar2;
        CEntity *this_00;
        uint32_t uVar3;
        uint32_t uVar4;

        uVar4 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        if (((((uVar4 & 0x600) == 0) && (eVar2 = this->m_nPedState, eVar2 != PEDSTATE_DIE)) &&
        (eVar2 != PEDSTATE_DEAD)) &&
        ((eVar2 != PEDSTATE_ARRESTED &&
        (this->m_nPedState = PEDSTATE_LOOK_ENTITY, this->m_nLookTime < CTimer::m_snTimeInMilliseconds)
        ))) {
            this_00 = this->m_pLookTarget;
            entity_00 = &this->m_pLookTarget;
            uVar4 = uVar4 & 0xfffffff7 | 4;
            bIsStanding = (char)uVar4;
            bInVehicle = (char)(uVar4 >> 8);
            bFiringWeapon = (char)(uVar4 >> 0x10);
            bNotAllowedToDuck = (char)(uVar4 >> 0x18);
            if (this_00 != nullptr) {
                this_00->CleanUpOldReference(entity_00); // TODO(port): decomp static-style call
            }
            *entity_00 = entity;
            entity->RegisterReference(entity_00); // TODO(port): was static-style call
            this->m_fLookDirection = 999999.0;
            this->m_nLookTime = 0;
            if (((this->m_nPedState != PEDSTATE_DRIVING) && (this->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR)
            ) && ((bIsDucking) == 0))
            {
                // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
                m_pedIK.m_nFlags = m_pedIK.m_nFlags & 0xfffffffd; // TODO(port): was decomp comment-artifact
            }
        }
        return;
}












void CPed::Look() {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone Look; body from gta-reversed Ped.cpp.
    TurnBody();
}












CEntity* CPed::AttachPedToEntity(CEntity* entity,  CVector offset,  uint16_t arg2,  float arg3,  eWeaponType weaponType) {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
        uint32_t uVar2;
        ePedType eVar3;
        CWeaponInfo *pCVar4;
        int mode; // TODO(port): eCamMode not ported

        if ((entity == nullptr) ||
        ((bInVehicle) != 0)) {
            return nullptr;
        }
        this->m_pAttachedTo = (CPhysical *)entity;
        entity->RegisterReference((CEntity **)&this->m_pAttachedTo); // TODO(port): decomp static-style call
        (this->m_vecTurretOffset).x = offset.x;
        (this->m_vecTurretOffset).y = offset.y;
        (this->m_vecTurretOffset).z = offset.z;
        this->m_fTurretAngleB = arg3;
        eVar3 = this->m_nPedType;
        this->m_fTurretAngleA = arg2;
        if ((eVar3 == PED_TYPE_PLAYER1) || (eVar3 == PED_TYPE_PLAYER2)) {
            m_nFlags = m_nFlags & 0xfffffffe;
        }
        else if (((uint32_t)entity->GetType() & 7) == 2) {
            this->m_pEntityIgnoredCollision = entity;
        }
        if (this->m_nSavedWeapon == WEAPON_UNIDENTIFIED) {
            this->m_nSavedWeapon =
            this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type;
            this->m_nTurretAmmo = this->m_aWeapons[this->m_nActiveWeaponSlot].m_TotalAmmo;
        }
        if ((eVar3 != PED_TYPE_PLAYER1) && (eVar3 != PED_TYPE_PLAYER2)) {
            GiveWeapon(weaponType,30000,true);
            pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
            this->SetCurrentWeapon((int32_t)pCVar4->m_nSlot); // TODO(port): decomp static-style call
            PositionAttachedPed();
            return entity;
        }
        if (weaponType != WEAPON_UNARMED) {
            GiveWeapon(weaponType,30000,true);
        }
        this->m_pPlayerData->m_nChosenWeapon = (uint8_t)weaponType;
        ((CPlayerPed *)this)->MakeChangesForNewWeapon(weaponType);
        if (weaponType == WEAPON_CAMERA) {
            mode = 0 /* TODO(port): MODE_CAMERA */;
        }
        else {
            if (entity->m_nModelIndex == 0x152) {
                pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD);
                if ((pCVar4->m_nFlags >> 2 & 1) == 0) {
                    ((CCamera *)&TheCamera)->SetNewPlayerWeaponMode(MODE_AIMWEAPON_ATTACHED,0,0);
                    this->m_pPlayerData->m_nPlayerFlags |= 8;
                    this->m_nPedState = PEDSTATE_SNIPER_MODE;
                    PositionAttachedPed();
                    return entity;
                }
            }
            mode = 0 /* TODO(port): MODE_HELICANNON_1STPERSON */;
        }
        ((CCamera *)&TheCamera)->SetNewPlayerWeaponMode((eCamMode)mode,0,0);
        this->m_nPedState = PEDSTATE_SNIPER_MODE;
        PositionAttachedPed();
        return entity;
}










void CPed::AttachPedToBike(CEntity* entity,  CVector offset,  uint16_t arg2,  float arg3,  float arg4,  eWeaponType weaponType) {    // converted from decomp src/CPed/*.c
        CEntity *pCVar1;

        pCVar1 = AttachPedToEntity(entity,offset,arg2,arg3,weaponType);
        if (pCVar1 == nullptr) {
            return;
        }
        this->m_nTurretPosnMode = arg4;
        return;
}










void CPed::DettachPedFromEntity() {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        uint32_t uVar2;
        CPhysical *this_00;
        CMatrixLink *pCVar3;
        CVector force;
        CWeaponInfo *pCVar4;

        this_00 = this->m_pAttachedTo;
        this->m_pAttachedTo = nullptr;
        if (this->m_nPedState == PEDSTATE_DIE) {
            this->m_pEntityIgnoredCollision = (CEntity *)this_00;
            if (this_00->m_matrix == nullptr) {
                this_00->AllocateMatrix();
                this_00->m_placement.UpdateMatrix(this_00->m_matrix);
            }
            pCVar3 = this_00->m_matrix;
            force.y = (pCVar3->m_forward).y * -4.0;
            force.x = (pCVar3->m_forward).x * -4.0;
            force.z = (pCVar3->m_forward).z * -4.0 + 4.0;
            CPhysical::ApplyMoveForce(force);
            // (decomp: cleared bit 0 (bIsStanding) of the ped-flags dword
            //  at 0x46c via byte-wise RMW; equivalent to bIsStanding = false)
            this->bIsStanding = false;
        }
        else if (this->m_nPedState != PEDSTATE_DEAD) {
            CAnimManager::BlendAnimation
            ((RpClump *)GetRwObject(),this->m_nAnimGroup,ANIM_ID_IDLE,1000.0);
            m_nFlags = m_nFlags | 1;
            if (this->m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
                this->m_aWeapons[this->m_nActiveWeaponSlot].m_AmmoInClip = 0;
                this->m_aWeapons[this->m_nActiveWeaponSlot].m_TotalAmmo = 0;
                pCVar4 = CWeaponInfo::GetWeaponInfo(this->m_nSavedWeapon, eWeaponSkill::STD);
                this->SetCurrentWeapon((int32_t)pCVar4->m_nSlot); // TODO(port): decomp static-style call
                this->m_aWeapons[this->m_nActiveWeaponSlot].m_TotalAmmo =
                this->m_nTurretAmmo;
                this->m_nSavedWeapon = WEAPON_UNIDENTIFIED;
            }
            if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
                ((CPlayerPed *)this)->ClearWeaponTarget();
                return;
            }
        }
        return;
}










void CPed::SetAimFlag(float heading) {    // converted from decomp src/CPed/SetAimFlag_005e8830.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        CEntity **entity;
        CEntity *this_00;
        uint32_t uVar2;
        eWeaponType weaponType;
        eWeaponSkill skill;
        uint32_t uVar3;
        CWeaponInfo *pCVar4;

        uVar3 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        entity = &this->m_pLookTarget;
        this->m_fLookDirection = heading;
        this_00 = *entity;
        uVar3 = uVar3 & 0xffffffdf | 0x10;
        bIsStanding = (char)uVar3;
        bInVehicle = (char)(uVar3 >> 8);
        bFiringWeapon = (char)(uVar3 >> 0x10);
        bNotAllowedToDuck = (char)(uVar3 >> 0x18);
        this->m_nLookTime = 0;
        if (this_00 != nullptr) {
            this_00->CleanUpOldReference(entity); // TODO(port): decomp static-style call
        }
        uVar2 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        *entity = nullptr;
        if ((uVar2 & 0x4000000) != 0) {
            // TODO(port): CPedIK opaque (see CPed.h); original sets m_pedIK.m_nFlags |= 2 (bTorsoUsed).
            m_pedIK.m_nFlags = m_pedIK.m_nFlags & 0xfffffffb; // TODO(port): was decomp comment-artifact
        }
        weaponType = this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type;
        skill = GetWeaponSkill(weaponType);
        pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType,skill);
        uVar3 = m_pedIK.m_nFlags;
        if ((pCVar4->m_nFlags >> 1 & 1) != 0) {
            m_pedIK.m_nFlags = uVar3 | 4;
            return;
        }
        m_pedIK.m_nFlags = uVar3 & 0xfffffffb;
        return;
}












bool CPed::CanWeRunAndFireWithWeapon() {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
        eWeaponType weaponType;
        eWeaponSkill skill;
        CWeaponInfo *pCVar1;

        weaponType = this->m_aWeapons[this->m_nActiveWeaponSlot].m_Type;
        skill = GetWeaponSkill(weaponType);
        pCVar1 = CWeaponInfo::GetWeaponInfo(weaponType,skill);
        // TODO(port): decomp CONCAT31 bit-pack; equivalent: bit1 set OR any of bits9-31 set
        return (pCVar1->m_nFlags & 0xFFFFFE02u) != 0;
}










void CPed::RequestDelayedWeapon() {    // converted from decomp src/CPed/*.c
        int modelId;
        int modelId_00;
        CWeaponInfo *pCVar1;

        if (this->m_nDelayedWeapon != WEAPON_UNIDENTIFIED) {
            pCVar1 = CWeaponInfo::GetWeaponInfo(this->m_nDelayedWeapon, eWeaponSkill::STD);
            modelId = pCVar1->m_nModelId1;
            pCVar1 = CWeaponInfo::GetWeaponInfo(this->m_nDelayedWeapon, eWeaponSkill::STD);
            modelId_00 = pCVar1->m_nModelId2;
            if (modelId != -1) {
                CStreaming::RequestModel(modelId,8);
            }
            if (modelId_00 != -1) {
                CStreaming::RequestModel(modelId_00,8);
            }
            if (((modelId == -1) || (CStreaming::ms_aInfoForModel[modelId].m_LoadState == LOADSTATE_LOADED))
            && ((modelId_00 == -1 ||
            (CStreaming::ms_aInfoForModel[modelId_00].m_LoadState == LOADSTATE_LOADED)))) {
                GiveWeapon(this->m_nDelayedWeapon,this->m_nDelayedWeaponAmmo,true);
                this->m_nDelayedWeapon = WEAPON_UNIDENTIFIED;
            }
        }
        return;
}










void CPed::GiveDelayedWeapon(eWeaponType weaponType,  uint32_t ammo) {    // converted from decomp src/CPed/*.c
        CTaskSimpleHoldEntity *pCVar1;

        if ((this->m_nPedType != PED_TYPE_PLAYER1) && (this->m_nPedType != PED_TYPE_PLAYER2)) {
            pCVar1 = this->m_pIntelligence->GetTaskHold(false);
            if ((pCVar1 != nullptr) &&
            ((pCVar1->m_pEntityToHold != nullptr && (pCVar1->m_nBoneFrameId == '\x06')))) {
                DropEntityThatThisPedIsHolding(true); // TODO(port): decomp static-style call
            }
        }
        if (this->m_nDelayedWeapon == WEAPON_UNIDENTIFIED) {
            this->m_nDelayedWeaponAmmo = ammo;
            this->m_nDelayedWeapon = weaponType;
            RequestDelayedWeapon();
        }
        return;
}










void CPed::GiveWeaponAtStartOfFight() {    // converted from decomp src/CPed/*.c
        CWeaponInfo *pCVar1;

        if ((this->m_nCreatedBy != PED_MISSION) &&
        (m_aWeapons[m_nActiveWeaponSlot].m_Type == WEAPON_UNARMED)) {
            switch(this->m_nPedType) {
                case PED_TYPE_GANG1:
                case PED_TYPE_GANG2:
                case PED_TYPE_GANG3:
                case PED_TYPE_GANG4:
                case PED_TYPE_GANG5:
                case PED_TYPE_GANG6:
                case PED_TYPE_GANG7:
                case PED_TYPE_GANG8:
                case PED_TYPE_GANG9:
                case PED_TYPE_GANG10:
                if (((this->m_nRandomSeed & 0x3ff) < 400) &&
                (this->m_nDelayedWeapon == WEAPON_UNIDENTIFIED)) {
                    GiveDelayedWeapon(WEAPON_PISTOL,0x32);
                    pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_PISTOL, eWeaponSkill::STD);
                    this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
                }
                break;
                case PED_TYPE_DEALER:
                case PED_TYPE_CRIMINAL:
                case PED_TYPE_PROSTITUTE:
                if (((this->m_nRandomSeed & 0x3ff) < 200) &&
                (this->m_nDelayedWeapon == WEAPON_UNIDENTIFIED)) {
                    GiveDelayedWeapon(WEAPON_KNIFE,0x32);
                    pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_KNIFE, eWeaponSkill::STD);
                    this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
                }
                if (((this->m_nRandomSeed & 0x3ff) < 400) &&
                (this->m_nDelayedWeapon == WEAPON_UNIDENTIFIED)) {
                    GiveDelayedWeapon(WEAPON_PISTOL,0x32);
                    pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_PISTOL, eWeaponSkill::STD);
                    this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
                    return;
                }
            }
        }
        return;
}










void CPed::GiveWeaponWhenJoiningGang() {    // converted from decomp src/CPed/*.c
        CWeaponInfo *pCVar1;

        if ((m_aWeapons[m_nActiveWeaponSlot].m_Type == WEAPON_UNARMED) &&
        (this->m_nDelayedWeapon == WEAPON_UNIDENTIFIED)) {
            if (CCheat::m_aCheatsActive[0x4d]) {
                GiveDelayedWeapon(WEAPON_AK47,200);
                pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_AK47, eWeaponSkill::STD);
                this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
                return;
            }
            if (CCheat::m_aCheatsActive[0x4e]) {
                GiveDelayedWeapon(WEAPON_RLAUNCHER,200);
                pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_RLAUNCHER, eWeaponSkill::STD);
                this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
                return;
            }
            GiveDelayedWeapon(WEAPON_PISTOL,200);
            pCVar1 = CWeaponInfo::GetWeaponInfo(WEAPON_PISTOL, eWeaponSkill::STD);
            this->SetCurrentWeapon((int32_t)pCVar1->m_nSlot); // TODO(port): decomp static-style call
        }
        return;
}










bool CPed::GetPedTalking() {    // converted from decomp src/CPed/*.c
        bool bVar1;

        bVar1 = this->m_pedSpeech.GetPedTalking();
        return bVar1;
}










void CPed::DisablePedSpeech(bool stopCurrentSpeech) {    // converted from decomp src/CPed/*.c
        uint8_t in_stack_00000005;

        this->m_pedSpeech.DisablePedSpeech(stopCurrentSpeech);
        return;
}










void CPed::EnablePedSpeech() {    // converted from decomp src/CPed/*.c
        this->m_pedSpeech.EnablePedSpeech();
        return;
}










void CPed::DisablePedSpeechForScriptSpeech(bool stopCurrentSpeech) {    // converted from decomp src/CPed/*.c
        uint8_t in_stack_00000005;

        this->m_pedSpeech.DisablePedSpeechForScriptSpeech(stopCurrentSpeech);
        return;
}










void CPed::EnablePedSpeechForScriptSpeech() {    // converted from decomp src/CPed/*.c
        this->m_pedSpeech.EnablePedSpeechForScriptSpeech();
        return;
}










bool CPed::CanPedHoldConversation() const {    // converted from decomp src/CPed/*.c
        bool bVar1;

        bVar1 = this->m_pedSpeech.CanPedHoldConversation();
        return bVar1;
}










void CPed::SayScript(eAudioEvents scriptID,  bool overrideSilence,  bool isForceAudible,  bool isFrontEnd) {    // converted from decomp src/CPed/*.c
        /* undefined3 in_stack_00000009; */
        // TODO(port): dropped unused decomp stack artifact in_stack_0000000d
        /* undefined3 in_stack_00000011; */

        // TODO(port): decomp vtable+4 call on m_pedSpeech (unresolved virtual);
        //   original: (**(code**)((int)m_pedSpeech.vtable+4))(0x35,scriptID,...).
        //   Audio entity is a stub; call dropped.
        (void)scriptID; (void)overrideSilence; (void)isForceAudible; (void)isFrontEnd;
        return;
}










int16_t CPed::Say(eGlobalSpeechContext gCtx,  uint32_t startTimeDelay,  float probability,  bool overrideSilence,  bool isForceAudible,  bool isFrontEnd) {    // converted from decomp src/CPed/*.c
        short sVar1;

        if (gCtx != CTX_GLOBAL_NO_SPEECH) {
            sVar1 = this->m_pedSpeech.AddSayEvent
            (AE_SPEECH_PED,gCtx,startTimeDelay,probability,
            overrideSilence,isForceAudible,isFrontEnd);
            return sVar1;
        }
        return -1;
}










void CPed::RemoveBodyPart(ePedNode pedNode,  char localDir) {    // converted from decomp src/CPed/*.c
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
        uint32_t uVar2;
        bool bVar3;

        if (*(int *)((char *)this->m_apBones[pedNode] + 0x10) == 0) {
            // TODO(port): FID_conflict__wprintf("Trying to remove ped component"); // debug print dropped
            return;
        }
        bVar3 = CLocalisation::ShootLimbs();
        if (bVar3) {
            // (decomp: set bit 15 of the ped-flags dword at 0x46c via
            //  byte-wise RMW; bit 15 is bRemoveHead per CPed.h)
            this->bRemoveHead = true;
            this->m_nBodypartToRemove = (char)pedNode;
        }
        return;
}










void CPed::SpawnFlyingComponent(int32_t arg0,  char arg1) {    // converted from decomp src/CPed/*.c
        return;
}










uint8_t CPed::DoesLOSBulletHitPed(CColPoint& colPoint) {    // converted from decomp src/CPed/*.c
        RpHAnimHierarchy *pRVar1;
        int iVar2;
        RwMatrix *pMatrixArray; // was: int iVar3 (decomp); matrix array pointer
        RwV3d vPoint{}; // was: uint32_t local_c, local_8; float local_4 (decomp split a vector)

        pRVar1 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
        // decomp: *(uint32_t*)(*(int*)(m_apBones+8) + 0x14); m_apBones+8 == &m_apBones[2]
        iVar2 = RpHAnimIDGetIndex(pRVar1,*(uint32_t *)((char *)this->m_apBones[2] + 0x14));
        pMatrixArray = RpHAnimHierarchyGetMatrixArray(pRVar1);
        RwV3dTransformPoints(&vPoint,&vPoint,1,pMatrixArray + iVar2); // RwMatrix is 0x40 bytes
        if ((this->m_nPedState != PEDSTATE_FALL) && (vPoint.z <= colPoint.m_vecPoint.z)) {
            if (colPoint.m_vecPoint.z < vPoint.z + 0.2) {
                return '\x02';
            }
            return '\0';
        }
        return '\x01';
}










void CPed::RemoveWeaponAnims(int32_t likeUnused,  float blendDelta) {    // converted from decomp src/CPed/*.c
        bool bVar1;
        CAnimBlendAssociation *pCVar2;
        int iVar3;

        bVar1 = false;
        iVar3 = 0x22;
        do {
            pCVar2 = RpAnimBlendClumpGetAssociation((RpClump *)GetRwObject(),0xe0);
            if (pCVar2 != nullptr) {
                *(uint8_t *)&pCVar2->m_Flags = (uint8_t)pCVar2->m_Flags | 4;
                if (((uint8_t)pCVar2->m_Flags >> 4 & 1) == 0) {
                    bVar1 = true;
                }
                else {
                    pCVar2->m_BlendDelta = blendDelta;
                }
            }
            iVar3 = iVar3 + -1;
        } while (iVar3 != 0);
        if (bVar1) {
            CAnimManager::BlendAnimation
            ((RpClump *)GetRwObject(),this->m_nAnimGroup,ANIM_ID_IDLE,-blendDelta);
        }
        return;
}










bool CPed::IsPedHeadAbovePos(float zPos) {    // converted from decomp src/CPed/*.c
        RpHAnimHierarchy *pRVar1;
        int iVar2;
        RwMatrix *pMatrixArray; // was: int iVar3 (decomp); matrix array pointer
        CSimpleTransform *pCVar4;
        RwV3d vPoint{}; // was: uint32_t local_c, local_8; float local_4 (decomp split a vector)

        pRVar1 = GetAnimHierarchyFromSkinClump((RpClump *)GetRwObject());
        // decomp: *(uint32_t*)(*(int*)(m_apBones+8) + 0x14); m_apBones+8 == &m_apBones[2]
        iVar2 = RpHAnimIDGetIndex(pRVar1,*(uint32_t *)((char *)this->m_apBones[2] + 0x14));
        pMatrixArray = RpHAnimHierarchyGetMatrixArray(pRVar1);
        RwV3dTransformPoints(&vPoint,&vPoint,1,pMatrixArray + iVar2); // RwMatrix is 0x40 bytes
        if (this->m_matrix == nullptr) {
            pCVar4 = &this->m_placement;
        }
        else {
            pCVar4 = (CSimpleTransform *)&this->m_matrix->m_pos;
        }
        if (zPos + (pCVar4->m_vPosn).z < vPoint.z) {
            return true;
        }
        return false;
}










// Decomp vector helpers (bodies verified from unk_*.c in src/).
// unk_0040fe90 / unk_0040fec0: out = vec * scale. unk_0040fe30: out = v1 + v2.
// All return the out pointer (decomp said void; callers use the return).
static inline CVector* _vec_scale(CVector* out, const CVector* vec, float scale) {
    *out = *vec * scale;
    return out;
}
static inline CVector* _vec_add(CVector* out, const CVector* v1, const CVector* v2) {
    *out = *v1 + *v2;
    return out;
}

void CPed::KillPedWithCar(CVehicle* car,  float fDamageIntensity,  bool bPlayDeadAnimation) {    // converted from decomp src/CPed/*.c
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): SEH/CRT artifact dropped
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
    // TODO(convert): vtable write
        uint32_t uVar1;
        uint32_t uVar2;
        uint8_t uVar5;
        uint8_t uVar6;
        CEntity *pCVar3;
        CMatrixLink *pCVar4;
        CVector vecMoveSpeed;
        CVector point;
        float fVar7;
        eWeaponType weaponType;
        bool bVar8;
        uint8_t bVar9;
        CTask *pCVar10;
        int iVar11;
        CVehicle *pCVar12;
        CPad *this_00;
        CColModel *pCVar13;
        uint32_t uVar14;
        CVector *pVecTmp; // vector-pointer temp (decomp stashed pointers in uVar14)
        CSimpleTransform *pCVar15;
        CObject *this_01;
        CVector *pCVar16;
        CSimpleTransform *pCVar17;
        float *pfVar18;
        int iVar19;
        CAnimBlendAssociation *pCVar20;
        uint32_t uVar21;
        uint32_t uVar22;
        AnimationId animId;
        long double /* Ghidra float10 */ fVar23;
        float fVar24;
        float fVar25;
        float fVar26;
        CVector* pVecMoveSpeedRef; // was: uint16_t uVar27 (decomp); points at m_vecMoveSpeed
        uint8_t uVar28;
        uint8_t uVar29;
        eAudioEvents event;
        uint32_t uVar30;
        char cStack_d1;
        CSimpleTransform *pCStack_d0;
        float fDamageFactor; // 14.0f/150.0f for CPedDamageResponseCalculator (decomp reused pCStack_d0 slot)
        float fStack_cc;
        CVector CStack_c8;
        float fStack_bc;
        float fStack_b8;
        int iStack_b4;
        CVector CStack_b0;
        uint8_t bStack_a1;
        float fStack_a0;
        float fStack_9c;
        float fStack_98;
        eWeaponType eStack_94;
        uint8_t auStack_90 [12];
        float fStack_84;
        CVector2D CStack_80;
        float fStack_78;
        float fStack_74;
        uint8_t auStack_70 [12];
        CEventDamage CStack_64;
        CPedDamageResponseCalculator CStack_20;
        uint32_t uStack_4;

        uStack_4 = 0xffffffff;
        pCVar10 = this->m_pIntelligence->m_TaskMgr.GetSimplestActiveTask();
        if (pCVar10 != nullptr) {
            // decomp called CTask vtable+0x10 (GetTaskType in the binary)
            iVar11 = pCVar10->GetTaskType();
            if ((iVar11 == TASK_SIMPLE_FALL) || (iVar11 == TASK_SIMPLE_DIE)) {
                if ((this->m_pEntityIgnoredCollision != nullptr) &&
                ((int)car->GetStatus() != 0)) {
                    return;
                }
                this->m_pEntityIgnoredCollision = (CEntity *)car;
                return;
            }
            if (iVar11 == TASK_SIMPLE_DEAD) {
                return;
            }
        }
        pCVar3 = this->m_pContactEntity;
        if ((pCVar3 == nullptr) || (pCVar3->GetType() != ENTITY_TYPE_VEHICLE)) {
            if ((this->m_pVehicle != nullptr) && (this->m_pVehicle == car)) {
                if (car->m_nVehicleType == VEHICLE_TYPE_BOAT) {
                    return;
                }
                if (car->m_nVehicleSubType == VEHICLE_TYPE_PLANE) {
                    return;
                }
            }
        }
        else {
            // TODO(port): decomp `pCVar3[0x19].m_pRwObject == (RwObject*)0x5` is a Ghidra
            //   array-index artifact (field at ~0x64 unidentified); condition dropped
            //   pending audit. Original returned early here.
            if (false) {
                return;
            }
            bVar8 = this->IsPlayer();
            if (bVar8) {
                return;
            }
        }
        pCVar10 = this->m_pIntelligence->m_TaskMgr.FindActiveTaskByType(TASK_COMPLEX_DESTROY_CAR_MELEE);
        // TODO(port): decomp `pCVar10[2].vtable == car` is a Ghidra artifact
        //   (member access on task at unknown offset); treated as false pending audit.
        if ((pCVar10 != nullptr) && false) {
            fStack_a0 = (car->m_vecMoveSpeed).x;
            fStack_9c = (car->m_vecMoveSpeed).y;
            fStack_98 = (car->m_vecMoveSpeed).z;
            fVar23 = (long double /* Ghidra float10 */)car->m_vecMoveSpeed.SquaredMagnitude(); // was CVector::unk_00406da0() (empty decomp)
            if (fVar23 < (long double /* Ghidra float10 */)0.0225) {
                return;
            }
        }
        pCVar15 = (CSimpleTransform *)&car->m_matrix->m_pos;
        if (car->m_matrix == nullptr) {
            pCVar15 = &car->m_placement;
        }
        if (this->m_matrix == nullptr) {
            pCVar17 = &this->m_placement;
        }
        else {
            pCVar17 = (CSimpleTransform *)&this->m_matrix->m_pos;
        }
        fStack_84 = 2.67648e-43;
        CStack_c8.z = (pCVar17->m_vPosn).z - (pCVar15->m_vPosn).z;
        fStack_74 = 1.0;
        CStack_c8.y = (pCVar17->m_vPosn).y - (pCVar15->m_vPosn).y;
        CStack_c8.x = (pCVar17->m_vPosn).x - (pCVar15->m_vPosn).x;
        if ((fDamageIntensity <= 12.0) || (bVar8 = this->IsPlayer(), bVar8)) {
            if ((-0.8 <= (this->m_vecLastCollisionImpactVelocity).z) || (fDamageIntensity <= 3.0)) {
                if (fDamageIntensity <= 6.0) {
                    return;
                }
                bVar8 = this->IsPlayer();
                if ((bVar8) && (fDamageIntensity <= 10.0)) {
                    return;
                }
            }
            uVar30 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
            uVar30 = uVar30 & 0xfffffffe;
            bIsStanding = (char)uVar30;
            bInVehicle = (char)(uVar30 >> 8);
            bFiringWeapon = (char)(uVar30 >> 0x10);
            bNotAllowedToDuck = (char)(uVar30 >> 0x18);
            CStack_80.y = -(car->m_vecMoveSpeed).y;
            pCVar16 = &car->m_vecMoveSpeed;
            CStack_80.x = -pCVar16->x;
            iVar11 = GetLocalDirection(CStack_80);
            fDamageFactor = 14.0f; // 0x41f00000
            bVar8 = this->IsPlayer();
            if ((bVar8) && (car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN)) {
                fDamageFactor = 150.0f; // 0x43160000
            }
            // TODO(port): decomp emitted the constructor as an explicit call; placement new.
            new (&CStack_20) CPedDamageResponseCalculator
            ((CEntity *)car,fDamageFactor,WEAPON_RAMMEDBYCAR,PED_PIECE_TORSO,false);
            uVar6 = bInVehicle;
            uStack_4 = 2;
            // TODO(port): decomp emitted the constructor as an explicit call; placement new.
            new (&CStack_64) CEventDamage((CEntity *)car, CTimer::m_snTimeInMilliseconds, WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, (uint8_t)iVar11, false, (bool)(uVar6 & 1));
            uStack_4 = (uStack_4 & ~0xFF) | 3; // TODO(port): was CONCAT31(uStack_4._1_3_,3)
            animId = (AnimationId)(iVar11 + ANIM_ID_KO_SKID_FRONT);
            bVar8 = CStack_64.AffectsPed(this); // TODO(port): decomp static-style call
            if (bVar8) {
                CStack_20.ComputeDamageResponse(this, CStack_64.m_damageResponse, true); // TODO(port): decomp static-style call
                CStack_64.m_nAnimGroup = ANIM_GROUP_DEFAULT;
                CStack_64.m_fAnimBlend = 8.0;
                CStack_64.m_fAnimSpeed = 1.0;
                CStack_64.m_nAnimID = animId;
                if (bPlayDeadAnimation) {
                    pCVar20 = CAnimManager::BlendAnimation
                    ((RpClump *)GetRwObject(),ANIM_GROUP_DEFAULT,animId,8.0);
                    pCVar20->m_Speed = 1.0;
                    CStack_64.bits_m_bJumpedOutOfMovingCar = CStack_64.bits_m_bJumpedOutOfMovingCar | 4;
                }
                this->m_pIntelligence->m_eventGroup.Add(&CStack_64, false); // TODO(port): decomp static-style call
                bVar8 = this->IsPlayer();
                if ((((!bVar8) || ((m_nFlags & 0x1000) != 0)) ||
                (car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN)) ||
                ((this->m_vecLastCollisionImpactVelocity).z < -0.8)) {
                    this->m_pEntityIgnoredCollision = (CEntity *)car;
                }
                (this->m_pIntelligence->m_collisionScanner).m_bAlreadyHitByCar = true;
            }
            uVar2 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar2 = uVar2 & 0xffffffef;
            bResetWalkAnims = (char)uVar2;
            bCollidedWithMyVehicle = (char)(uVar2 >> 8);
            bMiamiViceCop = (char)(uVar2 >> 0x10);
            bDontFight = (char)(uVar2 >> 0x18);
            if (car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN) {
                if ((m_nFlags & 0x1000) == 0) {
                    CStack_b0.x = pCVar16->x;
                    CStack_b0.y = (car->m_vecMoveSpeed).y;
                    CStack_b0.z = (car->m_vecMoveSpeed).z;
                    CStack_b0.Normalise();
                    // unk_0040fec0(out,vec,scale): out = vec * scale
                    *reinterpret_cast<CVector*>(auStack_90) =
                        CStack_b0 * (CStack_b0.x * (this->m_vecMoveSpeed).x +
                                     CStack_b0.z * (this->m_vecMoveSpeed).z +
                                     CStack_b0.y * (this->m_vecMoveSpeed).y);
                    // TODO(port): decomp lost the 2nd operands of CVector::unk_00406d70
                    //   (vec -= ?) and CPlaceable::unk_00411a00 (vec += ?); the scratch
                    //   vector is overwritten before any read, so both ops are dead.
                    *reinterpret_cast<CVector*>(auStack_90) = *pCVar16 * 0.3f; // 0x3e99999a
                }
                else {
                    CStack_b0.x = 0.0;
                    (this->m_vecMoveSpeed).x = 0.0;
                    CStack_b0.y = 0.0;
                    CStack_b0.z = 0.0;
                    (this->m_vecMoveSpeed).y = 0.0;
                    (this->m_vecMoveSpeed).z = 0.0;
                }
            }
            else if ((m_nFlags & 0x1000) == 0) {
                *reinterpret_cast<CVector*>(auStack_90) = *pCVar16 * 0.75f; // was CColSphere::unk_0040fec0 (0x3f400000)
                pfVar18 = (float *)auStack_90;
                (this->m_vecMoveSpeed).x = *pfVar18;
                (this->m_vecMoveSpeed).y = pfVar18[1];
                (this->m_vecMoveSpeed).z = pfVar18[2];
            }
            (this->m_vecMoveSpeed).z = 0.0;
            bVar8 = CLocalisation::KnockDownPeds();
            if (bVar8) {
                car->m_vehicleAudio.AddAudioEvent(AE_PED_KNOCK_DOWN,0.0);
            }
            Say((eGlobalSpeechContext)CTX_GLOBAL_PAIN_LOW,0,1.0f,false,false,false);
            uStack_4 = (uStack_4 & ~0xFF) | 2; // TODO(port): was CONCAT31(uStack_4._1_3_,2)
            goto LAB_005f1132;
        }
        iVar11 = CGeneral::GetRandomNumber();
        bStack_a1 = (uint8_t)iVar11 & 3;
        eStack_94 = WEAPON_RAMMEDBYCAR;
        pCVar12 = FindPlayerVehicle(-1,false);
        if (car == pCVar12) {
            fStack_bc = 1.0 / car->m_fMass;
            fVar24 = car->m_vecMoveSpeed.Magnitude();
            if (fVar24 * fStack_bc * 200000.0 + 80.0 <= 250.0) {
                car->m_vecMoveSpeed.Magnitude();
            }
            bVar9 = CGeneral::GetRandomNumber();
            uVar30 = 0;
            uVar28 = (uint8_t)(40000U / (uint64_t)(int64_t)(int)(uint32_t)bVar9);
            uVar29 = (uint8_t)(40000U / (uint64_t)(int64_t)(int)(uint32_t)bVar9 >> 8);
            this_00 = (CPad *)CPad::GetPad(0);
            this_00->StartShake((int16_t)((uVar29 << 8) | uVar28),bVar9,uVar30); // was CONCAT11
        }
        uVar22 = (bIsStanding ? 1u : 0u) | (bInVehicle ? 0x100u : 0u) | (bFiringWeapon ? 0x10000u : 0u) | (bNotAllowedToDuck ? 0x1000000u : 0u);
        uVar22 = uVar22 & 0xfffffffe;
        bIsStanding = (char)uVar22;
        bInVehicle = (char)(uVar22 >> 8);
        bFiringWeapon = (char)(uVar22 >> 0x10);
        bNotAllowedToDuck = (char)(uVar22 >> 0x18);
        CStack_80.y = -(car->m_vecMoveSpeed).y;
        pCVar16 = &car->m_vecMoveSpeed;
        CStack_80.x = -pCVar16->x;
        iStack_b4 = GetLocalDirection(CStack_80);
        pCVar13 = ((CEntity *)car)->GetColModel() /* TODO(port): decomp static-style */;
        pCVar4 = car->m_matrix;
        fVar24 = (pCVar13->m_boundBox).m_vecMax.x;
        fVar26 = CStack_c8.x * (pCVar4->m_right).x +
        CStack_c8.y * (pCVar4->m_right).y + CStack_c8.z * (pCVar4->m_right).z;
        fStack_bc = CStack_c8.x * (pCVar4->m_up).x +
        CStack_c8.y * (pCVar4->m_up).y + CStack_c8.z * (pCVar4->m_up).z;
        if (car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN) {
            // uVar28 = SUB41(&fStack_a0,0); // TODO(port): dead (decomp byte-grab for vector ptr)
        LAB_005f0b6a:
            cStack_d1 = '\x02';
        LAB_005f0b6f:
            eStack_94 = WEAPON_RUNOVERBYCAR;
            // was: pfVar18 = unk_0040fec0(uVar28,pCVar16,0x3f666666); m_vecMoveSpeed = *pfVar18
            this->m_vecMoveSpeed = *pCVar16 * 0.9f; // 0x3f666666
            (this->m_vecMoveSpeed).z = 0.0;
            if ((iStack_b4 == 1) || (iStack_b4 == 3)) {
                iStack_b4 = 2;
            }
        LAB_005f0baf:
            bVar8 = CLocalisation::KnockDownPeds();
            if (bVar8) {
                event = (eAudioEvents)AE_PED_CRUNCH;
            LAB_005f0bbc:
                car->m_vehicleAudio.AddAudioEvent(event,0.0);
            }
        }
        else {
            fVar25 = DotProduct(*pCVar16,pCVar4->m_forward);
            cStack_d1 = '\0';
            if (fVar25 < 0.0) goto LAB_005f0baf;
            fVar25 = fVar26;
            if (fVar26 < 0.0) {
                fVar25 = -fVar26;
            }
            if (fVar24 * 0.99 < fVar25) {
                cStack_d1 = '\x04';
                if (fVar26 <= 0.0) {
                    cStack_d1 = '\x03';
                }
                fVar24 = CStack_c8.z * (pCVar4->m_forward).z;
                fVar25 = CStack_c8.y * (pCVar4->m_forward).y;
                fVar26 = CStack_c8.x * (pCVar4->m_forward).x;
                pCVar13 = ((CEntity *)car)->GetColModel() /* TODO(port): decomp static-style */;
                if (std::abs(fVar26 + fVar25 + fVar24) < (pCVar13->m_boundBox).m_vecMax.y * 0.85) {
                    // uVar28 = SUB41(&fStack_a0,0); // TODO(port): dead (decomp byte-grab for vector ptr)
                    goto LAB_005f0b6f;
                }
                goto LAB_005f0baf;
            }
            if (((bStack_a1 != 0) && ((fStack_bc <= 0.1 || (bStack_a1 < 2)))) ||
            false /* TODO(port): was (*(uint8_t*)&car->m_pHandlingData->__anon0 & 8; tHandlingData opaque */) {
                // uVar28 = SUB41(auStack_90,0); // TODO(port): dead (decomp byte-grab for vector ptr)
                goto LAB_005f0b6a;
            }
            cStack_d1 = '\x01';
            pCVar13 = ((CEntity *)car)->GetColModel() /* TODO(port): decomp static-style */;
            fStack_bc = (pCVar13->m_boundBox).m_vecMax.y;
            pCVar13 = ((CEntity *)car)->GetColModel() /* TODO(port): decomp static-style */;
            fStack_cc = (pCVar13->m_boundBox).m_vecMin.y;
            pCVar13 = ((CEntity *)car)->GetColModel() /* TODO(port): decomp static-style */;
            pCVar4 = car->m_matrix;
            fStack_b8 = (pCVar13->m_boundBox).m_vecMax.z;
            if (-0.2 <= (pCVar4->m_forward).z) {
                if ((pCVar4->m_forward).z <= 0.1) {
                    if (pCVar4 == nullptr) {
                        pCStack_d0 = &car->m_placement;
                    }
                    else {
                        pCStack_d0 = (CSimpleTransform *)&pCVar4->m_pos;
                    }
                    pVecTmp = _vec_scale((CVector*)auStack_90, &pCVar4->m_up, fStack_b8);
                    // pCStack_d0 is CSimpleTransform*; its m_vPosn is the vector at offset 0
                    pVecTmp = _vec_add((CVector*)auStack_70, (CVector*)pCStack_d0, pVecTmp);
                    fStack_b8 = pVecTmp->z;
                    pCVar13 = ((CEntity *)car)->GetColModel() /* TODO(port): decomp static-style */;
                    fStack_cc = (pCVar13->m_boundBox).m_vecMax.y;
                }
                else {
                    // TODO(port): decomp used 16-bit chunk store (._0_2_) for pointer; direct assign
                    if (pCVar4 == nullptr) {
                        pCStack_d0 = (CSimpleTransform *)((uint8_t *)car + 4);
                    }
                    else {
                        pCStack_d0 = (CSimpleTransform *)((uint8_t *)pCVar4 + 0x30);
                    }
                    pVecTmp = _vec_scale((CVector*)auStack_90, &pCVar4->m_forward, fStack_bc);
                    // TODO(port): 4-arg unk_0040fe90 had a spurious trailing arg (dropped)
                    pVecTmp = _vec_scale((CVector*)auStack_70, &pCVar4->m_up, fStack_b8);
                    pVecTmp = _vec_add((CVector*)&fStack_a0, (CVector*)pCStack_d0, pVecTmp);
                    // TODO(port): 2-arg unk_0040fe30 missing an operand; assume out += v1
                    CStack_b0 = CStack_b0 + *pVecTmp;
                    fStack_b8 = CStack_b0.z;
                    pCVar4 = this->m_matrix;
                    fStack_cc = fStack_bc;
                    pCVar15 = (CSimpleTransform *)&pCVar4->m_pos;
                    if (pCVar4 == nullptr) {
                        pCVar15 = &this->m_placement;
                    }
                    if (0.0 < fStack_b8 - (pCVar15->m_vPosn).z) {
                        pCVar15 = (CSimpleTransform *)&pCVar4->m_pos;
                        if (pCVar4 == nullptr) {
                            pCVar15 = &this->m_placement;
                        }
                        (pCVar4->m_pos).z = (fStack_b8 - (pCVar15->m_vPosn).z) * 0.5 + (pCVar4->m_pos).z;
                        if (this->m_matrix == nullptr) {
                            pCVar15 = &this->m_placement;
                        }
                        else {
                            pCVar15 = (CSimpleTransform *)&this->m_matrix->m_pos;
                        }
                        fStack_b8 = (fStack_b8 - (pCVar15->m_vPosn).z) * 0.25 + fStack_b8;
                    }
                }
            }
            else {
                // TODO(port): decomp used 16-bit chunk store (._0_2_) for pointer; direct assign
                if (pCVar4 == nullptr) {
                    pCStack_d0 = (CSimpleTransform *)((uint8_t *)car + 4);
                }
                else {
                    pCStack_d0 = (CSimpleTransform *)((uint8_t *)pCVar4 + 0x30);
                }
                pVecTmp = _vec_scale((CVector*)&fStack_a0, &pCVar4->m_forward, fStack_cc);
                // TODO(port): 4-arg unk_0040fe90 had a spurious trailing arg (dropped)
                pVecTmp = _vec_scale(&CStack_b0, &pCVar4->m_up, fStack_b8);
                pVecTmp = _vec_add((CVector*)auStack_70, (CVector*)pCStack_d0, pVecTmp);
                // TODO(port): 2-arg unk_0040fe30 missing an operand; assume out += v1
                *(CVector*)auStack_90 = *(CVector*)auStack_90 + *pVecTmp;
                fStack_cc = fStack_bc - fStack_cc;
                fStack_b8 = ((CVector*)auStack_90)->z;
            }
            fVar24 = pCVar16->Magnitude();
            if (this->m_matrix == nullptr) {
                pCVar15 = &this->m_placement;
            }
            else {
                pCVar15 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            fVar26 = fStack_b8 - (pCVar15->m_vPosn).z;
            uVar30 = CGeneral::GetRandomNumber();
            CStack_c8.x = pCVar16->x;
            CStack_c8.y = (car->m_vecMoveSpeed).y;
            CStack_c8.z = (car->m_vecMoveSpeed).z;
            fVar24 = ((float)(uVar30 & 0xff) * 0.002 + 1.5) * (fVar26 / (fStack_cc / fVar24));
            CStack_c8.Normalise();
            pfVar18 = (float *)_vec_scale((CVector*)auStack_90, &CStack_c8, fVar24 * 0.2f);
            CStack_c8.x = *pfVar18;
            CStack_c8.y = pfVar18[1];
            CStack_c8.z = pfVar18[2] + fVar24;
            (this->m_vecMoveSpeed).x = CStack_c8.x;
            (this->m_vecMoveSpeed).y = CStack_c8.y;
            (this->m_vecMoveSpeed).z = CStack_c8.z;
            iVar11 = iStack_b4 + 2;
            if (3 < iStack_b4 + 2) {
                iVar11 = iStack_b4 + -2;
            }
            iStack_b4 = iVar11;
            if ((car->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) &&
            (this_01 = ((CAutomobile *)car)->RemoveBonnetInPedCollision(),
            this_01 != nullptr)) {
                uVar30 = CGeneral::GetRandomNumber();
                pVecMoveSpeedRef = &this->m_vecMoveSpeed; // was: uVar27 = SUB42(...) (decomp artifact)
                if ((uVar30 & 1) == 0) {
                    pVecTmp = _vec_scale((CVector*)auStack_90, &car->m_matrix->m_up, 0.5f); // 0x3f000000
                    // TODO(port): 4-arg unk_0040fe90 corrupt (0xcd scale, matrix as vec); best-effort:
                    //   auStack_70 = car_right * 1.0 + *pVecTmp
                    pVecTmp = _vec_scale((CVector*)auStack_70, (CVector*)&car->m_matrix->m_right, 1.0f);
                    pVecTmp = (CVector*)VectorSub(&fStack_a0, pVecMoveSpeedRef, pVecTmp);
                    // TODO(port): 2-arg unk_0040fe30 missing operand; assume out += v1
                    CStack_b0 = CStack_b0 + *pVecTmp;
                    pfVar18 = (float*)&CStack_b0;
                    (this_01->m_vecMoveSpeed).x = *pfVar18;
                    (this_01->m_vecMoveSpeed).y = pfVar18[1];
                    (this_01->m_vecMoveSpeed).z = pfVar18[2];
                }
                else {
                    pVecTmp = _vec_scale((CVector*)auStack_90, &car->m_matrix->m_up, 0.5f); // 0x3f000000
                    // TODO(port): 4-arg unk_0040fe90 corrupt (0xcd scale, matrix as vec); best-effort:
                    pVecTmp = _vec_scale((CVector*)auStack_70, (CVector*)&car->m_matrix->m_right, 1.0f);
                    pVecTmp = _vec_add((CVector*)&fStack_a0, pVecMoveSpeedRef, pVecTmp);
                    // TODO(port): 2-arg unk_0040fe30 missing operand; assume out += v1
                    CStack_b0 = CStack_b0 + *pVecTmp;
                    pfVar18 = (float*)&CStack_b0;
                    (this_01->m_vecMoveSpeed).x = *pfVar18;
                    (this_01->m_vecMoveSpeed).y = pfVar18[1];
                    (this_01->m_vecMoveSpeed).z = pfVar18[2];
                }
                pCVar4 = car->m_matrix;
                pCVar16 = _vec_scale((CVector*)auStack_90, &pCVar4->m_up, 10.0f); // 0x41200000
                ((CPhysical *)this_01)->ApplyTurnForce(*pCVar16,pCVar4->m_forward);
            }
            pCVar15 = (CSimpleTransform *)&car->m_matrix->m_pos;
            if (car->m_matrix == nullptr) {
                pCVar15 = &car->m_placement;
            }
            if (this->m_matrix == nullptr) {
                pCVar17 = &this->m_placement;
            }
            else {
                pCVar17 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            pfVar18 = (float *)VectorSub((CVector*)auStack_90,pCVar17,pCVar15);
            CStack_c8.x = *pfVar18;
            CStack_c8.y = pfVar18[1];
            CStack_c8.z = pfVar18[2];
            bVar8 = CLocalisation::KnockDownPeds();
            if (bVar8) {
                event = (eAudioEvents)AE_PED_BOUNCE;
                goto LAB_005f0bbc;
            }
        }
        if (car->m_pDriver != nullptr) {
            if (this->m_nPedType == PED_TYPE_COP) {
                bVar9 = 0xb;
            }
            else {
                bVar9 = 10;
            }
            CCrime::ReportCrime((eCrimeType)bVar9,(CEntity *)this,car->m_pDriver);
        }
        weaponType = eStack_94;
        // TODO(port): decomp emitted the constructor as an explicit call; placement new.
        new (&CStack_20) CPedDamageResponseCalculator
        ((CEntity *)car,1000.0f,eStack_94,PED_PIECE_TORSO,false);
        iVar11 = iStack_b4;
        uVar5 = bInVehicle;
        uStack_4 = 0;
        // TODO(port): decomp emitted the constructor as an explicit call; placement new.
        new (&CStack_64) CEventDamage((CEntity *)car, CTimer::m_snTimeInMilliseconds, weaponType, PED_PIECE_TORSO, (uint8_t)iStack_b4, false, (bool)(uVar5 & 1));
        uStack_4 = (uStack_4 & ~0xFF) | 1; // TODO(port): was CONCAT31(uStack_4._1_3_,1)
        iVar19 = CGeneral::GetRandomNumber();
        bVar9 = (uint8_t)iVar19 & 3;
        fVar24 = fStack_84;
        switch(iVar11) {
            case 0:
            if (cStack_d1 == '\x03') {
                if (bVar9 < 2) {
                LAB_005f0c7d:
                    fVar24 = (float)ANIM_ID_KO_SKID_FRONT;
                }
                else {
                    fVar24 = (float)ANIM_ID_KO_SPIN_R;
                }
            }
            else {
                if ((cStack_d1 != '\x04') || (bVar9 < 2)) goto LAB_005f0c7d;
                fVar24 = (float)ANIM_ID_KO_SPIN_L;
            }
            break;
            case 1:
        switchD_005f0c52_caseD_1:
            fVar24 = (float)ANIM_ID_KD_LEFT;
            break;
            case 2:
            if (cStack_d1 == '\x03') {
                if (1 < bVar9) goto switchD_005f0c52_caseD_1;
            }
            else if ((cStack_d1 == '\x04') && (1 < bVar9)) goto switchD_005f0c52_caseD_3;
            fVar24 = (float)ANIM_ID_KO_SKID_BACK;
            break;
            case 3:
        switchD_005f0c52_caseD_3:
            fVar24 = (float)ANIM_ID_KD_RIGHT;
        }
        bVar8 = CStack_64.AffectsPed(this); // TODO(port): decomp static-style call
        if (bVar8) {
            if (weaponType == WEAPON_RAMMEDBYCAR) {
                fVar26 = car->m_vecMoveSpeed.Magnitude();
                fStack_bc = fVar26 * 8.0 + 4.0;
            }
            else {
                fVar26 = car->m_vecMoveSpeed.Magnitude();
                fStack_bc = fVar26 * 12.0 + 4.0;
                fVar26 = car->m_vecMoveSpeed.Magnitude();
                fStack_74 = fVar26 * 16.0 + 1.0;
            }
            CStack_20.ComputeDamageResponse(this, CStack_64.m_damageResponse, true); // TODO(port): decomp static-style call
            CStack_64.m_nAnimGroup = ANIM_GROUP_DEFAULT;
            CStack_64.m_fAnimBlend = fStack_bc;
            CStack_64.m_fAnimSpeed = fStack_74;
            CStack_64.m_nAnimID = (AnimationId)fVar24;
            if (bPlayDeadAnimation) {
                pCVar20 = CAnimManager::BlendAnimation
                ((RpClump *)GetRwObject(),ANIM_GROUP_DEFAULT,(AnimationId)fVar24,
                fStack_bc);
                pCVar20->m_Speed = fStack_74;
                CStack_64.bits_m_bJumpedOutOfMovingCar = CStack_64.bits_m_bJumpedOutOfMovingCar | 4;
            }
            this->m_pIntelligence->m_eventGroup.Add(&CStack_64, false); // TODO(port): decomp static-style call
            if (this->m_pEntityIgnoredCollision == nullptr) {
                this->m_pEntityIgnoredCollision = (CEntity *)car;
            }
            uVar21 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            if (cStack_d1 == '\x01') {
                uVar21 = uVar21 | 0x10;
            }
            else {
                uVar21 = uVar21 & 0xffffffef;
            }
            bResetWalkAnims = (char)uVar21;
            bCollidedWithMyVehicle = (char)(uVar21 >> 8);
            bMiamiViceCop = (char)(uVar21 >> 0x10);
            bDontFight = (char)(uVar21 >> 0x18);
            (this->m_pIntelligence->m_collisionScanner).m_bAlreadyHitByCar = true;
        }
        else {
            uVar1 = (bResetWalkAnims ? 1u : 0u) | (bCollidedWithMyVehicle ? 0x100u : 0u) | (bMiamiViceCop ? 0x10000u : 0u) | (bDontFight ? 0x1000000u : 0u);
            uVar1 = uVar1 & 0xffffffef;
            bResetWalkAnims = (char)uVar1;
            bCollidedWithMyVehicle = (char)(uVar1 >> 8);
            bMiamiViceCop = (char)(uVar1 >> 0x10);
            bDontFight = (char)(uVar1 >> 0x18);
        }
        Say((eGlobalSpeechContext)CTX_GLOBAL_PAIN_DEATH_HIGH,0,1.0f,false,false,false);
        uStack_4 = uStack_4 & 0xffffff00;
    LAB_005f1132:
        // TODO(port): CEventDamage_Destructor(); - decomp destructor thunk
        uStack_4 = 0xffffffff;
        // TODO(port): CPedDamageResponseCalculator_Destructor(); - decomp destructor thunk
        pCVar4 = car->m_matrix;
        pfVar18 = (float *)_vec_scale((CVector*)auStack_90, &pCVar4->m_up,
        CStack_c8.x * (pCVar4->m_up).x +
        CStack_c8.z * (pCVar4->m_up).z + CStack_c8.y * (pCVar4->m_up).y);
        CStack_c8.x = CStack_c8.x - *pfVar18;
        CStack_c8.y = CStack_c8.y - pfVar18[1];
        CStack_c8.z = CStack_c8.z - pfVar18[2];
        CStack_b0.x = CStack_c8.x;
        CStack_b0.y = CStack_c8.y;
        CStack_b0.z = CStack_c8.z;
        CStack_b0.Normalise();
        fVar24 = CStack_b0.y * (car->m_vecMoveSpeed).y;
        fVar25 = CStack_b0.z * (car->m_vecMoveSpeed).z;
        fVar26 = CStack_b0.x * (car->m_vecMoveSpeed).x;
        if ((bResetWalkAnims & 0x10) == 0) {
            CStack_b0.z = CStack_b0.z - 0.2;
        }
        fVar7 = _DAT_008d22a8;
        if (car->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            fVar7 = _DAT_008d22ac;
        }
        CStack_80.x = CStack_c8.x * _DAT_008d22a4;
        CStack_80.y = CStack_c8.y * _DAT_008d22a4;
        fStack_78 = CStack_c8.z * _DAT_008d22a4;
        // unk_00404330(a,b) == min(a,b) (decomp body verified)
        fVar23 = (long double /* Ghidra float10 */)std::min(1.0f, car->m_fMass / DAT_008d22a0); // 0x3f800000=1.0f
        fStack_84 = (float)fVar23;
        // TODO(port): SUB41(DAT_008d22a0,0) was a decomp byte-grab; using full value
        fVar23 = (long double /* Ghidra float10 */)std::min(car->m_fMass, DAT_008d22a0);
        fVar23 = fVar23 * (long double /* Ghidra float10 */)fStack_84 * (long double /* Ghidra float10 */)((fVar26 + fVar25 + fVar24) * fVar7);
        fStack_a0 = (float)((long double /* Ghidra float10 */)CStack_b0.x * fVar23);
        fStack_9c = (float)((long double /* Ghidra float10 */)CStack_b0.y * fVar23);
        fStack_98 = (float)(fVar23 * (long double /* Ghidra float10 */)CStack_b0.z);
        // TODO(port): decomp built these vectors via 16-bit chunk stores (._0_2_ etc.); direct init
        vecMoveSpeed = CVector(fStack_a0, fStack_9c, fStack_98);
        point = CVector(CStack_80.x, CStack_80.y, fStack_78);
        ((CPhysical *)car)->ApplyForce(vecMoveSpeed,point,true);
        return;
}










void CPed::DeadPedMakesTyresBloody() {    // converted from decomp src/CPed/*.c
        CMatrixLink *pCVar1;
        float fVar2;
        float fVar3;
        float fVar4;
        float fVar5;
        CSimpleTransform *pCVar6;
        int iVar7;
        int iVar8;
        CSimpleTransform *pCVar9;
        int iVar10;
        int local_1c;
        int local_18;

        pCVar1 = this->m_matrix;
        iVar10 = 0;
        pCVar9 = (CSimpleTransform *)&pCVar1->m_pos;
        if (pCVar1 == nullptr) {
            pCVar9 = &this->m_placement;
        }
        pCVar6 = (CSimpleTransform *)&pCVar1->m_pos;
        if (pCVar1 == nullptr) {
            pCVar6 = &this->m_placement;
        }
        fVar3 = (pCVar6->m_vPosn).x + 2.0f; // was _unk_00858ca0
        pCVar6 = (CSimpleTransform *)&pCVar1->m_pos;
        if (pCVar1 == nullptr) {
            pCVar6 = &this->m_placement;
        }
        fVar2 = (pCVar6->m_vPosn).y - 2.0f; // was _unk_00858ca0
        if (pCVar1 == nullptr) {
            pCVar6 = &this->m_placement;
        }
        else {
            pCVar6 = (CSimpleTransform *)&pCVar1->m_pos;
        }
        fVar5 = (pCVar6->m_vPosn).y + 2.0f; // was _unk_00858ca0
        fVar4 = ((pCVar9->m_vPosn).x - 2.0f) * 0.02 + 60.0; // was _unk_00858ca0
        // CGeneral::uses_ctrlfp_008219f0((double)fVar4); // TODO(port): FPU setup no-op
        iVar7 = CGeneral::GetRandomNumber();
        if (iVar7 < 1) {
            local_1c = 0;
        }
        else {
            // CGeneral::uses_ctrlfp_008219f0((double)fVar4); // TODO(port): FPU setup no-op
            local_1c = CGeneral::GetRandomNumber();
        }
        fVar2 = fVar2 * 0.02 + 60.0;
        // CGeneral::uses_ctrlfp_008219f0((double)fVar2); // TODO(port): FPU setup no-op
        iVar7 = CGeneral::GetRandomNumber();
        if (0 < iVar7) {
            // CGeneral::uses_ctrlfp_008219f0((double)fVar2); // TODO(port): FPU setup no-op
            iVar10 = CGeneral::GetRandomNumber();
        }
        fVar3 = fVar3 * 0.02 + 60.0;
        // CGeneral::uses_ctrlfp_008219f0((double)fVar3); // TODO(port): FPU setup no-op
        iVar7 = CGeneral::GetRandomNumber();
        if (iVar7 < 0x77) {
            // CGeneral::uses_ctrlfp_008219f0((double)fVar3); // TODO(port): FPU setup no-op
            iVar7 = CGeneral::GetRandomNumber();
        }
        else {
            iVar7 = 0x77;
        }
        fVar3 = fVar5 * 0.02 + 60.0;
        // CGeneral::uses_ctrlfp_008219f0((double)fVar3); // TODO(port): FPU setup no-op
        iVar8 = CGeneral::GetRandomNumber();
        if (iVar8 < 0x77) {
            // CGeneral::uses_ctrlfp_008219f0((double)fVar3); // TODO(port): FPU setup no-op
            local_18 = CGeneral::GetRandomNumber();
        }
        else {
            local_18 = 0x77;
        }
        CWorld::AdvanceCurrentScanCode();
        // TODO(port): decomp sector loop was corrupted (bad indices, missing PtrList arg
        //   for MakeTyresMuddySectorList); CWorld sector APIs unported. Original:
        //   for (sy...) for (sx...) MakeTyresMuddySectorList(CWorld::GetRepeatSector(sx,sy).Vehicles);
        (void)local_1c; (void)local_18; (void)iVar7; (void)iVar8; (void)iVar10;
        return;
}










bool CPed::IsInVehicleThatHasADriver() {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone; body from gta-reversed Ped.cpp.
    if (bInVehicle && m_pVehicle && m_pVehicle->IsPassenger(this) && m_pVehicle->m_pDriver) {
        return true;
    }
    return false;
}












CWanted* CPed::GetPlayerWanted() const {    // no decomp; adapted from gta-reversed
    // TODO(port): CPlayerPedData incomplete (see CPed.h). Original: return GetPlayerData()->m_pWanted;
    return nullptr;
}












CPedGroup* CPed::GetGroup() const {    // no decomp; adapted from gta-reversed
    // TODO(port): CPedGroups not yet ported. Original: return CPedGroups::GetPedsGroup(this);
    return nullptr;
}












int32_t CPed::GetGroupId() {    // no decomp; adapted from gta-reversed
    // TODO(port): CPedGroup not yet ported. Original: return GetGroup() ? GetGroup()->GetId() : -1;
    return -1;
}












CPedClothesDesc* CPed::GetClothesDesc() {    // no decomp; adapted from gta-reversed
    // TODO(port): CPlayerPedData incomplete (see CPed.h). Original: return GetPlayerData()->m_pPedClothesDesc;
    return nullptr;
}












bool CPed::IsInVehicleAsPassenger() const noexcept {    // no decomp; adapted from gta-reversed
    return bInVehicle && m_pVehicle && m_pVehicle->m_pDriver != this;
}












CCopPed* CPed::AsCop() {    // no decomp; adapted from gta-reversed
    return reinterpret_cast<CCopPed*>(this);
}












CCivilianPed* CPed::AsCivilian() {    // no decomp; adapted from gta-reversed
    return reinterpret_cast<CCivilianPed*>(this);
}












CEmergencyPed* CPed::AsEmergency() {    // no decomp; adapted from gta-reversed
    return reinterpret_cast<CEmergencyPed*>(this);
}












CPlayerPed* CPed::AsPlayer() {    // no decomp; adapted from gta-reversed
    return reinterpret_cast<CPlayerPed*>(this);
}












bool CPed::IsFollowerOfGroup(const CPedGroup& group) const {    // no decomp; adapted from gta-reversed
    // TODO(port): CPedGroup not yet ported. Original: return group.GetMembership().IsFollower(this);
    (void)group;
    return false;
}






















int32_t CPed::GetPadNumber() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone GetPadNumber; body from gta-reversed Ped.cpp.
    switch (m_nPedType) {
    case PED_TYPE_PLAYER1: return 0;
    case PED_TYPE_PLAYER2: return 1;
    default: return 0; // only called for players; NOTSA_UNREACHABLE in gta-reversed
    }
}












CVector CPed::GetSeatPositionInVehicle() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone GetSeatPositionInVehicle; body from gta-reversed Ped.cpp.
    // TODO(port): CVehicleModelInfo::GetFrontSeatPosn/GetBackSeatPosn + CBmx not yet ported.
    return CVector();
}












bool CPed::IsJoggingOrFaster() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone; body from gta-reversed Ped.cpp.
    switch (m_nMoveState) {
    case PEDMOVE_JOG: case PEDMOVE_RUN: case PEDMOVE_SPRINT: return true;
    default: return false;
    }
}












bool CPed::IsRunningOrSprinting() const {    // no decomp; adapted from gta-reversed
    switch (m_nMoveState) {
    case PEDMOVE_RUN: case PEDMOVE_SPRINT: return true;
    default: return false;
    }
}












bool CPed::IsPedStandingInPlace() const {    // no decomp; adapted from gta-reversed
    switch (m_nMoveState) {
    case PEDMOVE_NONE: case PEDMOVE_STILL: case PEDMOVE_TURN_L: case PEDMOVE_TURN_R:
        return true;
    default: return false;
    }
}












bool CPed::IsRightArmBlockedNow() const {    // no decomp; adapted from gta-reversed
    if (bIsDucking) {
        return bDuckRightArmBlocked;
    }
    return bRightArmBlocked;
}












CVector CPed::GetRealPosition() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone GetRealPosition; body from gta-reversed Ped.h.
    return bInVehicle ? m_pVehicle->GetPosition() : GetPosition();
}












bool CPed::CanBeCriminal() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone CanBeCriminal; body from gta-reversed Ped.cpp.
    if (IsPlayer() || m_nCreatedBy == PED_MISSION) {
        return false;
    }
    switch (m_nPedType) {
    case PED_TYPE_COP:
    case PED_TYPE_MEDIC:
    case PED_TYPE_FIREMAN:
    case PED_TYPE_MISSION1: case PED_TYPE_MISSION2: case PED_TYPE_MISSION3:
    case PED_TYPE_MISSION4: case PED_TYPE_MISSION5: case PED_TYPE_MISSION6:
    case PED_TYPE_MISSION7: case PED_TYPE_MISSION8:
        return false;
    default:
        break;
    }
    return true;
}












void CPed::RenderThinBody() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone RenderThinBody; body from gta-reversed Ped.cpp.
    // TODO(port): CCheat not yet ported; original: if (!CCheat::IsActive(CHEAT_THIN_BODY)) return;
}












void CPed::RenderBigHead() const {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone RenderBigHead; logic from gta-reversed Ped.cpp.
    // TODO(port): CCheat + RenderWare matrix helpers not yet ported.
    // Original scales head/jaw/brow bones 3x when CHEAT_BIG_HEAD is active.
}












RwObject* SetPedAtomicVisibilityCB(RwObject* rwObject,  void* data) {    // TODO: decomp src/CPed/*.c
    (void)rwObject;
    (void)data;
    return nullptr;
}

bool IsPedPointerValid(CPed* ped) {    // TODO: decomp src/CPed/*.c
    (void)ped;
    return false;
}

bool IsPedPointerValid_NotInWorld(CPed* ped) {    // TODO: decomp src/CPed/*.c
    (void)ped;
    return false;
}

bool SayJacked(CPed* jacked,  CVehicle* vehicle,  uint32_t offset) {    // TODO: decomp src/CPed/*.c
    (void)jacked;
    (void)vehicle;
    (void)offset;
    return false;
}

bool SayJacking(CPed* jacker,  CPed* jacked,  CVehicle* vehicle,  uint32_t offset) {    // TODO: decomp src/CPed/*.c
    (void)jacker;
    (void)jacked;
    (void)vehicle;
    (void)offset;
    return false;
}

// Declared-only in the header (no stub): incomplete-type reference
// returns - define when the real classes land:
//   CEventHandlerHistory& CPed::GetEventHandlerHistory();
//   RpHAnimHierarchy& CPed::GetAnimHierarchy();
//   CAnimBlendClumpData& CPed::GetAnimBlendData();
