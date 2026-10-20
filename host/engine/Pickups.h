#pragma once
// CPickups / CPickup: the 620 pickup slots. A pickup is a model on the ground that the player
// collects by walking into it (|dz| < 2 and 2D distance < sqrt(1.8)). The rules follow
// CPickup::Update @ 00457410 and CPickups::GenerateNewOne.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <d3d9.h>

class D3DRenderer;
struct D3DRenderMesh;

class Pickups {
public:
    static constexpr int kSlots = 620;
    // ePickupType (the ones that exist without scripts)
    enum Type {
        OnStreet = 2, Once = 3, OnceTimeout = 4, OnceTimeoutSlow = 5, Money = 8,
        OnStreetSlow = 15, MoneyDoesntDisappear = 19, OnceForMission = 22,
    };
    // What the model is: MI_PICKUP_* or a weapon.
    enum Kind { Weapon, Cash, Health, Armour, Adrenaline, Info, Bribe, Bonus, Jetpack, kKinds };

    struct Slot {
        bool used = false;
        int type = 0, kind = Weapon, weapon = 0, ammo = 0;
        float pos[3] = {0, 0, 0};
        uint32_t regenMs = 0;               // m_nRegenerationTime: expiry or respawn time
        bool disabled = false, empty = false, helpShown = false, skipLogged = false;
    };

    struct Env {
        float player[3] = {0, 0, 0};          // the player ped position
        bool alive = true, onFoot = true;
        float health = 100, maxHealth = 100, armour = 0, maxArmour = 100;
        int wanted = 0;
        bool onJetpack = false;
        bool collectKey = false;              // CPickups::CollectPickupBuffer
        uint32_t msSinceHsLos = 100000;       // CPlayerData::m_nLastHSMissileLOSTime age
        // 0 slot empty or same weapon, 1 another weapon with ammo (or an exchangeable slot): needs the key,
        // 2 another weapon without ammo
        std::function<int(int weapon)> slotState;
        std::function<bool(int weapon)> wantsWeapon;       // CPlayerPed::DoesPlayerWantNewWeapon(weapon, false)
        std::function<bool(int weapon)> canPickNow;        // PlayerCanPickUpThisWeaponTypeAtThisMoment
        std::function<bool(int weapon)> exchangeable;      // the weapon's slot is 3, 4 or 5
        std::function<void(int weapon, int ammo)> grantWeapon;
        std::function<void(int weapon, int ammo)> grantAmmo;     // CPed::GrantAmmo: into the weapon of that slot
        std::function<void()> reloadSlot;                  // pedWeapon.Reload() after the extract
        std::function<void(const char* key)> help;
        std::function<void(int amount)> giveMoney;
        std::function<void(int kind)> goodie;              // Health, Armour, Adrenaline, Info, Bribe, Bonus, Jetpack
        std::function<void(const char* what, int slot, int a, int b)> log;
    };

    void Clear();
    // CPickups::GenerateNewOne. Returns the slot, -1 when all are in use and none can be replaced.
    int Add(const float pos[3], int kind, int weapon, int type, int ammo, bool empty, uint32_t nowMs);
    // CPickups::Update, every slot.
    void Update(uint32_t nowMs, Env& env);
    const Slot& At(int i) const { return m_s[i]; }
    void Remove(int i) { if (i >= 0 && i < kSlots) m_s[i] = Slot(); }   // CPickup::Remove
    int Used() const;
    // CPickups::CreateSomeMoney: wads of cash around a position (z already above the ground).
    void CreateSomeMoney(const float pos[3], int amount, uint32_t nowMs);
    // The AmmoForWeapon_OnStreet table.
    static int OnStreetAmmo(int weapon);

    // Models.
    bool Load(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    // world: row-major matrix handed to the weapon drawer.
    void Draw(D3DRenderer& r, uint32_t nowMs, const float cam[3],
              const std::function<void(int weapon, const float world[4][4])>& drawWeapon,
              const std::function<void(const float world[4][4])>& drawJetpack);

private:
    struct Model {
        std::vector<D3DRenderMesh*> meshes;
        std::vector<IDirect3DTexture9*> textures;
        float maxDim = 1.0f;
    };
    bool LoadModel(D3DRenderer& r, void* img, const char* dff, const char* txd, Model& out);
    Slot m_s[kSlots];
    Model m_model[kKinds];
    unsigned m_rng = 4242;
    int Rand() { m_rng = m_rng * 1664525u + 1013904223u; return (int)((m_rng >> 8) & 0xFFFF); }
};
