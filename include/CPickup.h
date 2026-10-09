// CPickup - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Pickup.h
// Single world pickup (weapon, money, collectable, property, ...).
//
// Adaptations: stripped plugin-sdk header block, InjectHooks(), VALIDATE_SIZE ->
// guarded static_assert. CompressedLargeVector is a minimal local replacement for
// gta-reversed's extensions/FixedVector.hpp (FixedVector<int16, 8.0f>, 6 bytes);
// the full fixed-vector port comes with a later subsystem. GetPosn2D() is spelled
// out explicitly: the adapted CVector2D has no implicit CVector conversion.

#pragma once

#include "CVector.h" // CVector, CVector2D

#include <cstdint>

// ---- Forward declarations (ported in later subsystems) ----
class CObject;
class CVehicle;
class CPlayerPed;

// Minimal replacement for gta-reversed's FixedVector<int16, 8.0f> (see
// source/game_sa/CompressedVector.h). Packs a world position into 3 int16s.
// TODO: port extensions/FixedVector.hpp and use it here; verify the exact
// fixed-point semantics (stored = world * 8.0f) against the decomp.
struct CompressedLargeVector {
    int16_t x{}, y{}, z{};

    constexpr CompressedLargeVector() = default;
    constexpr CompressedLargeVector(const CVector& v)
        : x{ static_cast<int16_t>(v.x * 8.0f) },
          y{ static_cast<int16_t>(v.y * 8.0f) },
          z{ static_cast<int16_t>(v.z * 8.0f) } {}

    constexpr operator CVector() const {
        return CVector{ x / 8.0f, y / 8.0f, z / 8.0f };
    }
};

enum ePickupType : uint8_t {
    PICKUP_NONE = 0,
    PICKUP_IN_SHOP = 1,
    PICKUP_ON_STREET = 2,
    PICKUP_ONCE = 3,
    PICKUP_ONCE_TIMEOUT = 4,
    PICKUP_ONCE_TIMEOUT_SLOW = 5,
    PICKUP_COLLECTABLE1 = 6,
    PICKUP_IN_SHOP_OUT_OF_STOCK = 7,
    PICKUP_MONEY = 8,
    PICKUP_MINE_INACTIVE = 9,
    PICKUP_MINE_ARMED = 10,
    PICKUP_NAUTICAL_MINE_INACTIVE = 11,
    PICKUP_NAUTICAL_MINE_ARMED = 12,
    PICKUP_FLOATINGPACKAGE = 13,
    PICKUP_FLOATINGPACKAGE_FLOATING = 14,
    PICKUP_ON_STREET_SLOW = 15,
    PICKUP_ASSET_REVENUE = 16,
    PICKUP_PROPERTY_LOCKED = 17,
    PICKUP_PROPERTY_FORSALE = 18,
    PICKUP_MONEY_DOESNTDISAPPEAR = 19,
    PICKUP_SNAPSHOT = 20,
    PICKUP_2P = 21,
    PICKUP_ONCE_FOR_MISSION = 22
};

enum ePickupPropertyText : int32_t {
    PICKUP_PROPERTY_TEXT_CANCEL   = 0, // "Cancel"
    PICKUP_PROPERTY_TEXT_CAN_BUY  = 1, // "Press TAB to buy ..."
    PICKUP_PROPERTY_TEXT_CANT_BUY = 2, // "You can't buy..."
};

class CPickup {
public:
    float                 m_fRevenueValue;
    CObject*              m_pObject;
    uint32_t              m_nAmmo;
    uint32_t              m_nRegenerationTime;
    CompressedLargeVector m_vecPos;
    uint16_t              m_nMoneyPerDay;
    int16_t               m_nModelIndex;
    int16_t               m_nReferenceIndex;
    ePickupType           m_nPickupType;
    struct {
        uint8_t bDisabled : 1; // waiting for regeneration
        uint8_t bEmpty : 1;    // no ammo
        uint8_t bHelpMessageDisplayed : 1;
        uint8_t bVisible : 1;
        uint8_t nPropertyTextIndex : 3; // see enum ePickupPropertyText
    } m_nFlags;

public:
    void SetPosn(CVector posn) { m_vecPos = posn; } // 0x454960
    [[nodiscard]] CVector GetPosn() const { return m_vecPos; } // 0x4549A0
    // TODO: adapted CVector2D has no implicit CVector conversion; restore the
    // original one-liner if that changes.
    [[nodiscard]] CVector2D GetPosn2D() const { const auto p = GetPosn(); return { p.x, p.y }; }
    [[nodiscard]] float GetXCoord() const { return m_vecPos.x; } // 0x4549F0
    [[nodiscard]] float GetYCoord() const { return m_vecPos.y; } // 0x454A10
    [[nodiscard]] float GetZCoord() const { return m_vecPos.z; } // 0x454A30
    void SetXCoord(float coord) { m_vecPos.x = coord; }
    void SetYCoord(float coord) { m_vecPos.y = coord; }
    void SetZCoord(float coord) { m_vecPos.z = coord; }

    void ExtractAmmoFromPickup(CPlayerPed* player);
    [[nodiscard]] bool IsVisible();
    void GetRidOfObjects();
    bool PickUpShouldBeInvisible();
    void Remove();
    void GiveUsAPickUpObject(CObject*& obj, int32_t slotIndex = -1);
    bool Update(CPlayerPed* player, CVehicle* vehicle, int32_t playerId);
    void ProcessGunShot(const CVector& origin, const CVector& target);

    static ePickupPropertyText FindTextIndexForString(const char* message);
    static const char* FindStringForTextIndex(ePickupPropertyText index);
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CompressedLargeVector) == 0x6, "CompressedLargeVector layout changed");
static_assert(sizeof(CPickup) == 0x20, "CPickup layout changed");
#endif
