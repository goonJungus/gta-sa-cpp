#include "Clothes.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#include "Cheat.h"
#include "ImgLoader.h"

namespace {
enum RuleTag { kCuts, kSetc, kTex, kHide, kEndIgnore, kIgnore, kEndExclusive, kExclusive };
struct Rule { RuleTag tag; uint32_t subject = 0; int slot = -1; uint32_t model = 0, tex = 0; };

std::vector<Rule> g_rules;
std::unordered_map<uint32_t, std::string> g_names;
std::unordered_map<uint32_t, int> g_flags;   // 1 dff, 2 txd
std::vector<std::string> g_models, g_textures;
const std::string kNone;

int ModelFromName(const char* n) {   // GetClothesModelFromName
    static const char* names[] = {"torso", "head", "hands", "legs", "feet", "necklace", "watch", "glasses", "hat", "extra1"};
    for (int i = 0; i < 10; i++)
        if (!std::strcmp(n, names[i])) return i;
    return CLOTHES_MODEL_TORSO;
}
}  // namespace

uint32_t Clothes::Key(const char* name) { return name ? Cheat::Key(name, (int)std::strlen(name)) : 0; }

void PedClothesDesc::SetModel(const char* name, ClothesModelPart part) { modelKeys[part] = Clothes::Key(name); }

void PedClothesDesc::SetTextureAndModel(uint32_t texture, uint32_t model, ClothesTexturePart part) {
    if (part >= CLOTHES_TEXTURE_TOTAL) return;
    textureKeys[part] = texture;
    const ClothesModelPart dep = Clothes::GetTextureDependency(part);
    if (dep != CLOTHES_MODEL_UNAVAILABLE) modelKeys[dep] = model;
}

void PedClothesDesc::SetTextureAndModel(const char* texture, const char* model, ClothesTexturePart part) {
    SetTextureAndModel(Clothes::Key(texture), Clothes::Key(model), part);
}

bool PedClothesDesc::IsWearingModel(const char* name, int part) const {
    const uint32_t k = Clothes::Key(name);
    if (part >= 0) return modelKeys[part] == k;
    for (uint32_t m : modelKeys)
        if (m == k) return true;
    return false;
}

bool PedClothesDesc::GetIsWearingBalaclava() const { return IsWearingModel("balaclava", CLOTHES_MODEL_SPECIAL); }

bool PedClothesDesc::HasVisibleNewHairCut(int type) const {
    if (modelKeys[CLOTHES_MODEL_SPECIAL] == Clothes::Key("balaclava")) return false;
    if (modelKeys[CLOTHES_MODEL_HATS] != 0) return false;
    if (modelKeys[CLOTHES_MODEL_HEAD] == Clothes::Key("head")) return false;
    return type != 1 || modelKeys[CLOTHES_MODEL_HEAD] == Clothes::Key("afro");
}

bool PedClothesDesc::HasVisibleTattoo() const {
    for (int i = CLOTHES_TEXTURE_LOWER_LEFT_ARM; i <= CLOTHES_TEXTURE_UPPER_BACK; i++)
        if (textureKeys[i]) return true;
    return false;
}

bool PedClothesDesc::operator==(const PedClothesDesc& o) const {
    return !std::memcmp(modelKeys, o.modelKeys, sizeof modelKeys) &&
           !std::memcmp(textureKeys, o.textureKeys, sizeof textureKeys) && fatStat == o.fatStat &&
           muscleStat == o.muscleStat;
}

ClothesModelPart Clothes::GetTextureDependency(ClothesTexturePart p) {
    switch (p) {
    case CLOTHES_TEXTURE_TORSO: return CLOTHES_MODEL_TORSO;
    case CLOTHES_TEXTURE_HEAD: return CLOTHES_MODEL_HEAD;
    case CLOTHES_TEXTURE_LEGS: return CLOTHES_MODEL_LEGS;
    case CLOTHES_TEXTURE_SHOES: return CLOTHES_MODEL_SHOES;
    case CLOTHES_TEXTURE_NECKLACE: return CLOTHES_MODEL_NECKLACE;
    case CLOTHES_TEXTURE_BRACELET: return CLOTHES_MODEL_BRACELET;
    case CLOTHES_TEXTURE_GLASSES: return CLOTHES_MODEL_GLASSES;
    case CLOTHES_TEXTURE_HATS: return CLOTHES_MODEL_HATS;
    case CLOTHES_TEXTURE_SPECIAL: return CLOTHES_MODEL_SPECIAL;
    default: return CLOTHES_MODEL_UNAVAILABLE;
    }
}

