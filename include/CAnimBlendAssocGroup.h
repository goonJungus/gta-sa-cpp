// CAnimBlendAssocGroup - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendAssocGroup.h
//
// Animation group (block) such as `ped`, `int_house` - maps AnimationIds and
// animation names to animation data.
//
// Adaptations:
// - Stripped InjectHooks(), Constructor()/Destructor() placement wrappers.
// - VALIDATE_SIZE(0x14) -> guarded static_assert (see bottom).
// - std::span -> notsa::span (C++17; see AnimTypes.h).
// - gta-reversed's Base.h include was only needed for CBaseModelInfo, which is
//   used solely as a parameter type here -> forward declaration instead
//   (the model-info subsystem is not yet converted).

#pragma once

#include "AnimTypes.h"
#include "CAnimBlendAssociation.h"
#include "CAnimBlendStaticAssociation.h"

#include <cstdint>

class CBaseModelInfo; // model-info subsystem (not yet converted); parameter type only

/*!
 * @brief Animation group (block)
 *
 * Represents a group (block) (Such as `ped`, `int_house`, etc).
 * Is also used for mapping `AnimationId`'s and animation names to animation data.
*/
class CAnimBlendAssocGroup {
public:
    CAnimBlendAssocGroup() = default;
    ~CAnimBlendAssocGroup();

    /*!
     * @addr 0x4CE0B0
     *
     * @brief Instantiate an animation
     *
     * @param name Name of the animation
     *
     * @return The uncompressed instantiated animation
    */
    CAnimBlendAssociation* CopyAnimation(const char* name);

    //! @addr 0x4CE130
    CAnimBlendAssociation* CopyAnimation(AnimationId id);

    /*!
     * @addr 0x4CE220
     *
     * @brief Create default associations
     *
     * @param blockName Name of the anim block to load the anims from
    */
    void CreateAssociations(const char* blockName);

    /*!
     * @brief Create associations for the specified animations
     *
     * @param blockName Name of the anim block to load the anims from
     * @param clump Fuck knows
     * @param names Animation names to load
     * @param cnt Num of anims to load (same as #names)
    */
    void CreateAssociations(const char* blockName, RpClump* clump, const char** names, uint32_t cnt);

    /*!
     * @addr 0x4CE3B0
     *
     * @brief Create animation associations with the given models
     *
     * @param blockName Name of the anim block to load the anims from
     * @param animNames Animation name array
     * @param modelNames Model (Clump models, I guess animation clumps?) name array. This and `animNames` form a kv map (So, the `n`th animation is associated with the `n`th model)
     * @param strBufLen Size of each field in the above name arrays.
    */
    void CreateAssociations(const char* blockName, const char* animNames, const char* modelNames, uint32_t strBufLen);

    //! @addr 0x4CDFF0
    void DestroyAssociations();

    //! @addr 0x4CE040
    CAnimBlendStaticAssociation* GetAnimation(const char* name);

    //! @addr 0x4CE090
    CAnimBlendStaticAssociation* GetAnimation(AnimationId id);

    //! 0x4CE1B0
    AnimationId GetAnimationId(const char* name);

    //! @addr 0x4CDFB0
    //! @unused
    void InitEmptyAssociations(RpClump* clump);

    //! @addr 0x4D37A0
    //! @unused
    auto IsCreated() const { return m_NumAnims != 0; }

    //! @addr 0x45B050
    //! @unused
    auto GetNumAnimations() const { return m_NumAnims; }

    //! @addr 0x45B060
    //! @unused
    auto GetAnimBlock() const { return m_AnimBlock; }

private:
    CAnimBlock* AllocateForBlock(const char* blockName, int32_t numAnims);

    auto GetAssociations() const { return notsa::span<CAnimBlendStaticAssociation>{ m_Anims, m_NumAnims }; }

    CAnimBlendAssociation* CopyAnimation(CAnimBlendStaticAssociation* def);

private:
    //! @notsa
    //! @brief Extract common code from `CreateAssociations`
    void CreateAssociation(CAnimBlendStaticAssociation* assoc, CAnimBlendHierarchy* anim, CBaseModelInfo* mi, size_t i);

public:
    CAnimBlock*                  m_AnimBlock{};              ///< The anim block of this group
    CAnimBlendStaticAssociation* m_Anims{};
    uint32_t                     m_NumAnims{};               ///< Number of anims (in the `m_Anims` array)
    uint32_t                     m_IdOffset{};               ///< This group's first anim ID
    notsa::WEnumS16<AssocGroupId> m_GroupID{ANIM_GROUP_NONE}; ///< This group's ID
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendAssocGroup) == 0x14, "CAnimBlendAssocGroup layout drift");
#endif
