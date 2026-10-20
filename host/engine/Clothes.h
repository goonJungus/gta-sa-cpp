#pragma once
// CClothes / CPedClothesDesc / CClothesBuilder::PreprocessClothesDesc: what CJ wears and the
// data\clothes.dat rules that turn a chosen outfit into the models and textures actually built.
// Items are named by the upper-cased CRC32 of their file stem in models\player.img.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum ClothesModelPart {
    CLOTHES_MODEL_TORSO = 0, CLOTHES_MODEL_HEAD, CLOTHES_MODEL_HANDS, CLOTHES_MODEL_LEGS, CLOTHES_MODEL_SHOES,
    CLOTHES_MODEL_NECKLACE, CLOTHES_MODEL_BRACELET, CLOTHES_MODEL_GLASSES, CLOTHES_MODEL_HATS,
    CLOTHES_MODEL_SPECIAL, CLOTHES_MODEL_TOTAL, CLOTHES_MODEL_UNAVAILABLE = CLOTHES_MODEL_TOTAL
};

enum ClothesTexturePart {
    CLOTHES_TEXTURE_TORSO = 0, CLOTHES_TEXTURE_HEAD, CLOTHES_TEXTURE_LEGS, CLOTHES_TEXTURE_SHOES,
    CLOTHES_TEXTURE_LOWER_LEFT_ARM, CLOTHES_TEXTURE_UPPER_LEFT_ARM, CLOTHES_TEXTURE_UPPER_RIGHT_ARM,
    CLOTHES_TEXTURE_LOWER_RIGHT_ARM, CLOTHES_TEXTURE_BACK_TOP, CLOTHES_TEXTURE_LEFT_CHEST,
    CLOTHES_TEXTURE_RIGHT_CHEST, CLOTHES_TEXTURE_STOMACH, CLOTHES_TEXTURE_UPPER_BACK,
    CLOTHES_TEXTURE_NECKLACE, CLOTHES_TEXTURE_BRACELET, CLOTHES_TEXTURE_GLASSES, CLOTHES_TEXTURE_HATS,
    CLOTHES_TEXTURE_SPECIAL, CLOTHES_TEXTURE_TOTAL, CLOTHES_TEXTURE_UNAVAILABLE = CLOTHES_TEXTURE_TOTAL
};

// CPedClothesDesc. A key of 0 is an empty slot.
struct PedClothesDesc {
    uint32_t modelKeys[CLOTHES_MODEL_TOTAL] = {};
    uint32_t textureKeys[CLOTHES_TEXTURE_TOTAL] = {};
    float fatStat = 0.0f, muscleStat = 0.0f;

    void Initialise() { *this = PedClothesDesc(); }
    void SetModel(uint32_t key, ClothesModelPart part) { modelKeys[part] = key; }
    void SetModel(const char* name, ClothesModelPart part);
    void SetTextureAndModel(uint32_t texture, uint32_t model, ClothesTexturePart part);
    void SetTextureAndModel(const char* texture, const char* model, ClothesTexturePart part);
    bool IsWearingModel(const char* name, int part = -1) const;
    bool GetIsWearingBalaclava() const;
    bool HasVisibleNewHairCut(int type) const;
    bool HasVisibleTattoo() const;
    bool operator==(const PedClothesDesc& o) const;
};

namespace Clothes {
// CClothes::Init: read data\clothes.dat and index models\player.img.
bool Load(const std::string& gameDir);
uint32_t Key(const char* name);   // CKeyGen::GetUppercaseKey
// File stem of an item key found in player.img, "" when unknown.
const std::string& Name(uint32_t key);
bool HasModel(uint32_t key);      // <stem>.dff exists
bool HasTexture(uint32_t key);    // <stem>.txd exists
// Every model / texture dictionary stem in player.img (the wardrobe catalogue).
const std::vector<std::string>& Models();
const std::vector<std::string>& Textures();

ClothesModelPart GetTextureDependency(ClothesTexturePart part);
ClothesTexturePart GetDependentTexture(ClothesModelPart part);
// CClothesBuilder::PreprocessClothesDesc: apply CUTS (cutscene only), SETC, TEX and HIDE with the
// IGNORE / EXCLUSIVE scopes.
void Preprocess(PedClothesDesc& desc, bool cutscene);
// CClothes::GetPlayerMotionGroupToLoad: 1 fat, 2 muscular, 0 the plain player group.
int MotionGroup(float fat, float muscle);
}  // namespace Clothes
