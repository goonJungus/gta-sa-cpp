// FxManager_c - particle effect manager (clean-room C++ port)
// Decompiled: src/FxManager_c/*.c (gta_sa.exe, Ghidra export)
// Reference: gta-reversed/source/game_sa/Fx/FxManager.cpp (logic cross-check)
//
// Portability notes:
// - Methods that only touch ported types (CFileMgr, CTxdStore, FxSystem_c,
//   TList_c) are fully implemented below.
// - Methods that need unported types (FxSystemBP_c, FxEmitterPrt_c,
//   FxInterpInfo255_c, FxSphere_c, Fx_c, RenderWare) keep their converted
//   logic in `#if 0` blocks with TODO(port) naming the missing pieces, and a
//   compilable stub body outside. This follows the CFileLoader.cpp precedent.
// - The decompiled code calls two TList_c<FxSystem_c> operations through
//   mis-attributed FxInterpInfo255_c statics (Ghidra nearest-neighbour
//   inference): unk_004a8df0 = add-to-head, unk_004a8e30 = remove. They are
//   implemented here as TU-local helpers (FxListAddHead/FxListRemove).

#include "FxManager.h"
#include "CTxdStore.h"

#include <cstdio>  // sscanf
#include <cstring> // strcpy, strlen, strncmp

// ---------------------------------------------------------------------------
// TU-local TList_c<FxSystem_c> helpers (decompiled unk_004a8df0 / unk_004a8e30)
// ---------------------------------------------------------------------------
static void FxListAddHead(TList_c<FxSystem_c>* list, FxSystem_c* item) {
    FxSystem_c* oldHead = list->m_head;
    list->m_head = item;
    item->m_pPrev = nullptr;
    item->m_pNext = oldHead;
    if (oldHead)
        oldHead->m_pPrev = item;
    else
        list->m_tail = item;
    ++list->m_cnt;
}

static void FxListRemove(TList_c<FxSystem_c>* list, FxSystem_c* item) {
    FxSystem_c* next = item->m_pNext;
    FxSystem_c* prev = item->m_pPrev;
    if (next)
        next->m_pPrev = prev;
    else
        list->m_tail = prev;
    if (prev)
        prev->m_pNext = next;
    else
        list->m_head = next;
    --list->m_cnt;
}

// 0x4A9470 (Constructor)
FxManager_c::FxManager_c() {
    Constructor();
}

FxManager_c* FxManager_c::Constructor() {
#if 0
    // TODO(port): static one-time initializers, all unported -
    //   FxInterpInfo255_c static init (decompiled unk_004a8dd0) x3,
    //   FxSphere_c::FxSphere_c(), unk_004a9c10.
#endif
    m_FxEmitters = nullptr;
    return this;
}

// 0x4A90A0 (Destructor) - the decompiled body only calls no-op nullsubs
// (nullsub_004a9c20, FxInterpInfo255_c::nullsub_004a8de0 x2).
FxManager_c* FxManager_c::Destructor() {
    return this;
}

// 0x4A98E0
bool FxManager_c::Init() {
    m_nCurrentMatrix = 0;
#if 0
    // TODO(port): the rest needs unported pieces -
    //   FxMemoryPool_c::Init(&m_Pool)            (fx/FxMemoryPool.h unported;
    //                                             only GetMem is on the stand-in),
    //   RwMatrixCreate() x8 -> m_apMatrices[]    (RenderWare),
    //   new FxEmitterPrt_c[1000]                 (FxEmitterPrt_c unported),
    //   add each emitter to m_FxEmitterParticles (TList_c<Particle_c>).
#endif
    return true;
}

// 0x4A9A10
void FxManager_c::Exit() {
    DestroyAllFxSystems();
    m_FxEmitters = nullptr;
    CTxdStore::RemoveTxdSlot(m_nFxTxdIndex);
#if 0
    // TODO(port): the rest needs unported pieces -
    //   FxInterpInfo255_c static cleanup (decompiled unk_004a8eb0) x2,
    //   RwMatrixDestroy() x8 on m_apMatrices[]  (RenderWare),
    //   FxMemoryPool_c::Exit(&m_Pool)            (fx/FxMemoryPool.h unported).
#endif
}

