// CAnimManager - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimManager.h
//
// Static manager for animation blocks, assoc groups and the animation LRU cache.
//
// Adaptations:
// - Stripped InjectHooks() (plugin-sdk hooking, not needed for clean-room).
// - StaticRef<T>(addr) statics -> plain static data members, defined in
//   src/CAnimManager.cpp (original GTA SA 1.0 addresses kept as comments).
// - notsa::ci_string_view (plugin-sdk extensions/ci_string.hpp) ->
//   std::string_view (C++17). NOTE: ci_string_view is case-insensitive; the
//   case-insensitive compare must be reimplemented from the decomp.
// - rng::views::take (plugin-sdk range utility) -> notsa::span (see AnimTypes.h).
// - IsClumpSkinned() demoted to a declaration: needs the RenderWare layer
//   (GetFirstAtomic/RpSkinGeometryGetSkin/RpAtomicGetGeometry).
// - GetOrCreateAnimBlock kept as `static auto` (deduced return in the original);
//   give it an explicit return type when porting from the decomp.

#pragma once

#include "AnimTypes.h"
#include "CAnimBlendAssocGroup.h"
#include "CAnimBlendAssociation.h"
#include "CAnimBlendHierarchy.h"

#include <array>
#include <cstdint>
#include <string_view>

constexpr auto MAX_ANIM_BLOCK_NAME = 16;
constexpr auto NUM_ANIM_ASSOC_GROUPS = 118;
constexpr auto NUM_ANIM_BLOCKS = 180;

class CAnimManager {
private:
    static std::array<AnimAssocDefinition, NUM_ANIM_ASSOC_GROUPS> ms_aAnimAssocDefinitionsX; // replacement (NOTSA)
    static AnimAssocDefinition ms_aAnimAssocDefinitions[NUM_ANIM_ASSOC_GROUPS]; // StaticRef 0x8AA5A8
    static uint32_t ms_numAnimAssocDefinitions;                                // StaticRef 0xB4EA28

    static CAnimBlendAssocGroup* ms_aAnimAssocGroups; // StaticRef 0xB4EA34

    static std::array<CAnimBlendHierarchy, 2500> ms_aAnimations; // StaticRef 0xB4EA40
    static int32_t ms_numAnimations;                            // StaticRef 0xB4EA2C

    static std::array<CAnimBlock, NUM_ANIM_BLOCKS> ms_aAnimBlocks; // StaticRef 0xB5D4A0
    static uint32_t ms_numAnimBlocks;                             // StaticRef 0xB4EA30

    static CLinkList<CAnimBlendHierarchy*> ms_AnimCache; // StaticRef 0xB5EB20

public:
    static void Initialise();
    static void Shutdown();

    static void LoadAnimFiles();
    static void LoadAnimFile(RwStream* stream, bool loadCompressed, const char (*uncompressedAnimations)[32] = nullptr);
    static void ReadAnimAssociationDefinitions();

    static CAnimBlock* GetAnimationBlock(AssocGroupId animGroup);

    // TODO(anim): minimal accessor for CPedIntelligence::GetUsingParachute (2026-10-09).
    // ms_aAnimBlocks stays private; the anim batch can widen access if needed.
    static const CAnimBlock& GetAnimBlockById(int32_t blockId) { return ms_aAnimBlocks[blockId]; }
    static CAnimBlock* GetAnimationBlock(const char* name);

    static int32_t GetAnimationBlockIndex(AssocGroupId animGroup);
    static int32_t GetAnimationBlockIndex(CAnimBlock* animBlock);
    static int32_t GetAnimationBlockIndex(const char* name);

    static AssocGroupId GetFirstAssocGroup(const char* name);
    static CAnimBlendHierarchy* GetAnimation(uint32_t hash, const CAnimBlock* animBlock);
    static CAnimBlendHierarchy* GetAnimation(const char* animName, const CAnimBlock* animBlock);
    static CAnimBlendHierarchy& GetAnimation(AnimationId id);
    static const char* GetAnimGroupName(AssocGroupId groupId);
    static const char* GetAnimBlockName(AssocGroupId groupId);
    static AssocGroupId GetAnimationGroupIdByName(std::string_view name); // was notsa::ci_string_view
    static CAnimBlendStaticAssociation* GetAnimAssociation(AssocGroupId groupId, AnimationId animId);
    static CAnimBlendStaticAssociation* GetAnimAssociation(AssocGroupId groupId, const char* animName);
    static CAnimBlendAssociation* AddAnimationToClump(RpClump* clump, CAnimBlendAssociation* anim); // NOTSA - Internal
    static int32_t GetNumRefsToAnimBlock(int32_t index);