ClothesTexturePart Clothes::GetDependentTexture(ClothesModelPart p) {
    switch (p) {
    case CLOTHES_MODEL_TORSO: return CLOTHES_TEXTURE_TORSO;
    case CLOTHES_MODEL_HEAD: return CLOTHES_TEXTURE_HEAD;
    case CLOTHES_MODEL_LEGS: return CLOTHES_TEXTURE_LEGS;
    case CLOTHES_MODEL_SHOES: return CLOTHES_TEXTURE_SHOES;
    case CLOTHES_MODEL_NECKLACE: return CLOTHES_TEXTURE_NECKLACE;
    case CLOTHES_MODEL_BRACELET: return CLOTHES_TEXTURE_BRACELET;
    case CLOTHES_MODEL_GLASSES: return CLOTHES_TEXTURE_GLASSES;
    case CLOTHES_MODEL_HATS: return CLOTHES_TEXTURE_HATS;
    case CLOTHES_MODEL_SPECIAL: return CLOTHES_TEXTURE_SPECIAL;
    default: return CLOTHES_TEXTURE_UNAVAILABLE;
    }
}

int Clothes::MotionGroup(float fat, float muscle) {
    if (fat > 500.0f && fat > muscle) return 1;
    return muscle <= 500.0f ? 0 : 2;
}

const std::string& Clothes::Name(uint32_t key) {
    auto it = g_names.find(key);
    return it == g_names.end() ? kNone : it->second;
}
bool Clothes::HasModel(uint32_t key) { auto it = g_flags.find(key); return it != g_flags.end() && (it->second & 1); }
bool Clothes::HasTexture(uint32_t key) { auto it = g_flags.find(key); return it != g_flags.end() && (it->second & 2); }
const std::vector<std::string>& Clothes::Models() { return g_models; }
const std::vector<std::string>& Clothes::Textures() { return g_textures; }

// CClothes::LoadClothesFile.
bool Clothes::Load(const std::string& gameDir) {
    g_rules.clear();
    g_names.clear();
    g_flags.clear();
    g_models.clear();
    g_textures.clear();
    ImgLoader img;
    if (img.Open(gameDir + "\\models\\player.img")) {
        for (size_t i = 0; i < img.EntryCount(); i++) {
            const std::string& n = img.GetEntryName(i);
            const size_t dot = n.rfind('.');
            if (dot == std::string::npos) continue;
            const std::string ext = n.substr(dot + 1), stem = n.substr(0, dot);
            const uint32_t k = Key(stem.c_str());
            if (ext == "dff") { g_flags[k] |= 1; g_models.push_back(stem); }
            else if (ext == "txd") { g_flags[k] |= 2; g_textures.push_back(stem); }
            else continue;
            g_names[k] = stem;
        }
    }
    std::ifstream f(gameDir + "\\data\\clothes.dat");
    if (!f) return false;
    std::string line;
    bool inRule = false;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (!inRule) { inRule = !line.compare(0, 4, "rule"); continue; }
        if (!line.compare(0, 3, "end")) { inRule = false; continue; }
        std::vector<std::string> tok;
        for (size_t p = 0; p < line.size();) {
            const size_t s = line.find_first_not_of(" \t,", p);
            if (s == std::string::npos) break;
            const size_t e = line.find_first_of(" \t,", s);
            tok.push_back(line.substr(s, e == std::string::npos ? e : e - s));
            p = e == std::string::npos ? line.size() : e;
        }
        if (tok.empty()) continue;
        static const struct { const char* n; RuleTag t; } tags[] = {{"CUTS", kCuts}, {"SETC", kSetc}, {"TEX", kTex}, {"HIDE", kHide},
            {"ENDIGNORE", kEndIgnore}, {"IGNORE", kIgnore}, {"ENDEXCLUSIVE", kEndExclusive}, {"EXCLUSIVE", kExclusive}};
        bool known = false;
        Rule r{};
        for (auto& t : tags)
            if (tok[0] == t.n) { r.tag = t.t; known = true; break; }
        if (!known || tok.size() < 2) { std::fprintf(stderr, "[CLOTHES] bad rule '%s'\n", line.c_str()); continue; }
        r.subject = Key(tok[1].c_str());
        auto arg = [&](size_t i) { return i < tok.size() ? tok[i].c_str() : "-"; };
        switch (r.tag) {
        case kCuts: r.model = Key(arg(2)); break;
        case kTex: r.tex = Key(arg(2)); break;
        case kSetc:
            r.slot = ModelFromName(arg(2));
            r.model = !std::strcmp(arg(3), "-") ? 0 : Key(arg(3));
            r.tex = !std::strcmp(arg(4), "-") ? 0 : Key(arg(4));
            break;
        case kHide: r.slot = ModelFromName(arg(2)); break;
        default: break;
        }
        g_rules.push_back(r);
    }
    std::fprintf(stderr, "[CLOTHES] %zu rules, %zu models, %zu textures\n", g_rules.size(), g_models.size(), g_textures.size());
    return true;
}

