// CAnimBlendAssociation - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendAssociation.h
//
// Running animation of a clump (usually peds), created from a CAnimBlendHierarchy.
// The sequence/frame data is copied from the hierarchy to the association when
// a clump requests an animation. The association is destroyed when the clump
// stops playing it; the hierarchy lives on until CStreaming unloads the IFP.
//
// Adaptations:
// - Stripped InjectHooks()/friend InjectHooksMain, Constructor0..3 placement
//   wrappers, NOTSA_EXPORT_VTABLE.
// - VALIDATE_SIZE(0x3C) -> guarded static_assert (see bottom).
// - notsa::WEnumS16 -> AnimTypes.h C++17 shim; eAnimBlendCallbackType -> AnimTypes.h.
// - CDefaultAnimCallback + CAnimBlendLink kept (the intrusive list is game logic).
// - std::span -> notsa::span (C++17; see AnimTypes.h).
// - Iterator members that reference CAnimBlendAssociation are DEFINED OUT-OF-LINE
//   after the class (below). gta-reversed defines them in-class, which relies on
//   MSVC's deferred name lookup; standard two-phase lookup rejects the in-class
//   form because CAnimBlendAssociation is declared later in this header.
//   Semantics are unchanged.

#pragma once

#include "AnimTypes.h"

#include <cassert>
#include <cstddef> // offsetof
#include <cstdint>

class CAnimBlendNode;
class CAnimBlendHierarchy;
class CAnimBlendStaticAssociation;

enum eAnimationFlags {
    ANIMATION_DEFAULT                = 0,      //0x0,
    ANIMATION_IS_PLAYING             = 1 << 0, //0x1,
    ANIMATION_IS_LOOPED              = 1 << 1, //0x2,
    ANIMATION_IS_BLEND_AUTO_REMOVE   = 1 << 2, //!< (0x4) Automatically `delete this` once faded out (`m_BlendAmount <= 0 && m_BlendDelta <= 0`)
    ANIMATION_IS_FINISH_AUTO_REMOVE  = 1 << 3, //0x8,  // Animation will be stuck on last frame, if not set
    ANIMATION_IS_PARTIAL             = 1 << 4, //0x10, // TODO: Flag name is possibly incorrect? Following the usual logic (like `ANIMATION_MOVEMENT`), it should be `ANIMATION_GET_IN_CAR` (See  `RemoveGetInAnims`)
    ANIMATION_IS_SYNCRONISED         = 1 << 5, //0x20,
    ANIMATION_CAN_EXTRACT_VELOCITY   = 1 << 6, //0x40,
    ANIMATION_CAN_EXTRACT_X_VELOCITY = 1 << 7, //0x80,

    // ** User defined flags **
    ANIMATION_WALK                      = 1 << 8,  //0x100,
    ANIMATION_200                       = 1 << 9,  //0x200,
    ANIMATION_DONT_ADD_TO_PARTIAL_BLEND = 1 << 10, //0x400, // Possibly should be renamed to ANIMATION_IDLE, see `CPed::PlayFootSteps()`
    ANIMATION_IS_FRONT                  = 1 << 11, //0x800,
    ANIMATION_SECONDARY_TASK_ANIM       = 1 << 12, //0x1000,
    // **

    ANIMATION_IGNORE_ROOT_TRANSLATION = 1 << 13, //0x2000,
    ANIMATION_REFERENCE_BLOCK         = 1 << 14, //0x4000,
    ANIMATION_FACIAL                  = 1 << 15, //0x8000 // The animation is never destroyed if this flag is set, NO MATTER WHAT
};

class CDefaultAnimCallback {
public:
    static void DefaultAnimCB(class CAnimBlendAssociation* animAssoc, void* something) {
        // nothing here
    }
};

class CAnimBlendLink {
public:
    template<typename Y>
    struct BaseIterator {
    public:
        using iterator_category = std::forward_iterator_tag; // Actually it's bidirectional, but there are quirks, so let's pretend like its not
        using difference_type   = std::ptrdiff_t;
        using value_type        = Y;
        using pointer           = Y*;
        using reference         = Y&;

        BaseIterator() = default;
        BaseIterator(CAnimBlendLink* link) : m_Link{ link } {}

        // Defined out-of-line after CAnimBlendAssociation (see note at top).
        reference operator*() const;
        pointer operator->();
        auto& operator++();
        auto operator++(int);

