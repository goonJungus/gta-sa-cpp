// CClumpModelInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/ClumpModelInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), NOTSA_EXPORT_VTABLE,
//   VALIDATE_SIZE (now guarded static_asserts).
// Replaced includes:
//   "RwObjectNameIdAssocation.h" -> struct defined below (TODO: move to its own
//       header when the RW layer is ported; values verified against gta-reversed).
//   "BaseModelInfo.h" -> "CBaseModelInfo.h"

#pragma once

#include "CBaseModelInfo.h"

#include <cstdint>

// ---- RenderWare forward declarations (clean-room renderer provides these later) ----
struct RwFrame;

// Object name -> hierarchy id association (gta-reversed RwObjectNameIdAssocation.h).
// TODO: move to RwObjectNameIdAssocation.h once the RW layer is ported.
// Vehicle component flags (bit layout verified against the decomp:
// SetVehicleComponentFlags @ 0x4C7C10, PreprocessHierarchy @ 0x4C8E60).
// TODO: move to the vehicle subsystem header when ported.
union tVehicleComponentFlagsUnion {
    uint32_t m_nFlags;
    struct {
        uint32_t : 1;
        uint32_t bIsDamageable : 1;
        uint32_t : 1;
        uint32_t bIsDummy : 1;
        uint32_t bIsDoor : 1;
        uint32_t bIsLeft : 1;
        uint32_t bIsRight : 1;
        uint32_t : 1;
        uint32_t bIsRear : 1;
        uint32_t bIsExtra : 1;
        uint32_t bHasAlpha : 1;
        uint32_t : 1;
        uint32_t bCull : 1;
        uint32_t bIsRearDoor : 1;
        uint32_t bIsFrontDoor : 1;
        uint32_t bSwinging : 1;
        uint32_t bIsTrainFrontBogie : 1;
        uint32_t bIsUpgrade : 1;
        uint32_t bDisableReflections : 1;
        uint32_t : 1;
        uint32_t bIsMainWheel : 1;
        uint32_t bIsWheel : 1;
        uint32_t bRenderAlways : 1;
        uint32_t : 8;
        uint32_t bIsFront : 1;
    };
};
struct RwObjectNameIdAssocation {
    char*    m_pName;
    uint32_t m_dwHierarchyId;
    uint32_t m_dwFlags; // see tVehicleComponentFlagsUnion (vehicle subsystem)

    tVehicleComponentFlagsUnion AsFlagsUnion() const {
        tVehicleComponentFlagsUnion u{};
        u.m_nFlags = m_dwFlags;
        return u;
    }
};

struct tCompSearchStructByName {
    const char* m_pName;
    RwFrame*    m_pFrame;

    inline tCompSearchStructByName(const char* name, RwFrame* frame) : m_pName(name), m_pFrame(frame) {}
};

// RwObjectIdAssoc
struct tCompSearchStructById {
    int32_t  m_nId;
    RwFrame* m_pFrame;

    inline tCompSearchStructById(int32_t id, RwFrame* frame) : m_nId(id), m_pFrame(frame) {}
};

// Shared by CPedModelInfo.h and CVehicleModelInfo.h (both derive from CClumpModelInfo).
// Values verified against gta-reversed source/game_sa/Enums/eVehicleClass.h.
// TODO: move to its own header when the vehicle subsystem is ported.
enum eVehicleClass : int8_t {
    VEHICLE_CLASS_IGNORE = -1,
    VEHICLE_CLASS_NORMAL = 0,
    VEHICLE_CLASS_POORFAMILY,
    VEHICLE_CLASS_RICHFAMILY,
    VEHICLE_CLASS_EXECUTIVE,
    VEHICLE_CLASS_WORKER,
    VEHICLE_CLASS_BIG,
    VEHICLE_CLASS_TAXI,
    VEHICLE_CLASS_MOPED,
    VEHICLE_CLASS_MOTORBIKE,
    VEHICLE_CLASS_LEISUREBOAT,
    VEHICLE_CLASS_WORKERBOAT,
    VEHICLE_CLASS_BICYCLE,
};

class CBox; // forward declaration (collision subsystem)

class CClumpModelInfo : public CBaseModelInfo {
public:
    union {
        char*    m_animFileName;
        uint32_t m_nAnimFileIndex;
    };

public:
    CClumpModelInfo() : CBaseModelInfo() {}

    // Overridden vtable methods
    ModelInfoType GetModelType() override;
    void Init() override;
    void Shutdown() override;
    void DeleteRwObject() override;
    uint32_t GetRwModelType() const override { return rpCLUMP; }
    RwObject* CreateInstance() override;
    RwObject* CreateInstance(RwMatrix* matrix) override;
    void SetAnimFile(const char* filename) override;
    void ConvertAnimFileIndex() override;
    int32_t GetAnimFileIndex() override;

    // Added vtable methods
    virtual CBox* GetBoundingBox();
    virtual void SetClump(RpClump* clump);

    // Class functions
    void SetFrameIds(RwObjectNameIdAssocation* data);

    void SetClumpModelInfoFlags(uint32_t flags); // Also calls SetBaseModelInfoFlags

    // static functions
    static RpAtomic* SetAtomicRendererCB(RpAtomic* atomic, void* renderFunc);
    static RpAtomic* AtomicSetupLightingCB(RpAtomic* atomic, void* data);
    static RpAtomic* SetHierarchyForSkinAtomic(RpAtomic* atomic, void* data);
    /* struct tSearchData { char *name; RwFrame *result; };
      returns 0 if we found frame, or last frame if we need to continue searching */
    static RwFrame* FindFrameFromNameCB(RwFrame* frame, void* searchData);
    static RwFrame* FindFrameFromNameWithoutIdCB(RwFrame* frame, void* searchData);
    static RwFrame* FindFrameFromIdCB(RwFrame* frame, void* searchData);
    static RwFrame* FillFrameArrayCB(RwFrame* frame, void* data);
    static RwFrame* GetFrameFromId(RpClump* clump, int32_t id);
    static RwFrame* GetFrameFromName(RpClump* clump, const char* name);
    static void FillFrameArray(RpClump* clump, RwFrame** frames);
};

void SetClumpModelInfoFlags(CClumpModelInfo* modelInfo, uint32_t dwFlags);

// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(RwObjectNameIdAssocation) == 0xC, "RwObjectNameIdAssocation layout drift");
static_assert(sizeof(CClumpModelInfo) == 0x24, "CClumpModelInfo layout drift");
#endif