// 0x4A9810
void FxManager_c::DestroyFxSystem(FxSystem_c* system) {
#if 0
    // TODO(port): particle reclamation needs unported types (FxSystemBP_c,
    // FxEmitterPrt_c, Particle_c internals). Converted logic:
    //   FxSystemBP_c* bp = system->m_SystemBP;
    //   for (i = 0; i < bp->m_nNumPrims; i++) {
    //       for (Particle_c* p = bp->m_Prims[i]->m_Particles; p; p = next) {
    //           next = p->m_pNext;
    //           if (p->m_pSystem == system) {
    //               remove p from its emitter list;   (unk_004a8e30)
    //               return p to the free pool;        (unk_004a8df0)
    //           }
    //       }
    //   }
#endif
    FxListRemove(&m_FxSystems, system);
    system->Exit();
    delete system;
}

// 0x4A98B0
void FxManager_c::DestroyAllFxSystems() {
    FxSystem_c* next;
    for (FxSystem_c* system = m_FxSystems.m_head; system; system = next) {
        next = system->m_pNext;
        DestroyFxSystem(system);
    }
}

// 0x5C2420
bool FxManager_c::LoadFxProject(const char* path) {
    // Build the txd path: the original overwrites the 4 chars before the
    // NUL terminator (the ".fxp" extension) with "PC.txd", so
    // "data/effects.fxp" becomes "data/effectsPC.txd".
    char txdPath[128];
    strcpy(txdPath, path);
    strcpy(txdPath + strlen(txdPath) - 4, "PC.txd");

    m_nFxTxdIndex = CTxdStore::AddTxdSlot("fx");
    CTxdStore::LoadTxd(m_nFxTxdIndex, txdPath);
    CTxdStore::AddRef(m_nFxTxdIndex);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(m_nFxTxdIndex);

    FILESTREAM file = CFileMgr::OpenFile(path, "rb");
    if (!file)
        return false;

    // The file is a sequence of "FX_SYSTEM_DATA:" sections. The original
    // skips the first two lines, then reads the section marker from every
    // third line (each LoadFxSystemBP consumes the id line itself).
    char line[256], token[128];
    CFileMgr::ReadLine(file, line, 0x100);
    sscanf(line, "%s", token);
    CFileMgr::ReadLine(file, line, 0x100);
    CFileMgr::ReadLine(file, line, 0x100);
    sscanf(line, "%s", token);
    // memcmp(token, "FX_SYSTEM_DATA:", 16) - 16 bytes, NUL included.
    if (strncmp(token, "FX_SYSTEM_DATA:", 16) == 0) {
        do {
            LoadFxSystemBP(path, file);
            CFileMgr::ReadLine(file, line, 0x100);
            CFileMgr::ReadLine(file, line, 0x100);
            sscanf(line, "%s", token);
        } while (strncmp(token, "FX_SYSTEM_DATA:", 16) == 0);
    }

    CFileMgr::CloseFile(file);
    CTxdStore::PopCurrentTxd();
#if 0
    // TODO(port): FxMemoryPool_c::Optimise(&m_Pool) - fx/FxMemoryPool.h unported.
#endif
    return true;
}

// 0x4A9AE0
void FxManager_c::UnloadFxProject() {
    DestroyAllFxSystems();
#if 0
    // TODO(port): the rest needs unported pieces -
    //   FxInterpInfo255_c static cleanup (decompiled unk_004a8eb0),
    //   FxMemoryPool_c::Reset(&m_Pool)              (fx/FxMemoryPool.h unported),
    //   new FxEmitterPrt_c[1000] -> m_FxEmitters    (FxEmitterPrt_c unported),
    //   add each emitter to m_FxEmitterParticles.
#endif
}

// 0x5C1F50
void FxManager_c::LoadFxSystemBP(const char* filename, FILESTREAM file) {
#if 0
    // TODO(port): needs FxSystemBP_c (fwd-declared only in FxSystem.h).
    // Converted logic:
    //   char line[256];
    //   int id;
    //   CFileMgr::ReadLine(file, line, 0x100);
    //   sscanf(line, "%d", &id);
    //   FxSystemBP_c* bp = new FxSystemBP_c();
    //   bp->Load(filename, file, id);
    //   add bp to the head of m_FxSystemBPs;   (decompiled unk_004a8df0)
#endif
    (void)filename; (void)file;
}