        friend bool operator==(const BaseIterator<Y>& lhs, const BaseIterator<Y>& rhs) { return lhs.m_Link == rhs.m_Link; }
        friend bool operator!=(const BaseIterator<Y>& lhs, const BaseIterator<Y>& rhs) { return !(lhs == rhs); }
    private:
        auto DeRefLink() const; // defined out-of-line after CAnimBlendAssociation
    private:
        CAnimBlendLink* m_Link;
    };

    using iterator       = BaseIterator<CAnimBlendAssociation>;
    using const_iterator = BaseIterator<const CAnimBlendAssociation>;

    CAnimBlendLink* next{};
    CAnimBlendLink* prev{};

    CAnimBlendLink() = default;

    void Init() {
        next = nullptr;
        prev = nullptr;
    }

    void Prepend(CAnimBlendLink* link) {
        if (next) {
            next->prev = link;
        }
        link->next = next;
        link->prev = this;
        next = link;
    }

    void Remove() {
        if (prev) {
            prev->next = next;
        }
        if (next) {
            next->prev = prev;
        }
        Init();
    }

    bool IsEmpty() const { return !next; }

    auto begin() { return iterator{this}; }
    auto end() { return iterator{nullptr}; }
};

/*!
 * @brief Running animation of a clump (Usually peds), created from an `CAnimBlendHierarchy`.
 *
 * @details The sequence/frames data is copied from `CAnimBlendHierarchy` to `CAnimBlendAssociation` when a clump requests an animation.
 * @details The instance of `CAnimBlendAssociation` gets destroyed when the ped/clump stops playing the animation.
 * @details But `CAnimBlendHierarchy` is never destroyed and stays in memory unless `CStreaming` forces the IFP to unload (to create space in memory)
 *
 * @details A clump can have one, or more, instances of this class. Usually there's only 1 primary animation,
 * @details but there are also partial animations, which can be played alongside primary animations, like hand gestures or smoking.
 */
class CAnimBlendAssociation {
public:
    CAnimBlendLink                m_Link;          //!< Link to the next association of the clump
    uint16_t                      m_NumBlendNodes; //!< Number of bones this anim moves
    notsa::WEnumS16<AssocGroupId> m_AnimGroupId;   //!< Anim's group
    CAnimBlendNode*               m_BlendNodes;    //!< Node per-node animations - NOTE: Order of these depends on order of nodes in Clump this was built from
    CAnimBlendHierarchy*          m_BlendHier;     //!< The animation hierarchy this association was created from
    float                         m_BlendAmount;   //!< How much this animation is blended
    float                         m_BlendDelta;    //!< How much `BlendAmount` changes over time
    float                         m_CurrentTime;   //!< Current play time
    float                         m_Speed;         //!< Play speed
    float                         m_TimeStep;      //!< Time-per-tick
    notsa::WEnumS16<AnimationId>  m_AnimId;        //!< Anim's ID
    uint16_t                      m_Flags;         //!< Flags

    // Callback shit
    eAnimBlendCallbackType m_nCallbackType;
    void (*m_pCallbackFunc)(CAnimBlendAssociation*, void*);
    void* m_pCallbackData;

public:
    CAnimBlendAssociation();
    CAnimBlendAssociation(RpClump* clump, CAnimBlendHierarchy* animHierarchy);
    CAnimBlendAssociation(CAnimBlendAssociation& assoc);
    explicit CAnimBlendAssociation(CAnimBlendStaticAssociation& assoc);

    virtual ~CAnimBlendAssociation();

    float GetTimeProgress() const;
    void  SetBlendAmount(float a) { m_BlendAmount = a; }
    float GetBlendAmount(float weight = 1.f) const { return IsPartial() ? m_BlendAmount : m_BlendAmount * weight; }
    float GetBlendDelta() const { return m_BlendDelta; }

    AnimationId GetAnimId() const { return m_AnimId; }

