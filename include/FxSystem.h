// FxSystem_c - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Fx/FxSystem.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations:
// - stripped InjectHooks()
// - ListItem_c<T>: minimal local base (m_pPrev/m_pNext). TODO: move to
//   Core/ListItem_c.h (and TList_c to Core/List_c.h) with the core-containers
//   subsystem - gta-reversed's version uses C++20 (std::predicate, deduced this).
// - CAEFireAudioEntity: size-verified (0x88) opaque stand-in. TODO: replace with the
//   real Audio/Entities/AEFireAudioEntity.h when the audio subsystem is ported.
// - eBoneTag: TODO port Enums/eBoneTag.h; aliased to int32 for now.
// - GetPrims(): original returns std::span<FxPrim_c*> (C++20); the clean-room build
//   is C++17 - adapted to pointer + out-count.
// - KillAndClear/SafeKillAndClear: `requires` is C++20 - adapted to C++17 SFINAE.

#pragma once

#include "GrTypes.h" // int8..uint32
#include "CVector.h"  // CVector
#include "CMatrix.h"  // RwMatrix
#include "AnimTypes.h" // eBoneTag (canonical)

#include <cstddef>     // size_t
#include <type_traits> // std::is_base_of_v, std::enable_if_t

class CPedGroup;
class CPed;
class CEntity;

struct RwCamera; // CCamera.h precedent: forward-declared until the RenderWare layer lands

// TODO: port Core/ListItem_c.h with the core-containers subsystem.
template<typename T> // T should be derived from `ListItem_c`
class ListItem_c {
public:
    ListItem_c() = default;
    ~ListItem_c() = default;

    T* m_pPrev{};
    T* m_pNext{};
};

// TODO: port Audio/Entities/AEFireAudioEntity.h with the audio subsystem.
// Size-verified opaque stand-in (gta-reversed VALIDATE_SIZE 0x88).
// Delete this when the real header lands.
struct CAEFireAudioEntity {
    uint8 _opaque[0x88];
};

// eBoneTag: canonical enum from AnimTypes.h (deduped 2026-10-09)

enum eFxSystemKillStatus : uint8 {
    FX_NOT_KILLED    = 0,
    FX_PLAY_AND_KILL = 1, // DESTROY_AFTER_FINISHING
    FX_KILLED        = 2,
    FX_3             = 3,
};

enum class eFxSystemPlayStatus : uint8 {
    FX_PLAYING = 0,
    FX_STOPPED = 1,
    T2         = 2,
};

class FxSystemBP_c;
class FxPrtMult_c;
class FxSphere_c;
class FxPrim_c;
class FxPrimBP_c;
class FxBox_c;
struct Particle_c;

class FxSystem_c : public ListItem_c<FxSystem_c> {
public:
    FxSystemBP_c* m_SystemBP;
    RwMatrix*     m_ParentMatrix;
    RwMatrix      m_LocalMatrix; // aka Offset Mat

    eFxSystemPlayStatus m_nPlayStatus;
    eFxSystemKillStatus m_nKillStatus;

    bool   m_UseConstTime;
    float  m_fCurrentTime;
    float  m_fCameraDistance;
    uint16 m_nConstTime;

    uint16 m_nRateMult;
    uint16 m_nTimeMult;

    uint8 m_allocatedParentMat : 1; //  m_bOwnedParentMatrix
    uint8 m_createLocal : 1;
    uint8 m_useZTest : 1;
    uint8 m_stopParticleCreation : 1;
    uint8 m_prevCulled : 1;
    uint8 m_MustCreateParticles : 1;

    float              m_LoopInterval;
    CVector            m_VelAdd;
    FxSphere_c*        m_BoundingSphere;
    FxPrim_c**         m_Prims;
    CAEFireAudioEntity m_FireAE;

public:
    FxSystem_c();
    ~FxSystem_c();
    FxSystem_c* Constructor();
    FxSystem_c* Destructor();

    bool Init(FxSystemBP_c* systemBP, const RwMatrix& local, RwMatrix* parent);
    void Exit();

    void Play();
    void PlayAndKill();
    void Kill();
    void Pause();
    void Stop();

    void AttachToBone(CEntity* entity, eBoneTag boneId);

    void AddParticle(const CVector& pos, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ = -1.0f, float lightMult = 1.2f, float lightMultLimit = 0.6f, bool createLocal = false);
    void AddParticle(const RwMatrix& mat, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ = -1.0f, float lightMult = 1.2f, float lightMultLimit = 0.6f, bool createLocal = false);

    // 9-arg static form used by CPed::PreRenderAfterTest (decomp static-style call).
    // TODO(port): verify against original; first arg is the DAT_00a9ae30/38 global.
    static void AddParticle(int32_t fxSys, CVector* pos, CVector* vel, float timeSince, FxPrtMult_c* fxMults, float rotZ, float lightMult, float lightMultLimit, bool createLocal) {
        (void)fxSys; (void)pos; (void)vel; (void)timeSince; (void)fxMults;
        (void)rotZ; (void)lightMult; (void)lightMultLimit; (void)createLocal;
    }

    void EnablePrim(int32 primIndex, bool enable);
    void SetMatrix(RwMatrix* matrix);
    void SetOffsetPos(const CVector& pos);
    void AddOffsetPos(const CVector& pos);
    void SetConstTime(bool on, float time);
    void SetRateMult(float mult);
    void SetTimeMult(float mult);
    void SetVelAdd(const CVector& velocity);
    void CopyParentMatrix();
    void GetCompositeMatrix(RwMatrix* out) const;
    eFxSystemPlayStatus GetPlayStatus() const;

    uint32 ForAllParticles(void(*callback)(Particle_c*, int32, FxBox_c**), FxBox_c* data);
    static void UpdateBoundingBoxCB(Particle_c* particle, int32 arg1, FxBox_c** data);

    void GetBoundingBox(FxBox_c* out);
    bool GetBoundingSphereWld(FxSphere_c* out) const;
    bool GetBoundingSphereLcl(FxSphere_c* out) const;
    void SetBoundingSphere(FxSphere_c* sphere);
    void ResetBoundingSphere();

    void SetLocalParticles(bool enable);
    void SetZTestEnable(bool enable);
    void SetMustCreatePrts(bool enable);

    bool IsVisible() const;

    void DoFxAudio(CVector pos);
    bool Update(RwCamera* camera, float timeDelta);

public:
    // C++17 adaptation of the original `std::span<FxPrim_c*> GetPrims()`
    // (std::span is C++20; see header note).
    FxPrim_c** GetPrims(uint32& numPrims);

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<FxSystem_c, T>>>
    static void KillAndClear(T*& fx) {
        fx->Kill();
        fx = nullptr;
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<FxSystem_c, T>>>
    static void SafeKillAndClear(T*& fx) {
        if (fx) {
            KillAndClear(fx);
        }
    }
};
