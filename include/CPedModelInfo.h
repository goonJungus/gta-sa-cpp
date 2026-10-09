// CPedModelInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/PedModelInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), NOTSA_EXPORT_VTABLE,
//   VALIDATE_SIZE (now a guarded static_assert),
//   StaticRef -> plain static members (defined in the .cpp; original GTA SA 1.0
//   addresses kept as comments).
// Replaced includes:
//   "ClumpModelInfo.h" -> "CClumpModelInfo.h"
//   "ColModel.h"       -> "CColModel.h" (ported; needed for the inline dtor)
//   <extensions/utility.hpp> (notsa::contains) -> plain || in IsPedTypeFemale()
//   <Audio/Enums/ePedAudioType.h> -> eAudioPedType defined below (values verified
//       against gta-reversed)
//   <Audio/Enums/PedSpeechVoices.h> -> ePedSpeechVoiceS16 is `using ... = int16_t`
//       there; aliased below.
// Enums below are verified against gta-reversed (source/game_sa/Enums/*.h).
// TODO: move each to its own header when its subsystem is ported.

#pragma once

#include "CClumpModelInfo.h"
#include "CVector.h"   // tPedColNodeInfo stores CVector by value
#include "CColModel.h" // inline dtor deletes m_pHitColModel

#include <cstdint>

// AssocGroupId: canonical full enum lives in AnimTypes.h (deduped 2026-10-09;
// the minimal 2-value stand-in here redefined it and was removed).
#include "AnimTypes.h"

// ePedType/ePedStats: canonical full ports (deduped 2026-10-08, ped batch)
#include "ePedType.h"
#include "ePedStats.h"



// Genre-based names
#include "eRadioID.h" // canonical (deduped 2026-10-09) // canonical (deduped 2026-10-09)

enum eAudioPedType : int16_t {
    PED_TYPE_UNK = -1, // notsa

    PED_TYPE_GEN = 0,
    PED_TYPE_EMG = 1,
    PED_TYPE_PLAYER = 2,
    PED_TYPE_GANG = 3,
    PED_TYPE_GFD = 4,
    PED_TYPE_SPC = 5, // SPC => Special (?)

    //
    // Add above
    //
    PED_TYPE_NUM // = 6
};

using ePedSpeechVoiceS16 = int16_t;

// eVehicleClass lives in CClumpModelInfo.h (shared with CVehicleModelInfo.h).

// Minimal stand-in: values live in gta-reversed source/game_sa/Enums/ePedRace.h.
// Only used via GetRace() cast here.
// TODO: port the full enum.
enum ePedRace : int32_t {
    PED_RACE_BLACK = 0,
};

struct tPedColNodeInfo {
    char     _pad[4];
    int32_t  m_nBoneID; // see eBoneTag
    int32_t  m_nFlags;
    CVector  m_vecCenter;
    float    m_fRadius;
};

// ---- RenderWare forward declarations (clean-room renderer provides these later) ----
struct RwTexture;

class CPedModelInfo : public CClumpModelInfo {
public:
    AssocGroupId       m_nAnimType;
    ePedType           m_nPedType;
    ePedStats          m_nStatType;
    uint16_t           m_nCarsCanDriveMask; //< Bitset of vehicle classes ped can drive. To check if it can drive a given class check if the bit is set (eg.: & (1 << eVehicleClass))
    uint16_t           m_nPedFlags;
    CColModel*         m_pHitColModel;
    eRadioID           m_nRadio1;
    eRadioID           m_nRadio2;
    uint8_t            m_nRace; // See `ePedRace` - TODO: Maybe we can change this? Check if `ePedRace` can be made 1 byte.
    eAudioPedType      m_nPedAudioType;
    ePedSpeechVoiceS16 m_nVoiceMin; // Also called voice1
    ePedSpeechVoiceS16 m_nVoiceMax; // Also called voice2
    ePedSpeechVoiceS16 m_nVoiceId;  // In `LoadPedObject` this is set to be the same as `m_nVoiceMin` (Which doesn't mean it will always be the same)

    static constexpr int32_t NUM_PED_NAME_ID_ASSOC = 13;
    static constexpr int32_t NUM_PED_COL_NODE_INFOS = 12;

    // StaticRef<RwObjectNameIdAssocation[NUM_PED_NAME_ID_ASSOC]>(0x8A6268) in the original.
    static RwObjectNameIdAssocation m_pPedIds[NUM_PED_NAME_ID_ASSOC]; // 0x8A6268
    // StaticRef<tPedColNodeInfo[NUM_PED_COL_NODE_INFOS]>(0x8A6308) in the original.
    static tPedColNodeInfo m_pColNodeInfos[NUM_PED_COL_NODE_INFOS];   // 0x8A6308

public:
    CPedModelInfo() : CClumpModelInfo(), m_pHitColModel(nullptr) {} // 0x4C57A0
    ~CPedModelInfo() { delete m_pHitColModel; } // 0x4C62F0

    ModelInfoType GetModelType() override;
    void DeleteRwObject() override;
    void SetClump(RpClump* clump) override;


    void AddXtraAtomics(RpClump* clump);
    void SetFaceTexture(RwTexture* texture);
    void CreateHitColModelSkinned(RpClump* clump);
    CColModel* AnimatePedColModelSkinned(RpClump* clump);
    CColModel* AnimatePedColModelSkinnedWorld(RpClump* clump);
    void IncrementVoice();
    auto GetRace() const { return (ePedRace)m_nRace; }
    auto GetPedType() const { return (ePedType)(m_nPedType); }
    auto GetPedStatType() const { return (ePedStats)(m_nStatType); }
    auto CanPedDriveVehicleClass(eVehicleClass cls) const { return (m_nCarsCanDriveMask & (1 << (size_t)cls)) != 0; }
    // Original used notsa::contains({PED_TYPE_CIVFEMALE, PED_TYPE_PROSTITUTE}, GetPedType()).
    bool IsPedTypeFemale() const noexcept {
        const auto pt = GetPedType();
        return pt == PED_TYPE_CIVFEMALE || pt == PED_TYPE_PROSTITUTE;
    }
};

// Layout check: gta-reversed VALIDATE_SIZE(CPedModelInfo, 0x44), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CPedModelInfo) == 0x44, "CPedModelInfo layout drift");
#endif