// 0x4A9360
FxSystemBP_c* FxManager_c::FindFxSystemBP(const char* name) {
#if 0
    // TODO(port): needs CKeyGen::GetUppercaseKey (unported) and
    // FxSystemBP_c::m_nNameKey (FxSystemBP_c unported). Converted logic:
    //   uint32 key = CKeyGen::GetUppercaseKey(name);
    //   for (FxSystemBP_c* bp = m_FxSystemBPs.m_head; bp; bp = bp->m_pNext) {
    //       if (bp->m_nNameKey == key)
    //           return bp;
    //   }
    //   debug("Cannot Find Fx System Blueprint - %s", name);
#endif
    (void)name;
    return nullptr;
}

// 0x4A93C0
Particle_c* FxManager_c::GetParticle(int8 primType) {
#if 0
    // TODO(port): the pool alloc is FxInterpInfo255_c::unk_004a8e70 (unported).
    // Converted logic:
    //   if (primType != 0)
    //       return nullptr;
    //   return (Particle_c*)FxInterpInfo255_c::unk_004a8e70();
#endif
    (void)primType;
    return nullptr;
}

// 0x4A93B0
void FxManager_c::ReturnParticle(FxEmitterPrt_c* emitter) {
#if 0
    // TODO(port): FxInterpInfo255_c::unk_004a8df0 (unported) returns the
    // emitter's particle to the free pool.
#endif
    (void)emitter;
}

// 0x4A9400
void FxManager_c::FreeUpParticle() {
#if 0
    // TODO(port): needs FxSystemBP_c, CGeneral::GetRandomNumber (both unported).
    // Converted logic: pick random live systems until one surrenders a particle.
    //   FxSystem_c* system;
    //   do {
    //       do {
    //           system = random entry from the system pool;
    //       } while (system->m_bAllocatedParentMat & 0x20);
    //   } while (!FxSystemBP_c::FreePrtFromSystem(system->m_SystemBP, system));
#endif
}

// 0x4A9A80
void FxManager_c::Update(RwCamera* camera, float timeDelta) {
    CalcFrustumInfo(camera);
#if 0
    // TODO(port): FxSystemBP_c::Update (FxSystemBP_c unported).
    //   for (FxSystemBP_c* bp = m_FxSystemBPs.m_head; bp; bp = bp->m_pNext)
    //       bp->Update(timeDelta);
#endif
    FxSystem_c* next;
    for (FxSystem_c* system = m_FxSystems.m_head; system; system = next) {
        next = system->m_pNext;
        // FxSystem_c::Update returns true when the system is finished.
        if (system->Update(camera, timeDelta))
            DestroyFxSystem(system);
    }
}

// 0x4A92A0
void FxManager_c::Render(RwCamera* camera, bool bHeatHaze) {
#if 0
    // TODO(port): needs RenderWare (RwEngineInstance render-state sets),
    // CCustomBuildingDNPipeline::m_fDNBalanceParam, FxSystemBP_c::Render.
    // Converted logic:
    //   float dnBalance = 1.0f - CCustomBuildingDNPipeline::m_fDNBalanceParam;
    //   m_bHeatHazeEnabled = false;
    //   RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, 1);   // 6,1
    //   RwRenderStateSet(rwRENDERSTATEZTESTENABLE, 0);    // 8,0
    //   RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, 0); // 0x1e,0
    //   for (bp in m_FxSystemBPs)
    //       bp->Render(camera, dnBalance * 0.6f + 0.4f, bHeatHaze);
    //   RwRenderStateSet(rwRENDERSTATEZTESTENABLE, 1);
    //   RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, 1);
#endif
    (void)camera; (void)bHeatHaze;
}

// 0x4A93E0
void FxManager_c::SetWindData(CVector* dir, float* speed) {
    m_pWindDir = dir;
    m_pfWindSpeed = speed;
}

// 0x4A9440 - hands out the next matrix from the 8-slot ring
RwMatrix* FxManager_c::FxRwMatrixCreate() {
    return m_apMatrices[m_nCurrentMatrix++];
}

// 0x4A9460 - the matrix arg is ignored; the ring just rewinds
void FxManager_c::FxRwMatrixDestroy(RwMatrix* matrix) {
    (void)matrix;
    --m_nCurrentMatrix;
}

// 0x4A9140
void FxManager_c::CalcFrustumInfo(RwCamera* camera) {
#if 0
    // TODO(port): needs the real RwCamera (viewWindow, farPlane, frustumPlanes)
    // and FxFrustumInfo_c internals (m_Sphere, m_Planes) - both unported.
    // Converted logic:
    //   Derives a bounding sphere + frustum planes from the camera: the sphere
    //   is centered along the camera forward axis at distance
    //     dist = sin(atan(sqrt(viewWindow.x^2 + viewWindow.y^2))) *
    //            (sqrt(viewWindow.x^2 + viewWindow.y^2 + 1) * farPlane /
    //             sin(180deg - 2*atan(sqrt(viewWindow.x^2+viewWindow.y^2)))),
    //   radius = dist, and m_Planes[] copies frustumPlanes[2..5].
#endif
    (void)camera;
}