// CClothesBuilder::PreprocessClothesDesc @ 005a44c0.
void Clothes::Preprocess(PedClothesDesc& d, bool cutscene) {
    uint32_t scope[8];
    bool ignoring[8] = {};   // true: IGNORE (skip while the slot wears the key); false: EXCLUSIVE (skip unless it does)
    for (uint32_t& s : scope) s = 0xFFFFFFFFu;
    for (const Rule& r : g_rules) {
        auto setScope = [&](bool ign) {
            for (int i = 0; i < 8; i++)
                if (scope[i] == 0xFFFFFFFFu) { scope[i] = r.subject; ignoring[i] = ign; return; }
        };
        auto endScope = [&](bool ign) {
            for (int i = 0; i < 8; i++)
                if (scope[i] == r.subject) { scope[i] = 0xFFFFFFFFu; ignoring[i] = ign; return; }
        };
        switch (r.tag) {
        case kIgnore: setScope(true); continue;
        case kExclusive: setScope(false); continue;
        case kEndIgnore: endScope(true); continue;
        case kEndExclusive: endScope(false); continue;
        default: break;
        }
        for (int part = 0; part < CLOTHES_MODEL_TOTAL; part++) {
            const int subjectPart = r.slot >= 0 ? r.slot : part;
            int i = 0;
            for (; i < 8; i++)
                if (scope[i] != 0xFFFFFFFFu && (d.modelKeys[subjectPart] == scope[i]) == ignoring[i]) break;
            if (i < 8) continue;
            if (d.modelKeys[part] != r.subject) continue;
            switch (r.tag) {
            case kCuts:
                if (cutscene) d.SetModel(r.model, (ClothesModelPart)part);
                break;
            case kSetc: {
                const ClothesModelPart slot = (ClothesModelPart)r.slot;
                if (slot == CLOTHES_MODEL_HANDS) { d.SetModel(r.model, CLOTHES_MODEL_HANDS); break; }
                uint32_t tex = r.tex, model = r.model;
                const ClothesTexturePart tp = GetDependentTexture(slot);
                if (tex == 0 && tp != CLOTHES_TEXTURE_UNAVAILABLE) tex = d.textureKeys[tp];
                if (model == 0) model = d.modelKeys[slot];
                d.SetTextureAndModel(tex, model, tp);
                break;
            }
            case kTex:
                d.SetTextureAndModel(r.tex, r.subject, GetDependentTexture((ClothesModelPart)part));
                break;
            case kHide: d.SetModel(0u, (ClothesModelPart)r.slot); break;
            default: break;
            }
        }
    }
}
