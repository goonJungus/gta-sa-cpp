#include "Shopping.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "Stats.h"

namespace {
std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// A data line without its comment, split on blanks and commas.
std::vector<std::string> Tokens(std::string line) {
    if (const size_t h = line.find('#'); h != std::string::npos) line.erase(h);
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ' ' || c == '\t' || c == ',' || c == '\r' || c == '\n') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// The stat a row changes (CShopping::LoadStats): fat, respect, sexy, health, stamina, calories, or "-" for none.
int StatIndex(const std::string& s) {
    const std::string l = Lower(s);
    if (l == "fat") return 0;
    if (l == "respect") return 1;
    if (l == "sexy") return 2;
    if (l == "health") return 3;
    if (l == "stamina") return 4;
    if (l == "calories") return 5;
    return -1;
}
// CShopping::IncrementStat: the stat each index moves (fat, clothes respect, appearance, health food, stamina, calories).
constexpr int kStatId[6] = {Stats::STAT_FAT, Stats::STAT_CLOTHES_RESPECT, Stats::STAT_APPEARANCE, 335, Stats::STAT_STAMINA, Stats::STAT_CALORIES};

// CWeaponInfo::FindWeaponType for the names shopping.dat uses; armour is the pseudo weapon 0x30.
int WeaponType(const std::string& n) {
    static const std::map<std::string, int> w = {
        {"pistol", 22}, {"pistol_silenced", 23}, {"desert_eagle", 24}, {"grenade", 16}, {"shotgun", 25}, {"sawnoff", 26},
        {"spas12", 27}, {"micro_uzi", 28}, {"mp5", 29}, {"ak47", 30}, {"m4", 31}, {"tec9", 32}, {"countryrifle", 33},
        {"sniperrifle", 34}, {"rlauncher", 35}, {"rlauncher_hs", 36}, {"fthrower", 37}, {"minigun", 38}, {"satchel_charge", 39},
        {"armour", 0x30}};
    auto it = w.find(Lower(n));
    return it == w.end() ? -1 : it->second;
}
}  // namespace

bool Shopping::Init(const std::string& gameDir) {
    m_dir = gameDir;
    m_items.clear(); m_sections.clear(); m_shops.clear(); m_componentIds.clear(); m_modifiers.clear();
    m_rows.clear(); m_contents.clear(); m_section.clear(); m_shopLoaded.clear();
    m_clothesStored = false;
    // veh_mods.ide: the model id of every car component (a car mod's key is its model id)
    {
        std::ifstream f(gameDir + "/data/maps/veh_mods/veh_mods.ide");
        std::string line;
        while (std::getline(f, line)) {
            const auto t = Tokens(line);
            if (t.size() >= 2 && std::isdigit((unsigned char)t[0][0])) m_componentIds[Lower(t[1])] = std::atoi(t[0].c_str());
        }
    }
    {   // carmods.dat "mods": a row per car, the components it can have
        std::ifstream cf(gameDir + "/data/carmods.dat");
        std::string cl;
        bool inMods = false;
        while (std::getline(cf, cl)) {
            const auto t = Tokens(cl);
            if (t.empty()) continue;
            if (!inMods) { inMods = Lower(t[0]) == "mods"; continue; }
            if (Lower(t[0]) == "end") break;
            for (size_t k = 1; k < t.size(); k++) m_carMods[Lower(t[0])].insert(Lower(t[k]));
        }
    }
    std::ifstream f(gameDir + "/data/shopping.dat");
    if (!f) return false;
    std::vector<std::string> stack;   // the open sections
    std::string line;
    while (std::getline(f, line)) {
        const auto t = Tokens(line);
        if (t.empty()) continue;
        const std::string head = Lower(t[0]);
        if (head == "section" && t.size() >= 2) { stack.push_back(Lower(t[1])); continue; }
        if (head == "end") { if (!stack.empty()) stack.pop_back(); continue; }
        if (stack.size() == 2 && stack[0] == "prices") {
            const std::string& sec = stack[1];
            // name, tag, [model, type | type, type | ammo], stat, change, stat, change, price
            size_t extra = (sec == "clothes" || sec == "haircuts" || sec == "tattoos") ? 2 : sec == "weapons" ? 1 : 0;
            if (t.size() < 2 + extra + 5) continue;
            Row r;
            r.key = KeyOf(t[0], sec);
            r.tag = t[1].substr(0, 8);
            if (sec == "clothes" || sec == "haircuts") { r.a = Clothes::Key(t[2].c_str()); r.b = (uint32_t)std::atol(t[3].c_str()); }
            else if (sec == "tattoos") { r.a = t[2][0] == '-' ? 0xffffffffu : (uint32_t)std::atol(t[2].c_str()); r.b = Clothes::Key(t[3].c_str()); }
            else if (sec == "weapons") r.a = (uint32_t)std::atol(t[2].c_str());
            const size_t s = 2 + extra;
            Item it;
            it.key = r.key; it.section = sec;
            it.stat[0] = StatIndex(t[s]); it.change[0] = (signed char)std::atol(t[s + 1].c_str());
            it.stat[1] = StatIndex(t[s + 2]); it.change[1] = (signed char)std::atol(t[s + 3].c_str());
            r.basePrice = r.price = (int)std::atol(t[s + 4].c_str());
            m_sections[sec].push_back(r);
            m_items.push_back(it);
        } else if (stack.size() == 2 && stack[0] == "shops") {
            ShopDef& d = m_shops[stack[1]];
            if (head == "type" && t.size() >= 2) d.type = t[1];
            else if (head == "item" && t.size() >= 2) d.items.push_back(t[1]);
        }
    }
    if (hooks.log) hooks.log(("SHOPPING: " + std::to_string(m_items.size()) + " items, " + std::to_string(m_shops.size()) + " shops").c_str());
    return !m_items.empty();
}