    static CAnimBlendAssociation* CreateAnimAssociation(AssocGroupId groupId, AnimationId animId);

    static CAnimBlendAssociation* AddAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId);
    static CAnimBlendAssociation* AddAnimation(RpClump* clump, CAnimBlendHierarchy* hier, int32_t clumpAssocFlag);
    static CAnimBlendAssociation* AddAnimationAndSync(RpClump* clump, CAnimBlendAssociation* syncWith, AssocGroupId groupId, AnimationId animId);
    static AnimAssocDefinition* AddAnimAssocDefinition(const char* groupName, const char* blockName, uint32_t modelIndex, uint32_t animsCount, AnimDescriptor* descriptor);
    static void AddAnimToAssocDefinition(AnimAssocDefinition* definition, const char* animName);
    static void AddAnimBlockRef(int32_t index);

    static void CreateAnimAssocGroups();
    static int32_t RegisterAnimBlock(const char* name);
    static void RemoveLastAnimFile();
    static void RemoveAnimBlock(int32_t index);
    static void RemoveAnimBlockRef(int32_t index);
    static void RemoveAnimBlockRefWithoutDelete(int32_t index);
    static void RemoveFromUncompressedCache(CAnimBlendHierarchy* hier);

    /*!
     * @addr 0x4D41C0
     *
     * @brief Uncompress animation data (Unless the animation has the keep-compressed flag).
     * @brief Also marks the anim as recently-used in the LRU cache
     *
     * @details This function does a 2 things:
     * @details - Uncompress anim data  (Unless the animation has the keep-compressed flag)
     * @details - Since it uses a LRU cache, the hierarchy is put at the front (thus marking it as recently used)
     * @details So even if the anim is already un-compressed this function should be called
     * @details to mark it as recently-used in the LRU cache.
     *
     * @param hier The hierarchy to uncompress
    */
    static void UncompressAnimation(CAnimBlendHierarchy* hier);
    static CAnimBlendAssociation* BlendAnimation(RpClump* clump, CAnimBlendHierarchy* animBlendHier, int32_t flags, float clumpAssocBlendData = 8.f);
    static CAnimBlendAssociation* BlendAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId, float clumpAssocBlendData = 8.f);

    static uint32_t GetAnimIndex(const CAnimBlendHierarchy* h);
    static bool IsAnimInBlock(const CAnimBlendHierarchy* h, const CAnimBlock* b);

    /// NOTSA. Get random gangtalk anim
    static AnimationId GetRandomGangTalkAnim();

    static notsa::span<CAnimBlock> GetAnimBlocks() { return notsa::span<CAnimBlock>{ ms_aAnimBlocks.data(), ms_numAnimBlocks }; }
    static notsa::span<CAnimBlendAssocGroup> GetAssocGroups() { return notsa::span<CAnimBlendAssocGroup>{ ms_aAnimAssocGroups, ms_numAnimAssocDefinitions }; }
    static notsa::span<AnimAssocDefinition> GetAssocGroupDefs() { return notsa::span<AnimAssocDefinition>{ ms_aAnimAssocDefinitions, ms_numAnimAssocDefinitions }; }

    static void StreamAnimBlock(const char* blck, bool shouldBeLoaded, bool& isLoaded);

private:
    static void LoadAnimFile_ANPK(RwStream* stream, const IFPSectionHeader& h, bool compress, const char (*uncompressedAnims)[32]);
    static void LoadAnimFile_ANP23(RwStream* stream, const IFPSectionHeader& h, bool compress, bool isANP3);
    static auto GetOrCreateAnimBlock(const char* name, uint32_t numAnims);
};

// 0x4C4DC0 - demoted to a declaration: needs the RenderWare layer
// (GetFirstAtomic/RpSkinGeometryGetSkin/RpAtomicGetGeometry). TODO: decomp.
bool IsClumpSkinned(RpClump* clump);
