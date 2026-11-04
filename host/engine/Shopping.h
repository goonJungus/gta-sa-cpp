#pragma once
// CShopping: data\shopping.dat. The price sections list every buyable item (car mods, clothes, haircuts,
// tattoos, food, weapons) with the stats it changes and its price; the shop sections name the items one
// shop sells. The shop scripts load a shop, ask for its items and prices, and buy. Buying takes the money,
// moves the item's two stats, counts the budgets and applies the item (clothes, tattoo, weapon...).
// The rules follow CShopping::LoadStats, LoadPrices, LoadShop and Buy.

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Clothes.h"

class Shopping {
public:
    // What the host does for a purchase.
    struct Hooks {
        std::function<int()> money;
        std::function<void(int)> addMoney;                          // negative to spend
        std::function<PedClothesDesc()> outfit;                     // what CJ wears
        std::function<void(const PedClothesDesc&)> setOutfit;       // CClothes::RebuildPlayer
        std::function<void(int weapon, int ammo)> giveWeapon;       // and make it the current weapon
        std::function<void()> fillArmour;
        std::function<void()> clearWantedAndParole;                 // CWanted::ClearWantedLevelAndGoOnParole
        std::function<void()> storeVehicleMods, restoreVehicleMods; // CShopping::StoreVehicleMods / RestoreVehicleMods
        std::function<std::string()> playerCarName;                 // the dff name of the car the player is in, "" when on foot
        std::function<void(const char*)> log;
    };
    Hooks hooks;

    // CShopping::Init + LoadStats: every item of every price section, in file order.
    bool Init(const std::string& gameDir);
    bool Ready() const { return !m_items.empty(); }

    void LoadPrices(const std::string& section);                    // 0x075D
    void LoadShop(const std::string& shop);                         // 0x075E ("bought": everything bought from the loaded section)
    int NumItems() const { return (int)m_contents.size(); }         // 0x075F
    uint32_t ItemInShop(int i) const { return i >= 0 && i < (int)m_contents.size() ? m_contents[(size_t)i] : 0; }   // 0x0760
    int GetPrice(uint32_t key) const;                               // 0x0761
    uint32_t GetExtraInfo(uint32_t key, int which) const;           // 0x0783: 0 model key / tattoo part / ammo, 1 type
    std::string NameTag(uint32_t key) const;                        // 0x078C: the GXT key of the item's name
    void Buy(uint32_t key);                                         // 0x0790
    bool HasPlayerBought(uint32_t key) const;                       // 0x0942
    void SetPlayerHasBought(uint32_t key);
    const std::string& ShopLoaded() const { return m_shopLoaded; }  // 0x07B0
    void RemoveLoadedShop();                                        // 0x087C
    void ShutdownForRestart();                                      // a new game: nothing bought
    void StoreClothesState();                                       // 0x0793: the try-on starts, Buy then edits the stored copy
    void RestoreClothesState();                                     // 0x0794
    void AddPriceModifier(uint32_t key, int price);                 // 0x08C8
    void RemovePriceModifier(uint32_t key);                         // 0x08C9
    void SetCurrentProperty(int property) { m_property = property; }   // 0x082F
    int CurrentProperty() const { return m_property; }
    bool ClothesStored() const { return m_clothesStored; }
    const PedClothesDesc& StoredClothes() const { return m_stored; }
    int NumBuyable() const { return (int)m_items.size(); }

private:
    struct Item {                       // ms_keys / ms_statModifiers / ms_bHasBought
        uint32_t key = 0;
        std::string section;
        int stat[2] = {-1, -1};         // 0 fat, 1 respect, 2 sexy, 3 health, 4 stamina, 5 calories, -1 none
        int change[2] = {0, 0};
        bool bought = false;
    };
    struct Row {                        // ms_prices
        uint32_t key = 0;
        std::string tag;
        uint32_t a = 0, b = 0;          // clothes / haircuts: model key, texture part. tattoos: part (or -1), type key. weapons: ammo
        int basePrice = 0, price = 0;
    };
    struct ShopDef { std::string type; std::vector<std::string> items; };

    uint32_t KeyOf(const std::string& name, const std::string& section) const;
    int FindItem(uint32_t key) const;
    int FindRow(uint32_t key) const;
    void UpdateStats(int item, bool apply);
    void IncrementStat(int which, int change);
    void ApplyOutfit(const PedClothesDesc& d);

    std::string m_dir;
    std::vector<Item> m_items;
    std::map<std::string, std::vector<Row>> m_sections;   // lower-cased section name -> its rows
    std::map<std::string, ShopDef> m_shops;               // lower-cased shop name
    std::map<std::string, std::set<std::string>> m_carMods;   // carmods.dat "mods": car dff name -> its components
    bool ModFits(uint32_t key) const;                      // a car mod shop lists only what the player's car takes
    std::map<std::string, int> m_componentIds;            // veh_mods.ide: component name -> model id
    std::vector<std::pair<uint32_t, int>> m_modifiers;    // ms_priceModifiers
    std::vector<Row> m_rows;                              // ms_prices: the loaded price section
    std::string m_section;                                // ms_priceSectionLoaded (lower case)
    std::vector<uint32_t> m_contents;                     // ms_shopContents
    std::string m_shopLoaded;
    PedClothesDesc m_stored;
    bool m_clothesStored = false;
    int m_property = -1;
};