// CShopping::GetKey: a car mod is its component's model id, a weapon its type, everything else the upper-cased name's hash.
uint32_t Shopping::KeyOf(const std::string& name, const std::string& section) const {
    if (section == "carmods") {
        auto it = m_componentIds.find(Lower(name));
        return it == m_componentIds.end() ? 0xffffffffu : (uint32_t)it->second;
    }
    if (section == "weapons") return (uint32_t)WeaponType(name);
    return Clothes::Key(name.c_str());
}

// The player's car can take a component when carmods.dat lists it for that car; the wheels, the stereo and the
// hydraulics are every car's. On foot (no car name) everything is listed.
bool Shopping::ModFits(uint32_t key) const {
    const std::string car = hooks.playerCarName ? Lower(hooks.playerCarName()) : std::string();
    if (car.empty()) return true;
    std::string comp;
    for (const auto& c : m_componentIds) if ((uint32_t)c.second == key) { comp = c.first; break; }
    if (comp.empty()) return false;
    if (comp.compare(0, 6, "wheel_") == 0 || comp == "stereo" || comp == "hydralics") return true;
    auto it = m_carMods.find(car);
    return it != m_carMods.end() && it->second.count(comp) != 0;
}

int Shopping::FindItem(uint32_t key) const {
    for (size_t i = 0; i < m_items.size(); i++) if (m_items[i].key == key) return (int)i;
    return -1;
}

int Shopping::FindRow(uint32_t key) const {
    for (size_t i = 0; i < m_rows.size(); i++) if (m_rows[i].key == key) return (int)i;
    return -1;
}

void Shopping::LoadPrices(const std::string& section) {
    const std::string s = Lower(section);
    if (s == m_section) return;
    m_contents.clear();
    m_section = s;
    m_rows.clear();
    if (auto it = m_sections.find(s); it != m_sections.end()) m_rows = it->second;
    for (Row& r : m_rows) {
        r.price = r.basePrice;
        for (const auto& m : m_modifiers) if (m.first == r.key) r.price = m.second;
    }
}

void Shopping::LoadShop(const std::string& shop) {
    if (Lower(shop) == Lower(m_shopLoaded)) return;
    m_shopLoaded = shop;
    m_contents.clear();
    if (Lower(shop) == "bought") {   // everything the player has bought from the loaded price section
        for (const Row& r : m_rows) if (const int i = FindItem(r.key); i >= 0 && m_items[(size_t)i].bought) m_contents.push_back(r.key);
        return;
    }
    auto it = m_shops.find(Lower(shop));
    if (it == m_shops.end()) return;
    LoadPrices(it->second.type);
    m_contents.clear();
    for (const std::string& name : it->second.items) {
        const uint32_t key = KeyOf(name, m_section);
        if (m_section == "carmods" && !ModFits(key)) continue;
        m_contents.push_back(key);
    }
}

int Shopping::GetPrice(uint32_t key) const {
    const int r = FindRow(key);
    return r >= 0 ? m_rows[(size_t)r].price : 0;
}

uint32_t Shopping::GetExtraInfo(uint32_t key, int which) const {
    const int r = FindRow(key);
    if (r < 0) return 0;
    return which == 0 ? m_rows[(size_t)r].a : which == 1 ? m_rows[(size_t)r].b : 0;
}

std::string Shopping::NameTag(uint32_t key) const {
    const int r = FindRow(key);
    return r >= 0 ? m_rows[(size_t)r].tag : std::string();
}

void Shopping::IncrementStat(int which, int change) {   // CShopping::IncrementStat: respect and appearance move ten times as far
    if (which < 0 || which > 5) return;
    if (which == 1 || which == 2) change *= 10;
    Stats::Modify(kStatId[which], (float)change);
}

void Shopping::UpdateStats(int item, bool apply) {   // CShopping::UpdateStats: false takes an item's effect back off
    const Item& it = m_items[(size_t)item];
    for (int k = 0; k < 2; k++) IncrementStat(it.stat[k], apply ? it.change[k] : -it.change[k]);
}

void Shopping::ApplyOutfit(const PedClothesDesc& d) {
    if (m_clothesStored) m_stored = d;   // trying clothes on: the bought item goes into the stored outfit
    else if (hooks.setOutfit) hooks.setOutfit(d);
}

