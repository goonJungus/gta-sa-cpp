// CAnimBlendHierarchy - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendHierarchy.h
//
// The animation object: per-bone CAnimBlendSequences. The data is copied to a
// CAnimBlendAssociation when a clump requests an animation. The hierarchy is
// never destroyed except when CStreaming unloads the IFP to free up memory.
//
// Adaptations:
// - Stripped InjectHooks()/friend InjectHooksMain, Constructor()/Destructor()
//   placement wrappers.
// - VALIDATE_SIZE(0x18) -> guarded static_assert (see bottom).
// - `#undef MoveMemory` kept (defensive; Windows headers define it as a macro).
// - std::span -> notsa::span (C++17; see AnimTypes.h).
// - CLink<T> comes from ColTypes.h (collision subsystem) via AnimTypes.h.

#pragma once

#include "AnimTypes.h"
#include "CAnimBlendSequence.h"

#include <algorithm> // std::max
#include <cstdint>

#undef MoveMemory

/*!
 * @brief The animation object.
 *
 * @details It contains `CAnimBlendSequence`'s each of which is the animation for one bone (node).
 * @details The data from here is copied to `CAnimBlendAssociation` when an animation is requested for a clump.
 * @details It is never destroyed and stays in memory unless `CStreaming` forces the IFP to unload to free up memory.
 */
class CAnimBlendHierarchy {
public:
    uint32_t              m_hashKey;
    CAnimBlendSequence*   m_pSequences; //!< Per-node animations - NOTE: Order of these depends on the order of nodes in Clump this was built from
    uint16_t              m_nSeqCount;
    bool                  m_bIsCompressed;
    bool                  m_bKeepCompressed;
    int32_t               m_nAnimBlockId;
    float                 m_fTotalTime;
    CLink<CAnimBlendHierarchy*>* m_Link; //!< Link to the next animation in the block (?)

public:
    CAnimBlendHierarchy();
    ~CAnimBlendHierarchy();

    void Shutdown();

    uint8_t* AllocSequenceBlock(bool compressed) const;

    //! @addr 0x4CF2F0
    void CalcTotalTime() { ICalcTotalTime<false>(); }

    //! @addr 0x4CF3E0
    void CalcTotalTimeCompressed() { ICalcTotalTime<true>(); }

    void RemoveAnimSequences();
    void RemoveQuaternionFlips() const;
    void RemoveUncompressedData();

    void SetName(const char* string);
    void Uncompress();

    CAnimBlendSequence* FindSequence(const char* name) const;
    void* GetSequenceBlock() const;
    void CompressKeyframes() const;

    void MoveMemory();
    void Print();

    auto GetSequences() const { return notsa::span<CAnimBlendSequence>{ m_pSequences, (size_t)m_nSeqCount }; }
    auto GetHashKey() const { return m_hashKey; }
    auto GetTotalTime() const { return m_fTotalTime; }
    bool IsRunningCompressed() const { return m_bKeepCompressed; }
    bool IsUncompressed() const { return !m_bIsCompressed; }
    void SetNumSequences(size_t n);

    uint32_t GetIndex() const;

private: // Function implementations
    template<bool Compressed>
    void ICalcTotalTime() {
        m_fTotalTime = 0.0f;
        for (auto& seq : GetSequences()) {
            if (seq.m_FramesNum == 0) { // FIX_BUGS by Mitchell Tobass
                continue;
            }
            m_fTotalTime = std::max<float>(m_fTotalTime, seq.GetKeyFrame<Compressed>(seq.m_FramesNum - 1)->DeltaTime);
            for (auto j = seq.m_FramesNum; j --> 1;) {
                const auto kfA = seq.GetKeyFrame<Compressed>(j - 1);
                const auto kfB = seq.GetKeyFrame<Compressed>(j);
                kfB->DeltaTime = kfB->DeltaTime - kfA->DeltaTime; // TODO/NOTE: With `FixedFloat` this has unnecessary conversions float <=> int
            }
        }
    }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendHierarchy) == 0x18, "CAnimBlendHierarchy layout drift");
#endif
