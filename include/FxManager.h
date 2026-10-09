// FxManager_c - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Fx/FxManager.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations:
// - stripped InjectHooks()
// - VALIDATE_SIZE(FxManager_c, 0xBC) -> guarded static_assert (32-bit only)
// - TList_c<T>: minimal layout-compatible stand-in (m_head/m_tail/m_cnt).
//   TODO: port Core/List_c.h with the core-containers subsystem.
// - FxFrustumInfo_c (0x54) / FxMemoryPool_c (0xC): size-verified opaque stand-ins.
//   TODO: port fx/FxFrustumInfo.h and fx/FxMemoryPool.h with the fx subsystem and
//   delete the stand-ins. GetMem() is declared on the pool stand-in so the inline
//   Allocate<>() template below still compiles.
// - plugin-sdk `Const` -> `const`
// - FILESTREAM comes from CFileMgr.h (same batch)

#pragma once

#include "GrTypes.h"  // int8..uint32
#include "CVector.h"  // CVector
#include "CMatrix.h"  // RwMatrix
#include "FxSystem.h" // FxSystem_c, ListItem_c, Particle_c fwd-decl
#include "CFileMgr.h" // FILESTREAM

#include <algorithm> // std::min (Allocate)
#include <array>
#include <cstddef> // size_t

struct RwCamera; // CCamera.h precedent: forward-declared until the RenderWare layer lands

class FxEmitterPrt_c;

// TODO: port Core/List_c.h with the core-containers subsystem.
// Minimal layout-compatible stand-in: the real TList_c is an intrusive
// doubly-linked list with exactly these three data members (m_head/m_tail/m_cnt).
template <typename T>
struct TList_c {
    T*     m_head{};
    T*     m_tail{};
    size_t m_cnt{};
};

// TODO: port fx/FxFrustumInfo.h (needs FxSphere_c/FxPlane_c) with the fx subsystem.
// Size-verified opaque stand-in (gta-reversed VALIDATE_SIZE 0x54).
// Delete this when the real header lands.
struct FxFrustumInfo_c {
    uint8 _opaque[0x54];

    // TODO: bool IsCollision(FxSphere_c& sphere); - needs FxSphere_c
};

// TODO: port fx/FxMemoryPool.h with the fx subsystem.
// Size-verified opaque stand-in (gta-reversed VALIDATE_SIZE 0xC).
// Delete this when the real header lands.
struct FxMemoryPool_c {
    uint8 _opaque[0xC];

    // Declared here so FxManager_c::Allocate<>() keeps compiling; the real
    // fx/FxMemoryPool.h declares the full interface.
    void* GetMem(int32 size, int32 align);
};

static constexpr auto FX_MANAGER_NUM_EMITTERS = 1000;

class FxManager_c {
public:
    TList_c<FxSystemBP_c>    m_FxSystemBPs;
    TList_c<FxSystem_c>      m_FxSystems;
    FxEmitterPrt_c*          m_FxEmitters;
    TList_c<Particle_c>      m_FxEmitterParticles;
    int32                    m_nFxTxdIndex;
    CVector*                 m_pWindDir;
    float*                   m_pfWindSpeed;
    FxFrustumInfo_c          m_Frustum;
    uint32                   m_nCurrentMatrix;
    std::array<RwMatrix*, 8> m_apMatrices;
    FxMemoryPool_c           m_Pool;
    bool                     m_bHeatHazeEnabled;

public:
    FxManager_c();
    ~FxManager_c() = default; // 0x4A90A0
    FxManager_c* Constructor();
    FxManager_c* Destructor();

    bool Init();
    void Exit();
    void DestroyFxSystem(FxSystem_c* system);
    // TODO(port): stub added 2026-10-09 for CPed
    void TriggerWaterSplash(CVector* posn) { (void)posn; }
    void DestroyAllFxSystems();

    bool LoadFxProject(const char* path);
    void UnloadFxProject();

    void LoadFxSystemBP(const char* filename, FILESTREAM file);
    FxSystemBP_c* FindFxSystemBP(const char* name);

    FxFrustumInfo_c* GetFrustumInfo() { return &m_Frustum; } // 0x4A9130
    void CalcFrustumInfo(RwCamera* camera);

    void ReturnParticle(FxEmitterPrt_c* emitter);
    Particle_c* GetParticle(int8 primType);
    void FreeUpParticle();

    void Update(RwCamera* camera, float timeDelta);
    void Render(RwCamera* camera, bool bHeatHaze);

    void SetWindData(CVector* dir, float* speed);

    RwMatrix* FxRwMatrixCreate();
    void FxRwMatrixDestroy(RwMatrix* matrix);

    bool ShouldCreate(FxSystemBP_c* system, const RwMatrix& transform, RwMatrix* objectMatrix, bool ignoreBoundingChecks = false);
    FxSystem_c* CreateFxSystem(const char* name, const RwMatrix& transform, RwMatrix* objectMatrix, bool ignoreBoundingChecks = false);
    FxSystem_c* CreateFxSystem(const char* name, const CVector& point, RwMatrix* objectMatrix, bool ignoreBoundingChecks = false);
    FxSystem_c* CreateFxSystem(FxSystemBP_c* systemBP, const CVector& point, RwMatrix* objectMatrix, bool ignoreBoundingChecks = false);
    FxSystem_c* CreateFxSystem(FxSystemBP_c* systemBP, const RwMatrix& transform, RwMatrix* objectMatrix, bool ignoreBoundingChecks = false);

    FxMemoryPool_c& GetMemPool() { return m_Pool; }

    template <typename Type>
    Type* Allocate(int32 count) {
        const auto size = sizeof(Type) * count;
        const auto align = std::min(sizeof(Type), sizeof(int32));
        return (Type*)GetMemPool().GetMem(size, align);
    }
};

// Layout check: gta-reversed VALIDATE_SIZE(FxManager_c, 0xBC), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(FxManager_c) == 0xBC, "FxManager_c layout drift");
#endif

extern FxManager_c& g_fxMan;
