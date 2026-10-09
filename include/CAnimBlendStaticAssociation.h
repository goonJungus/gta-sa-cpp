// CAnimBlendStaticAssociation - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendStaticAssociation.h
//
// Static animation data loaded from an animation clump.
//
// Adaptations:
// - Stripped InjectHooks(), NOTSA_EXPORT_VTABLE, Constructor1/2/Destructor()
//   placement wrappers.
// - VALIDATE_SIZE(0x14) -> guarded static_assert (see bottom).
// - notsa::WEnumS16 -> AnimTypes.h C++17 shim.

#pragma once

#include "AnimTypes.h"
#include "CAnimBlendHierarchy.h"

#include <cstdint>

class CAnimBlendSequence;

//! Stores static animation data loaded from an animation clump.
class CAnimBlendStaticAssociation {
public:
    CAnimBlendStaticAssociation() = default;
    CAnimBlendStaticAssociation(RpClump* clump, CAnimBlendHierarchy* hier);
    virtual ~CAnimBlendStaticAssociation();

    void Init(RpClump* clump, CAnimBlendHierarchy* hier);
    void AllocateSequenceArray(int32_t count);
    void FreeSequenceArray();

    auto GetHashKey() const noexcept { return m_BlendHier->m_hashKey; }

    bool IsValid() const { return !!m_BlendSeqs; } // vanilla sa, inlined function
    auto GetAnimHierarchy() const { return m_BlendHier; }

public:
    uint16_t                       m_NumBlendNodes{};
    notsa::WEnumS16<AnimationId>   m_AnimId{ANIM_ID_UNDEFINED};
    notsa::WEnumS16<AssocGroupId>  m_AnimGroupId{ANIM_GROUP_NONE};
    uint16_t                       m_Flags{};
    CAnimBlendSequence**           m_BlendSeqs{};
    CAnimBlendHierarchy*           m_BlendHier{};
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendStaticAssociation) == 0x14, "CAnimBlendStaticAssociation layout drift");
#endif