    [[nodiscard]] bool IsPlaying() const { return (m_Flags & ANIMATION_IS_PLAYING) != 0; }
    [[nodiscard]] bool IsLooped() const { return (m_Flags & ANIMATION_IS_LOOPED) != 0; }
    [[nodiscard]] bool IsPartial() const { return (m_Flags & ANIMATION_IS_PARTIAL) != 0; }
    [[nodiscard]] bool IsSyncronised() const { return (m_Flags & ANIMATION_IS_SYNCRONISED) != 0; }
    [[nodiscard]] bool CanExtractXVelocity() const { return (m_Flags & ANIMATION_CAN_EXTRACT_X_VELOCITY) != 0; }
    [[nodiscard]] bool CanExtractVelocity() const { return (m_Flags & ANIMATION_CAN_EXTRACT_VELOCITY) != 0; }
    [[nodiscard]] bool IsFacial() const { return (m_Flags & ANIMATION_FACIAL) != 0; }

    void AllocateAnimBlendNodeArray(int32_t count);
    void FreeAnimBlendNodeArray();

    void Init(RpClump* clump, CAnimBlendHierarchy* hierarchy);
    void Init(CAnimBlendAssociation& source);
    void Init(CAnimBlendStaticAssociation& source);

    void ReferenceAnimBlock();
    void SetBlendDelta(float value) { m_BlendDelta = value; }
    void SetBlend(float blendAmount, float blendDelta);
    void SetBlendTo(float blendAmount, float blendDelta);
    void SetCurrentTime(float currentTime);
    float GetCurrentTime() const { return m_CurrentTime; }

    void SetDeleteCallback(void(*callback)(CAnimBlendAssociation*, void*), void* data = nullptr);
    void SetDefaultDeleteCallback() { SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr); }

    void SetFinishCallback(void(*callback)(CAnimBlendAssociation*, void*), void* data = nullptr);
    void SetDefaultFinishCallback() { SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr); }

    void Start(float currentTime = 0.f);

    /*!
     * @addr 0x4CEB40
     * @brief Sync the play time of this animation with another
    */
    void SyncAnimation(CAnimBlendAssociation* syncWith);
    bool UpdateBlend(float timeStep);
    bool UpdateTime(float timeStep, float timeMult);
    void UpdateTimeStep(float speedMult, float timeMult);
    bool HasFinished() const;
    [[nodiscard]] uint32_t GetHashKey() const noexcept;

    // NOTSA
    void SetFlag(eAnimationFlags flag, bool value = true) {
        if (value)
            m_Flags |= (int)flag;
        else
            m_Flags &= ~(int)flag;
    }

    bool HasFlag(eAnimationFlags flag) const { return m_Flags & flag; }

    static CAnimBlendAssociation* FromLink(CAnimBlendLink* link) {
        return (CAnimBlendAssociation*)((uint8_t*)link - offsetof(CAnimBlendAssociation, m_Link));
    }

    auto GetSpeed() const { return m_Speed; }
    void SetSpeed(float speed) { m_Speed = speed; }

    notsa::span<CAnimBlendNode> GetNodes();
    // 0x4CEB60 - declaration only: the inline body would instantiate
    // notsa::span<CAnimBlendNode>::operator[] with CAnimBlendNode still
    // incomplete here (Node.h includes this header). Defined in the .cpp.
    CAnimBlendNode*             GetNode(int32_t nodeIndex);
    CAnimBlendNode*             GetNodesPtr() { return m_BlendNodes; }

    auto& GetLink() { return m_Link; }
    auto  GetHier() const { return m_BlendHier; }
};

// ---------------------------------------------------------------------------
// Out-of-line definitions of CAnimBlendLink::BaseIterator members that
// reference CAnimBlendAssociation (see the adaptation note at the top).
// ---------------------------------------------------------------------------
template<typename Y>
auto CAnimBlendLink::BaseIterator<Y>::DeRefLink() const {
    return CAnimBlendAssociation::FromLink(m_Link);
}

template<typename Y>
typename CAnimBlendLink::BaseIterator<Y>::reference CAnimBlendLink::BaseIterator<Y>::operator*() const {
    return *DeRefLink();
}

template<typename Y>
typename CAnimBlendLink::BaseIterator<Y>::pointer CAnimBlendLink::BaseIterator<Y>::operator->() {
    return DeRefLink();
}

template<typename Y>
auto& CAnimBlendLink::BaseIterator<Y>::operator++() {
    assert(m_Link);
    m_Link = DeRefLink()->GetLink().next;
    return *this;
}

template<typename Y>
auto CAnimBlendLink::BaseIterator<Y>::operator++(int) {
    const auto tmp{ *this };
    ++(*this);
    return tmp;
}

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendAssociation) == 0x3C, "CAnimBlendAssociation layout drift");
#endif
