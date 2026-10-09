// CAnimBlendClumpData - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendClumpData.h
//
// Per-clump animation data: the list of running CAnimBlendAssociations plus
// the frame data array.
//
// Adaptations:
// - Stripped InjectHooks(), Constructor()/Destructor() placement wrappers.
// - VALIDATE_SIZE(0x14) -> guarded static_assert (see bottom).
// - CAnimBlendLink -> from CAnimBlendAssociation.h.
// - AnimBlendFrameData/CVector -> AnimTypes.h.
// - std::span/std::invoke -> notsa::span (C++17; see AnimTypes.h) / <functional>.

#pragma once

#include "AnimTypes.h"
#include "CAnimBlendAssociation.h"

#include <cassert>
#include <cstdint>
#include <functional>

class CAnimBlendClumpData {
public:
    CAnimBlendLink m_AnimList; //!< List of `CAnimBlendAssociation` - List of anims that are being played on this clump
    union {
        uint32_t m_NumFrameData; // For skinned clumps
        uint32_t m_NumBones;      // For non-skinned clumps
    };
    CVector*            m_PedPosition;
    AnimBlendFrameData* m_FrameDatas; // There's always at least 1 frame present

public:
    CAnimBlendClumpData();
    ~CAnimBlendClumpData();

    void ForAllFrames(void (*callback)(AnimBlendFrameData*, void*), void* data);

    /*!
     * @notsa
     * @brief Iterate all frames (Using a functor, usually a lambda)
     * @param Fn The functor to be called
    */
    template<typename Functor>
    void ForAllFramesF(Functor&& Fn) {
        for (auto& frame : notsa::span<AnimBlendFrameData>{ m_FrameDatas, m_NumFrameData }) {
            std::invoke(Fn, &frame);
        }
    }

    auto& GetAnims() { return m_AnimList; }

private:
    void ForAllFramesInSPR(void (*callback)(AnimBlendFrameData*, void*), void* data, uint32_t a3);
    void LoadFramesIntoSPR();

public:
    AnimBlendFrameData& GetRootFrameData() const { assert(m_NumFrameData >= 1); return m_FrameDatas[0]; }
    void SetNumberOfBones(uint32_t numBones);
    auto GetFrames() const { return m_FrameDatas; }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendClumpData) == 0x14, "CAnimBlendClumpData layout drift");
#endif