void Shopping::Buy(uint32_t key) {
    const int ri = FindRow(key);
    const int ii = ri >= 0 ? FindItem(m_rows[(size_t)ri].key) : -1;
    if (ri < 0 || ii < 0) return;
    const Row row = m_rows[(size_t)ri];
    PedClothesDesc d = m_clothesStored ? m_stored : (hooks.outfit ? hooks.outfit() : PedClothesDesc());
    const int cost = row.price;
    if (hooks.addMoney) hooks.addMoney(-cost);
    UpdateStats(ii, true);
    Stats::Modify(Stats::STAT_TOTAL_SHOPPING_BUDGET, (float)cost);
    m_items[(size_t)ii].bought = true;
    if (hooks.log) hooks.log(("SHOPPING: bought " + row.tag + " for " + std::to_string(cost) + " (" + m_section + ")").c_str());
    const auto replace = [&](bool tryOn) {   // the item that was on this part gives its stats back
        const ClothesTexturePart part = (ClothesTexturePart)row.b;
        if (part >= CLOTHES_TEXTURE_TOTAL) return;
        if (const uint32_t cur = d.textureKeys[part]; cur != 0)
            if (const int old = FindItem(cur); old >= 0) UpdateStats(old, false);
        d.SetTextureAndModel(key, row.a, part);
        (void)tryOn;
    };
    if (m_section == "carmods" || m_section == "carpaintjobs") {
        Stats::Modify(Stats::STAT_CAR_MODIFICATION_BUDGET, (float)cost);
    } else if (m_section == "clothes") {
        Stats::Modify(Stats::STAT_FASHION_BUDGET, (float)cost);
        replace(false);
        ApplyOutfit(d);
        switch (Clothes::GetTextureDependency((ClothesTexturePart)row.b)) {   // a new look: the police lose the player
        case CLOTHES_MODEL_TORSO: case CLOTHES_MODEL_LEGS: case CLOTHES_MODEL_HEAD: case CLOTHES_MODEL_GLASSES: case CLOTHES_MODEL_HATS:
            if (hooks.clearWantedAndParole) hooks.clearWantedAndParole();
            break;
        default: break;
        }
    } else if (m_section == "haircuts") {
        Stats::Modify(Stats::STAT_HAIRDRESSING_BUDGET, (float)cost);
        replace(false);
        ApplyOutfit(d);
        if (hooks.clearWantedAndParole) hooks.clearWantedAndParole();
    } else if (m_section == "tattoos") {
        Stats::Modify(Stats::STAT_TATTOO_BUDGET, (float)cost);
        if (row.a < CLOTHES_TEXTURE_TOTAL) d.SetTextureAndModel(key, 0, (ClothesTexturePart)row.a);
        ApplyOutfit(d);
    } else if (m_section == "gifts") {
        if (hooks.giveWeapon) hooks.giveWeapon(14, 0);   // flowers
    } else if (m_section == "food") {
        Stats::Modify(Stats::STAT_NUMBER_OF_MEALS_EATEN, 1.0f);
        Stats::Modify(Stats::STAT_FOOD_BUDGET, (float)cost);
    } else if (m_section == "weapons") {
        Stats::Modify(Stats::STAT_WEAPON_BUDGET, (float)cost);
        if (key == 0x30) { if (hooks.fillArmour) hooks.fillArmour(); }
        else if (hooks.giveWeapon) hooks.giveWeapon((int)key, (int)row.a);
    }
}

bool Shopping::HasPlayerBought(uint32_t key) const {
    const int i = FindItem(key);
    return i >= 0 && m_items[(size_t)i].bought;
}

void Shopping::SetPlayerHasBought(uint32_t key) {
    if (const int i = FindItem(key); i >= 0) m_items[(size_t)i].bought = true;
}

void Shopping::RemoveLoadedShop() {
    m_shopLoaded.clear();
    m_section.clear();
}

void Shopping::ShutdownForRestart() {
    for (Item& it : m_items) it.bought = false;
    m_shopLoaded.clear();
    m_section.clear();
    m_modifiers.clear();
    m_clothesStored = false;
}

void Shopping::StoreClothesState() {
    if (hooks.outfit) m_stored = hooks.outfit();
    m_clothesStored = true;
}

void Shopping::RestoreClothesState() {
    if (hooks.setOutfit) hooks.setOutfit(m_stored);
    m_clothesStored = false;
}

void Shopping::AddPriceModifier(uint32_t key, int price) {   // CShopping::unk_0049bdd0
    auto it = std::find_if(m_modifiers.begin(), m_modifiers.end(), [&](const auto& m) { return m.first == key; });
    if (it != m_modifiers.end()) it->second = price; else m_modifiers.push_back({key, price});
    if (const int r = FindRow(key); r >= 0) m_rows[(size_t)r].price = price;
}

void Shopping::RemovePriceModifier(uint32_t key) {
    m_modifiers.erase(std::remove_if(m_modifiers.begin(), m_modifiers.end(), [&](const auto& m) { return m.first == key; }), m_modifiers.end());
    if (const int r = FindRow(key); r >= 0) m_rows[(size_t)r].price = m_rows[(size_t)r].basePrice;
}
