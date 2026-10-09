// CPickups - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Pickups.h
// Manager for the world's pickup pool (aPickUps) plus pickup messages.
//
// Adaptations: stripped plugin-sdk header block, InjectHooks(), StaticRef
// globals (now plain static members; original 1.0 US addresses kept as comments
// and re-resolved in CPickups.cpp for the clean-room build), VALIDATE_SIZE ->
// guarded static_assert. tPickupMessage (from gta-reversed's tPickupMessage.h)
// is defined here until a headers pass ports it separately. eWeaponType is only
// used as a parameter/return type here; the full enum is ported with the weapons
// subsystem. The NOTSA GetAllActivePickups() range helper needs std::views
// (C++20) — commented out until the project moves past C++17.

#pragma once

#include "CVector.h"     // CVector
#include "RenderTypes.h" // CRGBA, GxtChar
#include "CPickup.h"     // CPickup, ePickupType
#include "eWeaponType.h" // eWeaponType, NUM_WEAPONS (canonical)

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

// ---- Forward declarations (ported in later subsystems) ----
class CEntity;

// eWeaponType/NUM_WEAPONS: canonical from eWeaponType.h (deduped 2026-10-09)

struct tPickupMessage {
public:
    CVector  pos;
    float    width;
    float    height;
    CRGBA    color;
    uint8_t  flags;
    char     field_19;
    uint32_t price;
    GxtChar* text;
};

struct tPickupReference;

constexpr uint32_t MAX_COLLECTED_PICKUPS = 20;
constexpr uint32_t MAX_PICKUP_MESSAGES = 16;
constexpr uint32_t MAX_NUM_PICKUPS = 620;

// Dump @ 0x8A5F50
constexpr uint16_t AmmoForWeapon_OnStreet[NUM_WEAPONS]{ 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 8u, 8u, 8u, 8u, 4u, 4u, 30u, 10u, 10u, 15u, 10u, 10u, 60u, 60u, 80u, 80u, 60u, 20u, 10u, 4u, 3u, 100u, 500u, 5u, 1u, 500u, 500u, 36u, 0u, 0u, 1u };

class CPickups {
public:
    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CPickups.cpp.
    // TODO: re-resolve for the clean-room build.
    static uint8_t  DisplayHelpMessage;   // 0x8A5F48
    static int32_t  PlayerOnWeaponPickup; // 0x97D640
    static int32_t  StaticCamStartTime;   // 0x978618
    static CVector* StaticCamCoors;       // was (CVector*)0x97D660
    static CVehicle* pPlayerVehicle;      // 0x97861C
    static bool     bPickUpcamActivated;  // 0x978620
    static uint16_t CollectedPickUpIndex; // 0x978624
    static std::array<int32_t, MAX_COLLECTED_PICKUPS> aPickUpsCollected; // 0x978628
    static uint16_t NumMessages;          // 0x978678
    static std::array<tPickupMessage, MAX_PICKUP_MESSAGES> aMessages; // 0x978680
    static std::array<CPickup, MAX_NUM_PICKUPS> aPickUps; // 0x9788C0

public:
    static void Init();
    static void ReInit();

    static void AddToCollectedPickupsArray(int32_t pickupIndex);
    static void CreatePickupCoorsCloseToCoors(float inX, float inY, float inZ, float& outX, float& outY, float& outZ);
    static void CreateSomeMoney(CVector coors, int32_t amount);
    static void DetonateMinesHitByGunShot(const CVector& shotOrigin, const CVector& shotTarget);

    static void DoCollectableEffects(CEntity* entity);
    static void DoMineEffects(CEntity* entity);
    static void DoMoneyEffects(CEntity* entity);
    static void DoPickUpEffects(CEntity* entity);

