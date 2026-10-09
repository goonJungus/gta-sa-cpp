// CAnimBlendNode - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendNode.h
//
// Animation of a single node (bone). Each node has a sequence of key-frames.
//
// Adaptations:
// - Stripped InjectHooks().
// - VALIDATE_SIZE(0x18) -> guarded static_assert (see bottom).
// - NOTSA_FORCEINLINE dropped (plain inline templates; the macro is plugin-sdk).
// - CQuaternion -> minimal stand-in in AnimTypes.h (full class is a TODO there);
//   lerp -> trivial template in AnimTypes.h.
// - The I_* implementation templates are kept verbatim: they are the core
//   per-frame interpolation logic. They are only instantiated when the public
//   wrappers are called, and nothing in the stub build calls them. The
//   compressed-path key-frame ops resolve via the AnimTypes.h shims
//   (CompressedQuat::operator CQuaternion, FixedFloat::operator float,
//   FixedVector::operator CVector).

#pragma once

#include "AnimTypes.h"
#include "CAnimBlendSequence.h"
#include "CAnimBlendAssociation.h"

#include <cassert>
#include <cstdint>

/*!
 * @brief Represents the animation of a single node (bone)
 *
 * @details An animation can move one or more nodes (bones).
 * @details Each node has a sequence (that contain key-frames)
 */
class CAnimBlendNode {
public:
    void Init();

    void CalcTheta(float angle);

    //! @addr 0x4D0190
    void CalcDeltas();

    //! @addr 0x4D0350
    void CalcDeltasCompressed();


    //! @addr 0x4CFC50
    void GetCurrentTranslation(CVector& trans, float weight) { return I_GetCurrentTranslation<false>(trans, weight); }

    //! @addr 0x4CFE60
    void GetCurrentTranslationCompressed(CVector& trans, float weight) { return I_GetCurrentTranslation<true>(trans, weight); }


    //! @addr 0x4CFD90
    void GetEndTranslation(CVector& trans, float weight) { return I_GetEndTranslation<false>(trans, weight); }

    //! @addr 0x4D0000
    void GetEndTranslationCompressed(CVector& trans, float weight) { return I_GetEndTranslation<true>(trans, weight); }


    //! @addr 0x4D04A0
    bool NextKeyFrame() { return I_NextKeyFrame<false>(); }

    //! @addr 0x4D0570
    bool NextKeyFrameCompressed() { return I_NextKeyFrame<true>(); }

    //! @addr 0x4CFB90
    bool NextKeyFrameNoCalc();


    //! @addr 0x4D06C0
    bool Update(CVector& trans, CQuaternion& rot, float weight) { return I_Update<false>(trans, rot, weight); }

    //! @addr 0x4D08D0
    bool UpdateCompressed(CVector& trans, CQuaternion& rot, float weight) { return I_Update<true>(trans, rot, weight); }

    bool UpdateTime(); // Unused
    bool SetupKeyFrameCompressed();
    bool FindKeyFrame(float time);
    auto GetAnimAssoc() const { return m_BlendAssoc; }
    bool IsValid() const { return !!m_Seq; }
    auto GetSeq() const { return m_Seq; }
    auto GetRootKF() const { return &m_Seq[0]; }

    //! @notsa
    //! @brief Inverse progress of current frame (Eg.: How much is left of this frame)
    template<bool IsCompressed>
    float GetTimeRemainingProgress() const {
        const auto kf = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
        return kf->DeltaTime == 0.0f
            ? 0.0f
            : (kf->DeltaTime - m_KFRemainingTime) / kf->DeltaTime;
    }