// 0x4A9500
bool FxManager_c::ShouldCreate(FxSystemBP_c* system, const RwMatrix& transform,
                               RwMatrix* objectMatrix, bool ignoreBoundingChecks) {
#if 0
    // TODO(port): needs FxSystemBP_c::m_BoundingSphere, FxSphere_c, and
    // RenderWare matrix ops (RwMatrixMultiply, RwV3dTransformPoints).
    // Converted logic:
    //   if (ignoreBoundingChecks)
    //       return true;
    //   if (system->m_BoundingSphere) {
    //       compose the system matrix (transform * objectMatrix, or a copy
    //       of transform when objectMatrix is null) on the matrix stack;
    //       transform the bounding sphere into world space;
    //       return frustum/sphere visibility test on m_Frustum;
    //   }
    //   return true;
#endif
    (void)system; (void)transform; (void)objectMatrix; (void)ignoreBoundingChecks;
    return true;
}

// 0x4A95C0
FxSystem_c* FxManager_c::CreateFxSystem(FxSystemBP_c* systemBP, const RwMatrix& transform,
                                        RwMatrix* objectMatrix, bool ignoreBoundingChecks) {
#if 0
    // TODO(port): needs FxSystemBP_c, FxSystem_c::Init (declared, but the
    // definition needs the BP), Fx_c::GetFxQuality (unported). Converted logic:
    //   if (!systemBP || !ShouldCreate(systemBP, transform, objectMatrix, ignoreBoundingChecks))
    //       return nullptr;
    //   FxSystem_c* system = new FxSystem_c();
    //   system->Constructor();
    //   system->Init(systemBP, transform, objectMatrix);
    //   add system to the head of m_FxSystems;   (decompiled unk_004a8df0)
    //   switch (Fx_c::GetFxQuality()) {          // particle rate by quality
    //   case 0: system->SetRateMult(0.5f); break;
    //   case 1: system->SetRateMult(0.75f); break;
    //   case 2: system->SetRateMult(1.0f); break;
    //   default: break;                          // quality < 0: keep default rate
    //   }
    //   return system;
#endif
    (void)systemBP; (void)transform; (void)objectMatrix; (void)ignoreBoundingChecks;
    return nullptr;
}

// 0x4A96B0
FxSystem_c* FxManager_c::CreateFxSystem(FxSystemBP_c* systemBP, const CVector& point,
                                        RwMatrix* objectMatrix, bool ignoreBoundingChecks) {
#if 0
    // TODO(port): needs FxSystemBP_c, ShouldCreate, RwMatrix field access and
    // RwMatrixUpdate (RenderWare). Converted logic:
    //   if (!systemBP)
    //       return nullptr;
    //   RwMatrix* transform = FxRwMatrixCreate();  // from the 8-slot ring
    //   *transform = identity; transform->pos = point; transform->flags |= 0x20003;
    //   RwMatrixUpdate(transform);
    //   if (!ShouldCreate(systemBP, *transform, objectMatrix, ignoreBoundingChecks)) {
    //       FxRwMatrixDestroy(transform);
    //       return nullptr;
    //   }
    //   ... same new/Constructor/Init/add-to-list/rate-mult as the
    //   transform overload above ...
    //   FxRwMatrixDestroy(transform);
    //   return system;
#endif
    (void)systemBP; (void)point; (void)objectMatrix; (void)ignoreBoundingChecks;
    return nullptr;
}

// 0x4A9BB0
FxSystem_c* FxManager_c::CreateFxSystem(const char* name, const RwMatrix& transform,
                                        RwMatrix* objectMatrix, bool ignoreBoundingChecks) {
    return CreateFxSystem(FindFxSystemBP(name), transform, objectMatrix, ignoreBoundingChecks);
}

// 0x4A9BE0
FxSystem_c* FxManager_c::CreateFxSystem(const char* name, const CVector& point,
                                        RwMatrix* objectMatrix, bool ignoreBoundingChecks) {
    return CreateFxSystem(FindFxSystemBP(name), point, objectMatrix, ignoreBoundingChecks);
}