    static CPickup* FindPickUpForThisObject(CObject* object);
    static tPickupReference GenerateNewOne(CVector coors, uint32_t modelId, ePickupType pickupType, uint32_t ammo, uint32_t moneyPerDay = 0u, bool isEmpty = false, char* message = nullptr);
    static tPickupReference GenerateNewOne_WeaponType(CVector coors, eWeaponType weaponType, ePickupType pickupType, uint32_t ammo, bool isEmpty, char* message);
    static int32_t GetActualPickupIndex(tPickupReference pickupIndex);
    static tPickupReference GetNewUniquePickupIndex(int32_t pickupIndex);
    static tPickupReference GetUniquePickupIndex(int32_t pickupIndex);
    static bool GivePlayerGoodiesWithPickUpMI(uint16_t modelId, int32_t playerId);
    static bool IsPickUpPickedUp(tPickupReference pickupRef);
    static int32_t ModelForWeapon(eWeaponType weaponType);
    static void PassTime(uint32_t time);
    static void PickedUpHorseShoe();
    static void PickedUpOyster();
    static void PictureTaken();
    static bool PlayerCanPickUpThisWeaponTypeAtThisMoment(eWeaponType weaponType);
    static void RemoveMissionPickUps();
    static void RemovePickUp(tPickupReference pickupRef);
    static void RemovePickUpsInArea(float minX, float maxX, float minY, float maxY, float minZ, float maxZ);
    static void RemovePickupObjects();
    static void RemoveUnnecessaryPickups(const CVector& posn, float radius);
    static void RenderPickUpText();
    static bool TestForPickupsInBubble(const CVector posn, float radius);
    static bool TryToMerge_WeaponType(CVector posn, eWeaponType weaponType, ePickupType pickupType, uint32_t ammo, bool arg4);
    static void Update();
    static void UpdateMoneyPerDay(tPickupReference pickupRef, uint16_t money);
    static eWeaponType WeaponForModel(int32_t modelId);
    static void Load();
    static void Save();

    // Helpers NOTSA

    /*!
     * @brief Our custom Vector based overload
     * @copydoc CPickups::CreatePickupCoorsCloseToCoors
     */
    static void CreatePickupCoorsCloseToCoors(const CVector& pos, CVector& createdAtPos) {
        return CreatePickupCoorsCloseToCoors(pos.x, pos.y, pos.z, createdAtPos.x, createdAtPos.y, createdAtPos.z);
    }
    /*!
     * @brief Our custom Vector based overload
     * @copydoc CPickups::CreatePickupCoorsCloseToCoors
     */
    static void CreatePickupCoorsCloseToCoors(const CVector& pos, float& x, float& y, float& z) {
        return CreatePickupCoorsCloseToCoors(pos.x, pos.y, pos.z, x, y, z);
    }

    // TODO: std::views is C++20; restore this helper when the project moves past C++17
    // (or write a plain loop over aPickUps).
    // static auto GetAllActivePickups() { return aPickUps | std::views::filter([](auto&& p) { return p.m_nPickupType != PICKUP_NONE; }); }
};

// NOTSA
struct tPickupReference {
    union {
        struct {
            int16_t index;
            int16_t refIndex;
        };

        int32_t num;
    };

    tPickupReference() :
        num(-1) {}

    tPickupReference(int32_t value) :
        num(value) {}

    tPickupReference(int16_t idx, int16_t refIdx) :
        index(idx),
        refIndex(refIdx) {}

    tPickupReference(CPickup& pickup) {
        ptrdiff_t arrIndex = (&pickup - CPickups::aPickUps.data());
        assert(arrIndex >= 0 && arrIndex < MAX_NUM_PICKUPS);
        index    = static_cast<int16_t>(arrIndex);
        refIndex = pickup.m_nReferenceIndex;
    }
};

// was `static inline auto& CollectPickupBuffer = StaticRef<int32>(0x97D644)`;
// C++17 inline variable keeps the single-definition semantics.
inline int32_t CollectPickupBuffer{}; // 0x97D644; TODO: re-resolve

void ModifyStringLabelForControlSetting(char* stringLabel);

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tPickupMessage) == 0x24, "tPickupMessage layout changed");
static_assert(sizeof(tPickupReference) == 0x4, "tPickupReference layout changed");
#endif