    template<bool IsCompressed>
    void I_GetCurrentTranslation(CVector& trans, float weight) {
        trans = CVector{0.0f, 0.0f, 0.0f};

        if (!m_Seq->m_bHasTranslation) { // Moved up here
            return;
        }

        const float blend = m_BlendAssoc->GetBlendAmount(weight);
        if (blend <= 0.0f) {
            return;
        }

        const auto kfA = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr),
                   kfB = m_Seq->GetKeyFrame<IsCompressed>(m_KFPrev);
        trans = lerp<CVector>(kfB->Trans, kfA->Trans, GetTimeRemainingProgress<IsCompressed>()) * blend;
    }

    template<bool IsCompressed>
    void I_GetEndTranslation(CVector& trans, float weight) {
        trans = CVector{ 0.0f, 0.0f, 0.0f };

        if (!m_Seq->m_bHasTranslation) { // Moved up here
            return;
        }

        const float blend = m_BlendAssoc->GetBlendAmount(weight);
        if (blend <= 0.0f) {
            return;
        }

        const auto kf = m_Seq->GetKeyFrame<IsCompressed>(m_Seq->m_FramesNum - 1);
        trans = kf->Trans;
        trans *= blend;
    }

    template<bool IsCompressed>
    bool I_NextKeyFrame() {
        if (m_Seq->m_FramesNum <= 1) {
            return false;
        }

        // Store old KF
        m_KFPrev = m_KFCurr;

        // Find next frame, this should usually be the next one
        // Unless `m_BlendAssoc->m_TimeStep > NextKF->DeltaTime` (In which case `m_KFRemainingTime` becomes negative)
        bool looped = false;
        while (m_KFRemainingTime <= 0.0f) {
            m_KFCurr++;

            if (m_KFCurr >= m_Seq->m_FramesNum) {
                // reached end of animation
                if (!m_BlendAssoc->IsLooped()) {
                    m_KFCurr--;
                    m_KFRemainingTime = 0.0f;
                    return false;
                }
                looped   = true;
                m_KFCurr = 0;
            }

            m_KFRemainingTime += m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr)->DeltaTime;
        }

        m_KFPrev = m_KFCurr == 0
            ? m_Seq->m_FramesNum - 1 // Previous frame was the last in the sequence
            : m_KFCurr - 1;

        if constexpr (IsCompressed) {
            CalcDeltasCompressed();
        } else {
            CalcDeltas();
        }

        return looped;
    }

    template<bool IsCompressed>
    bool I_Update(CVector& trans, CQuaternion& rot, float weight) {
        bool looped = false;

        trans = CVector{0.0f, 0.0f, 0.0f};
        rot   = CQuaternion{0.0f, 0.0f, 0.0f, 0.0f};

        if (m_BlendAssoc->IsPlaying()) {
            m_KFRemainingTime -= m_BlendAssoc->m_TimeStep;
            if (m_KFRemainingTime <= 0.0f) {
                looped = I_NextKeyFrame<IsCompressed>();
            }
        }

        const float blend = m_BlendAssoc->GetBlendAmount(weight);
        if (blend > 0.0f) {
            const auto kfA = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
            const auto kfB = m_Seq->GetKeyFrame<IsCompressed>(m_KFPrev);

            // Calculate interpolation t
            const float t = GetTimeRemainingProgress<IsCompressed>();

            // Sanity check
            assert(m_Seq->m_bHasTranslation || m_Seq->m_bHasRotation);

            // Update translation
            if (m_Seq->m_bHasTranslation) {
                trans = lerp<CVector>(kfB->Trans, kfA->Trans, t) * blend;
            }

            // Update rotation
            if (m_Seq->m_bHasRotation) {
                rot.Slerp(kfB->Rot, kfA->Rot, m_Theta, m_InvSinTheta, t);
                rot *= blend;
            }
        }

        return looped;
    }

public:
    float                  m_Theta{};           //!< Angle between current and previous key-frames
    float                  m_InvSinTheta{};     //!< 1 / sin(m_Theta), used in slerp calculation
    int16_t                m_KFCurr{ -1 };      //!< Current key frame (A)
    int16_t                m_KFPrev{ -1 };      //!< Previous key frame (B)
    float                  m_KFRemainingTime{}; //!< Time until frames have to advance (So, basically time left from current key-frame, so <frame progress> = m_KFRemainingTime / KeyFrameCurr->DeltaTime)
    CAnimBlendSequence*    m_Seq{};             //!< Node animation key-frames
    CAnimBlendAssociation* m_BlendAssoc{};      //!< Parent anim to which this node belongs to
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendNode) == 0x18, "CAnimBlendNode layout drift");
#endif
