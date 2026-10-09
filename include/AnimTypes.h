// AnimTypes - adapted from gta-reversed for clean-room C++ build
// Minimal stand-ins for small animation-adjacent types that the animation
// subsystem headers reference but which belong to other subsystems (not yet
// converted). Each is a faithful DATA LAYOUT copied from its gta-reversed
// source (sizes cross-checked against the VALIDATE_SIZE in the original).
// Methods are omitted until the owning subsystem is converted - see TODOs.
// Follows the precedent of include/ColTypes.h (collision subsystem).
//
// SOURCES (all under gta-reversed/source/game_sa/):
//   AssocGroupId                 <- Enums/AnimationEnums.h (copied in full)
//   AnimationId                  <- Enums/AnimationEnums.h (MINIMAL - see below)
//   eAnimBlendCallbackType       <- Enums/eAnimBlendCallbackType.h
//   eBoneTag                     <- Enums/eBoneTag.h
//   notsa::WEnumS16/WEnumS32     <- extensions/WEnum.hpp (plugin-sdk; C++17 shim)
//   notsa::span                  <- C++17 stand-in for std::span (C++20)
//   AnimDescriptor/AnimAssocDefinition <- Animation/AnimationStyleDescriptor.h (0x30)
//   CAnimBlock                   <- Animation/AnimBlock.h (0x20)
//   AnimBlendFrameData           <- Animation/AnimBlendFrameData.h (0x18)
//   KeyFrame/KeyFrameTrans       <- Animation/AnimSequenceFrames.h (0x14/0x20)
//   KeyFrameCompressed/TransCompressed <- same (0xA/0x10)
//   CQuaternion (minimal)        <- Quaternion.h (0x10; full class not yet converted)
//   lerp                         <- gta-reversed extensions (trivial, inlined here)
//
// RenderWare SDK types are forward-declared (RW layer not yet converted):
//   RpClump, RwStream, RwLLLink, RwFrame, RpHAnimBlendInterpFrame
//
// Types that already have converted headers are NOT duplicated here:
//   CVector            <- include/CVector.h
//   FixedFloat/FixedVector, CLink/CLinkList <- include/ColTypes.h

#pragma once

#include "CVector.h"
#include "ColTypes.h" // FixedFloat/FixedVector (C++17), CLink/CLinkList

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>

// ---------------------------------------------------------------------------
// RenderWare SDK forward declarations (RW layer not yet converted).
// ---------------------------------------------------------------------------
struct RpClump;
struct RwStream;
struct RwLLLink;
struct RwFrame;
struct RpHAnimBlendInterpFrame;
struct IFPSectionHeader;

// ---------------------------------------------------------------------------
// extensions/WEnum.hpp (plugin-sdk) - C++17 stand-in.
// Wraps an enum in a smaller integer for storage while keeping the enum API.
// Must stay a literal type with a trivial default ctor: it is used as a
// union member with a default member initializer
// (CAnimBlendSequence::m_BoneTag).
// TODO: replace with converted extensions/ headers if the project moves to C++20
// ---------------------------------------------------------------------------
namespace notsa {
template <typename E, typename Storage = int16_t>
class WEnum {
    Storage v{};

public:
    constexpr WEnum() = default;
    constexpr WEnum(E e) : v(static_cast<Storage>(e)) {}
    constexpr operator E() const { return static_cast<E>(v); }
    constexpr WEnum& operator=(E e) {
        v = static_cast<Storage>(e);
        return *this;
    }
    constexpr bool operator==(E e) const { return static_cast<E>(v) == e; }
    constexpr bool operator!=(E e) const { return !(*this == e); }
    // NOTE: no WEnum-to-WEnum comparison - compare via the enum value.
};
template <typename E>
using WEnumS16 = WEnum<E, int16_t>;
template <typename E>
using WEnumS32 = WEnum<E, int32_t>;
template <typename E>
using WEnumU32 = WEnum<E, uint32_t>;

// ---------------------------------------------------------------------------
// C++17 stand-in for std::span<T> (C++20). Pointer + size, nothing more.
// Used everywhere gta-reversed used std::span in these headers.
// Mechanical upgrade path: s/notsa::span/std::span/ if the standard is raised.
// ---------------------------------------------------------------------------
template <typename T>
class span {
    T* m_data{};
    size_t m_size{};

public:
    constexpr span() = default;
    constexpr span(T* data, size_t size) : m_data(data), m_size(size) {}

    constexpr T* data() const { return m_data; }
    constexpr size_t size() const { return m_size; }
    constexpr bool empty() const { return m_size == 0; }
    constexpr T* begin() const { return m_data; }
    constexpr T* end() const { return m_data + m_size; }
    constexpr T& operator[](size_t i) const {
        assert(i < m_size);
        return m_data[i];
    }
};
} // namespace notsa

// ---------------------------------------------------------------------------
// Enums/AnimationEnums.h - AssocGroupId copied in full (values as in gta-reversed).
// ---------------------------------------------------------------------------
enum AssocGroupId : int32_t {
    ANIM_GROUP_NONE = -1,
    ANIM_GROUP_DEFAULT = 0,
    ANIM_GROUP_DOOR = 1,
    ANIM_GROUP_BIKES = 2,
    ANIM_GROUP_BIKEV = 3,
    ANIM_GROUP_BIKEH = 4,
    ANIM_GROUP_BIKED = 5,
    ANIM_GROUP_WAYFARER = 6,
    ANIM_GROUP_BMX = 7,
    ANIM_GROUP_MTB = 8,
    ANIM_GROUP_CHOPPA = 9,
    ANIM_GROUP_QUAD = 10,

    ANIM_GROUP_PYTHON = 11,
    ANIM_GROUP_PYTHONBAD = 12,
    ANIM_GROUP_COLT45 = 13,
    ANIM_GROUP_COLT_COP = 14,
    ANIM_GROUP_COLT45PRO = 15,
    ANIM_GROUP_SAWNOFF = 16,
    ANIM_GROUP_SAWNOFFPRO = 17,
    ANIM_GROUP_SILENCED = 18,
    ANIM_GROUP_SHOTGUN = 19,
    ANIM_GROUP_SHOTGUNBAD = 20,
    ANIM_GROUP_BUDDY = 21,
    ANIM_GROUP_BUDDYBAD = 22,
    ANIM_GROUP_UZI = 23,
    ANIM_GROUP_UZIBAD = 24,
    ANIM_GROUP_RIFLE = 25,
    ANIM_GROUP_RIFLEBAD = 26,
    ANIM_GROUP_SNIPER = 27,
    ANIM_GROUP_GRENADE = 28,
    ANIM_GROUP_FLAME = 29,
    ANIM_GROUP_ROCKET = 30,
    ANIM_GROUP_SPRAYCAN = 31,

    ANIM_GROUP_GOGGLES = 32,
    ANIM_GROUP_MELEE_1 = 33,
    ANIM_GROUP_MELEE_2 = 34,
    ANIM_GROUP_MELEE_3 = 35,
    ANIM_GROUP_MELEE_4 = 36,
    ANIM_GROUP_BBBAT_1 = 37,
    ANIM_GROUP_GCLUB_1 = 38,
    ANIM_GROUP_KNIFE_1 = 39,
    ANIM_GROUP_SWORD_1 = 40,
    ANIM_GROUP_DILDO_1 = 41,
    ANIM_GROUP_FLOWERS_1 = 42,
    ANIM_GROUP_CSAW_1 = 43,
    ANIM_GROUP_KICK_STD = 44,
    ANIM_GROUP_PISTLWHP = 45,
    ANIM_GROUP_MEDIC = 46,
    ANIM_GROUP_BEACH = 47,
    ANIM_GROUP_SUNBATHE = 48,
    ANIM_GROUP_PLAYIDLES = 49,
    ANIM_GROUP_RIOT = 50,
    ANIM_GROUP_STRIP = 51,
    ANIM_GROUP_GANGS = 52,
    ANIM_GROUP_ATTRACTORS = 53,
    ANIM_GROUP_PLAYER = 54,
    ANIM_GROUP_FAT = 55,
    ANIM_GROUP_MUSCULAR = 56,
    ANIM_GROUP_PLAYERROCKET = 57,
    ANIM_GROUP_PLAYERROCKETF = 58,
    ANIM_GROUP_PLAYERROCKETM = 59,
    ANIM_GROUP_PLAYER2ARMED = 60,
    ANIM_GROUP_PLAYER2ARMEDF = 61,
    ANIM_GROUP_PLAYER2ARMEDM = 62,
    ANIM_GROUP_PLAYERBBBAT = 63,
    ANIM_GROUP_PLAYERBBBATF = 64,
    ANIM_GROUP_PLAYERBBBATM = 65,
    ANIM_GROUP_PLAYERCSAW = 66,
    ANIM_GROUP_PLAYERCSAWF = 67,
    ANIM_GROUP_PLAYERCSAWM = 68,
    ANIM_GROUP_PLAYERSNEAK = 69,
    ANIM_GROUP_PLAYERJETPACK = 70,
    ANIM_GROUP_SWIM = 71,
    ANIM_GROUP_DRIVEBYS = 72,
    ANIM_GROUP_BIKE_DBZ = 73,
    ANIM_GROUP_COP_DBZ = 74,
    ANIM_GROUP_QUAD_DBZ = 75,
    ANIM_GROUP_FAT_TIRED = 76,
    ANIM_GROUP_HANDSIGNAL = 77,
    ANIM_GROUP_HANDSIGNALL = 78,
    ANIM_GROUP_LHAND = 79,
    ANIM_GROUP_RHAND = 80,
    ANIM_GROUP_CARRY = 81,
    ANIM_GROUP_CARRY05 = 82,
    ANIM_GROUP_CARRY105 = 83,
    ANIM_GROUP_INT_HOUSE = 84,
    ANIM_GROUP_INT_OFFICE = 85,
    ANIM_GROUP_INT_SHOP = 86,
    ANIM_GROUP_STEALTH_KN = 87,

    ANIM_GROUP_CARS_BEGIN,
    ANIM_GROUP_STDCARAMIMS = ANIM_GROUP_CARS_BEGIN,
    ANIM_GROUP_LOWCARAMIMS = 89,
    ANIM_GROUP_TRKCARANIMS = 90,
    ANIM_GROUP_STDBIKEANIMS = 91,
    ANIM_GROUP_SPORTBIKEANIMS = 92,
    ANIM_GROUP_VESPABIKEANIMS = 93,
    ANIM_GROUP_HARLEYBIKEANIMS = 94,
    ANIM_GROUP_DIRTBIKEANIMS = 95,
    ANIM_GROUP_WAYFBIKEANIMS = 96,
    ANIM_GROUP_BMXBIKEANIMS = 97,
    ANIM_GROUP_MTBBIKEANIMS = 98,
    ANIM_GROUP_CHOPPABIKEANIMS = 99,
    ANIM_GROUP_QUADBIKEANIMS = 100,
    ANIM_GROUP_VANCARANIMS = 101,
    ANIM_GROUP_RUSTPLANEANIMS = 102,
    ANIM_GROUP_COACHCARANIMS = 103,
    ANIM_GROUP_BUSCARANIMS = 104,
    ANIM_GROUP_DOZERCARANIMS = 105,
    ANIM_GROUP_KARTCARANIMS = 106,
    ANIM_GROUP_CONVCARANIMS = 107,
    ANIM_GROUP_MTRKCARANIMS = 108,
    ANIM_GROUP_TRAINCARRANIMS = 109,
    ANIM_GROUP_STDTALLCARAMIMS = 110,
    ANIM_GROUP_HOVERCARANIMS = 111,
    ANIM_GROUP_TANKCARANIMS = 112,
    ANIM_GROUP_BFINJCARAMIMS = 113,
    ANIM_GROUP_LEARPLANEANIMS = 114,
    ANIM_GROUP_HARRPLANEANIMS = 115,
    ANIM_GROUP_STDCARUPRIGHT = 116,
    ANIM_GROUP_NVADAPLANEANIMS = 117,
    ANIM_GROUP_CARS_END,

    ANIM_GROUP_MAN = ANIM_GROUP_CARS_END,
    ANIM_GROUP_SHUFFLE = 119,
    ANIM_GROUP_OLDMAN = 120,
    ANIM_GROUP_GANG1 = 121,
    ANIM_GROUP_GANG2 = 122,
    ANIM_GROUP_OLDFATMAN = 123,
    ANIM_GROUP_FATMAN = 124,
    ANIM_GROUP_JOGGER = 125,
    ANIM_GROUP_DRUNKMAN = 126,
    ANIM_GROUP_BLINDMAN = 127,
    ANIM_GROUP_SWAT = 128,
    ANIM_GROUP_WOMAN = 129,
    ANIM_GROUP_SHOPPING = 130,
    ANIM_GROUP_BUSYWOMAN = 131,
    ANIM_GROUP_SEXYWOMAN = 132,
    ANIM_GROUP_PRO = 133,
    ANIM_GROUP_OLDWOMAN = 134,
    ANIM_GROUP_FATWOMAN = 135,
    ANIM_GROUP_JOGWOMAN = 136,
    ANIM_GROUP_OLDFATWOMAN = 137,
    ANIM_GROUP_SKATE = 138,

    ANIM_TOTAL_GROUPS,
};

// ---------------------------------------------------------------------------
// Enums/AnimationEnums.h - AnimationId: MINIMAL.
// The full enum lists every animation ID in the game (~100KB in gta-reversed);
// only the sentinel is referenced by these headers. Copy values on demand
// from gta-reversed Enums/AnimationEnums.h (verified against decomp first).
// TODO: fill in the full AnimationId enum when the ped/task subsystems need it.
// ---------------------------------------------------------------------------
enum AnimationId : int32_t {
    ANIM_ID_UNDEFINED = -1, // verified in gta-reversed Enums/AnimationEnums.h
    ANIM_ID_FLOOR_HIT   = 36, // copied on demand for CBulletInfo (gta-reversed)
    ANIM_ID_FLOOR_HIT_F = 39, // copied on demand for CBulletInfo (gta-reversed)

    // Added 2026-10-09 for CAnimManager::aStdAnimDescs /
    // GetRandomGangTalkAnim (values verified against
    // gta-reversed/source/game_sa/Enums/AnimationEnums.h).
    ANIM_ID_WALK = 0,
    ANIM_ID_RUN = 1,
    ANIM_ID_SPRINT = 2,
    ANIM_ID_IDLE = 3,
    ANIM_ID_ROADCROSS = 4,
    ANIM_ID_WALK_START = 5,
    ANIM_ID_RUN_STOP = 6,
    ANIM_ID_RUN_STOPR = 7,
    ANIM_ID_IDLE_HBHB_0 = 8,
    ANIM_ID_IDLE_HBHB_1 = 9,
    ANIM_ID_IDLE_TIRED = 10,
    ANIM_ID_IDLE_ARMED = 11,
    ANIM_ID_IDLE_CHAT = 12,
    ANIM_ID_IDLE_TAXI = 13,
    ANIM_ID_SWIM_TREAD = 14,
    ANIM_ID_KO_SHOT_FRONT_0 = 15,
    ANIM_ID_KO_SHOT_FRONT_1 = 16,
    ANIM_ID_KO_SHOT_FRONT_2 = 17,
    ANIM_ID_KO_SHOT_FRONT_3 = 18,
    ANIM_ID_KO_SHOT_FACE = 19,
    ANIM_ID_KO_SHOT_STOM = 20,
    ANIM_ID_GAS_CWR = 21,
    ANIM_ID_KD_LEFT = 22,
    ANIM_ID_KD_RIGHT = 23,
    ANIM_ID_KO_SKID_FRONT = 24,
    ANIM_ID_KO_SPIN_R = 25,
    ANIM_ID_KO_SKID_BACK = 26,
    ANIM_ID_KO_SPIN_L = 27,
    ANIM_ID_SHOT_PARTIAL = 28,
    ANIM_ID_SHOT_LEFTP = 29,
    ANIM_ID_SHOT_PARTIAL_B = 30,
    ANIM_ID_SHOT_RIGHTP = 31,
    ANIM_ID_HIT_FRONT = 32,
    ANIM_ID_HIT_L = 33,
    ANIM_ID_HIT_BACK = 34,
    ANIM_ID_HIT_R = 35,
    ANIM_ID_HIT_WALK = 37,
    ANIM_ID_HIT_WALL = 38,
    ANIM_ID_HIT_BEHIND = 40,
    ANIM_ID_FIGHTSH_FWD = 41,
    ANIM_ID_FIGHTSH_LEFT = 42,
    ANIM_ID_FIGHTSH_BWD = 43,
    ANIM_ID_FIGHTSH_RIGHT = 44,
    ANIM_ID_FIGHTSHF = 45,
    ANIM_ID_FIGHTSHB = 46,
    ANIM_ID_FIGHT2IDLE = 47,
    ANIM_ID_BOMBER = 48,
    ANIM_ID_GUN_STAND = 49,
    ANIM_ID_GUNMOVE_FWD = 50,
    ANIM_ID_GUNMOVE_L = 51,
    ANIM_ID_GUNMOVE_BWD = 52,
    ANIM_ID_GUNMOVE_R = 53,
    ANIM_ID_GUN_2_IDLE = 54,
    ANIM_ID_WEAPON_CROUCH = 55,
    ANIM_ID_GUNCROUCHFWD = 56,
    ANIM_ID_CROUCH_ROLL_L = 57,
    ANIM_ID_GUNCROUCHBWD = 58,
    ANIM_ID_CROUCH_ROLL_R = 59,
    ANIM_ID_CAR_SIT = 60,
    ANIM_ID_CAR_LSIT = 61,
    ANIM_ID_CAR_SIT_WEAK = 62,
    ANIM_ID_CAR_SIT_PRO = 63,
    ANIM_ID_CAR_SITP = 64,
    ANIM_ID_CAR_SITPLO = 65,
    ANIM_ID_DRIVE_L = 66,
    ANIM_ID_DRIVE_R = 67,
    ANIM_ID_DRIVE_LO_L = 68,
    ANIM_ID_DRIVE_LO_R = 69,
    ANIM_ID_DRIVE_L_WEAK = 70,
    ANIM_ID_DRIVE_R_WEAK = 71,
    ANIM_ID_DRIVE_L_PRO = 72,
    ANIM_ID_DRIVE_R_PRO = 73,
    ANIM_ID_DRIVEBY_L = 74,
    ANIM_ID_DRIVEBY_R = 75,
    ANIM_ID_DRIVEBYL_L = 76,
    ANIM_ID_DRIVEBYL_R = 77,
    ANIM_ID_CAR_LB = 78,
    ANIM_ID_CAR_LB_WEAK = 79,
    ANIM_ID_CAR_LB_PRO = 80,
    ANIM_ID_DRIVE_BOAT = 81,
    ANIM_ID_DRIVE_BOAT_L = 82,
    ANIM_ID_DRIVE_BOAT_R = 83,
    ANIM_ID_DRIVE_BOAT_BACK = 84,
    ANIM_ID_DRIVE_L_SLOW = 85,
    ANIM_ID_DRIVE_R_SLOW = 86,
    ANIM_ID_DRIVE_L_WEAK_SLOW = 87,
    ANIM_ID_DRIVE_R_WEAK_SLOW = 88,
    ANIM_ID_DRIVE_L_PRO_SLOW = 89,
    ANIM_ID_DRIVE_R_PRO_SLOW = 90,
    ANIM_ID_DRIVE_TRUCK = 91,
    ANIM_ID_DRIVE_TRUCK_L = 92,
    ANIM_ID_DRIVE_TRUCK_R = 93,
    ANIM_ID_DRIVE_TRUCK_BACK = 94,
    ANIM_ID_KART_DRIVE = 95,
    ANIM_ID_KART_L = 96,
    ANIM_ID_KART_R = 97,
    ANIM_ID_KART_LB = 98,
    ANIM_ID_BIKE_PICKUPR = 99,
    ANIM_ID_BIKE_PICKUPL = 100,
    ANIM_ID_BIKE_PULLUPR = 101,
    ANIM_ID_BIKE_PULLUPL = 102,
    ANIM_ID_BIKE_ELBOWL = 103,
    ANIM_ID_BIKE_ELBOWR = 104,
    ANIM_ID_BIKE_FALL_OFF = 105,
    ANIM_ID_BIKE_FALLR = 106,
    ANIM_ID_CAR_HOOKERTALK = 107,
    ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0 = 108,
    ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1 = 109,
    ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS = 110,
    ANIM_ID_DEFAULT_CAR_ROLLOUT_RHS = 111,
    ANIM_ID_GETUP_0 = 112,
    ANIM_ID_GETUP_1 = 113,
    ANIM_ID_GETUP_2 = 114,
    ANIM_ID_GETUP_FRONT = 115,
    ANIM_ID_JUMP_LAUNCH = 116,
    ANIM_ID_JUMP_LAUNCH_R = 117,
    ANIM_ID_JUMP_GLIDE = 118,
    ANIM_ID_JUMP_LAND = 119,
    ANIM_ID_FALL_FALL = 120,
    ANIM_ID_FALL_GLIDE = 121,
    ANIM_ID_FALL_LAND = 122,
    ANIM_ID_FALL_COLLAPSE = 123,
    ANIM_ID_FALL_BACK = 124,
    ANIM_ID_FALL_FRONT = 125,
    ANIM_ID_EV_STEP = 126,
    ANIM_ID_EV_DIVE = 127,
    ANIM_ID_CLIMB_JUMP = 128,
    ANIM_ID_CLIMB_IDLE = 129,
    ANIM_ID_CLIMB_PULL = 130,
    ANIM_ID_CLIMB_STAND = 131,
    ANIM_ID_CLIMB_STAND_FINISH = 132,
    ANIM_ID_CLIMB_JUMP_B = 133,
    ANIM_ID_CLIMB_JUMP2FALL = 134,
    ANIM_ID_XPRESSSCRATCH = 135,
    ANIM_ID_TURN_180 = 136,
    ANIM_ID_TURN_L = 137,
    ANIM_ID_TURN_R = 138,
    ANIM_ID_ARRESTGUN = 139,
    ANIM_ID_DROWN = 140,
    ANIM_ID_DUCK_COWER = 141,
    ANIM_ID_HANDSUP = 142,
    ANIM_ID_HANDSCOWER = 143,
    ANIM_ID_FUCKU = 144,
    ANIM_ID_PHONE_IN = 145,
    ANIM_ID_PHONE_OUT = 146,
    ANIM_ID_PHONE_TALK = 147,
    ANIM_ID_SEAT_DOWN = 148,
    ANIM_ID_SEAT_UP = 149,
    ANIM_ID_SEAT_IDLE = 150,
    ANIM_ID_ATM = 151,
    ANIM_ID_ABSEIL = 152,
    ANIM_ID_WALK_DOORPARTIAL = 153,
    ANIM_ID_FACSURP = 154,
    ANIM_ID_FACSURPM = 155,
    ANIM_ID_FACURIOS = 156,
    ANIM_ID_FACANGER_0 = 157,
    ANIM_ID_FACANGER_1 = 158,
    ANIM_ID_FACANGER_2 = 159,
    ANIM_ID_FACTALK = 160,
    ANIM_ID_FACGUM = 161,
    ANIM_ID_TAP_HAND = 162,
    ANIM_ID_TAP_HANDP = 163,
    ANIM_ID_SHOVE_PARTIAL = 164,
    ANIM_ID_FLEE_LKAROUND_01 = 165,
    ANIM_ID_ENDCHAT_01 = 166,
    ANIM_ID_ENDCHAT_02 = 167,
    ANIM_ID_ENDCHAT_03 = 168,
    ANIM_ID_SMOKE_IN_CAR = 169,
    ANIM_ID_PASS_SMOKE_IN_CAR = 170,
    ANIM_ID_DAM_ARML_FRMBK = 171,
    ANIM_ID_DAM_ARML_FRMFT = 172,
    ANIM_ID_DAM_ARML_FRMLT = 173,
    ANIM_ID_DAM_ARMR_FRMBK = 174,
    ANIM_ID_DAM_ARMR_FRMFT = 175,
    ANIM_ID_DAM_ARMR_FRMRT = 176,
    ANIM_ID_DAM_LEGL_FRMBK = 177,
    ANIM_ID_DAM_LEGL_FRMFT = 178,
    ANIM_ID_DAM_LEGL_FRMLT = 179,
    ANIM_ID_DAM_LEGR_FRMBK = 180,
    ANIM_ID_DAM_LEGR_FRMFT = 181,
    ANIM_ID_DAM_LEGR_FRMRT = 182,
    ANIM_ID_DAM_STOMACH_FRMBK = 183,
    ANIM_ID_DAM_STOMACH_FRMFT = 184,
    ANIM_ID_DAM_STOMACH_FRMLT = 185,
    ANIM_ID_DAM_STOMACH_FRMRT = 186,
    ANIM_ID_CAR_DEAD_LHS = 187,
    ANIM_ID_CAR_DEAD_RHS = 188,
    ANIM_ID_CAR_TUNE_RADIO = 189,
    ANIM_ID_GANG_GUNSTAND = 190,
    ANIM_ID_NO_ANIMATION_SET = 191, // verified: src_prev_export/_types.h (for CWeapon/CPed)
    ANIM_ID_DOOR_LHINGE_O = 192,   // verified: src_prev_export/_types.h (for CWeapon)
    ANIM_ID_RELOAD = 226,          // verified: src_prev_export/_types.h (for CWeapon)
    ANIM_ID_PRTIAL_GNGTLKA = 279,
    ANIM_ID_PRTIAL_GNGTLKB = 280,
    ANIM_ID_PRTIAL_GNGTLKC = 281,
    ANIM_ID_PRTIAL_GNGTLKD = 282,
    ANIM_ID_PRTIAL_GNGTLKE = 283,
    ANIM_ID_PRTIAL_GNGTLKF = 284,
    ANIM_ID_PRTIAL_GNGTLKG = 285,
    ANIM_ID_PRTIAL_GNGTLKH = 286,
};

// ---------------------------------------------------------------------------
// Enums/eAnimBlendCallbackType.h (copied in full - 3 entries).
// ---------------------------------------------------------------------------
enum eAnimBlendCallbackType : uint32_t {
    ANIM_BLEND_CALLBACK_NONE = 0,
    ANIM_BLEND_CALLBACK_FINISH = 1,
    ANIM_BLEND_CALLBACK_DELETE = 2
};

// ---------------------------------------------------------------------------
// Enums/eBoneTag.h (copied in full).
// ---------------------------------------------------------------------------
enum eBoneTag : int16_t {
    BONE_UNKNOWN = -1,

    BONE_ROOT = 0, // Normal or Root, both are same
    BONE_PELVIS = 1,
    BONE_SPINE = 2,
    BONE_SPINE1 = 3,
    BONE_NECK = 4,
    BONE_HEAD = 5,
    BONE_L_BROW = 6,
    BONE_R_BROW = 7,
    BONE_JAW = 8,

    BONE_R_CLAVICLE = 21,
    BONE_R_UPPER_ARM = 22,
    BONE_R_FORE_ARM = 23,
    BONE_R_HAND = 24,
    BONE_R_FINGER = 25,
    BONE_R_FINGER_01 = 26,

    BONE_L_CLAVICLE = 31,
    BONE_L_UPPER_ARM = 32,
    BONE_L_FORE_ARM = 33,
    BONE_L_HAND = 34,
    BONE_L_FINGER = 35,
    BONE_L_FINGER_01 = 36,

    BONE_L_THIGH = 41,
    BONE_L_CALF = 42,
    BONE_L_FOOT = 43,
    BONE_L_TOE_0 = 44,

    BONE_R_THIGH = 51,
    BONE_R_CALF = 52,
    BONE_R_FOOT = 53,
    BONE_R_TOE_0 = 54,

    BONE_BELLY = 201,

    BONE_L_BREAST = 302,
    BONE_R_BREAST = 301,

    BONE_MAX_ID = 303,
    MAX_BONE_NUM = 32
};

using eBoneTag16 = notsa::WEnumS16<eBoneTag>;
using eBoneTag32 = notsa::WEnumS32<eBoneTag>;
using eBoneTagU32 = notsa::WEnumU32<eBoneTag>;

#include "CQuaternion.h" // canonical (deduped 2026-10-09; was inlined here)

// ---------------------------------------------------------------------------
// Trivial lerp used by CAnimBlendNode (gta-reversed extensions).
// ---------------------------------------------------------------------------
template <typename T>
constexpr T lerp(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

// ---------------------------------------------------------------------------
// Animation/AnimSequenceFrames.h - key-frame structs.
// Compressed variants use the C++17 FixedFloat/FixedVector from ColTypes.h
// (2-param versions; gta-reversed's 3rd bool template param dropped -
// see the FixedFloat note in ColTypes.h). FixedQuat has no ColTypes
// equivalent yet: minimal int16[4] stand-in with the 1/4096 scale applied
// on conversion (matches FixedQuat<int16, 4096.f> semantics).
// ---------------------------------------------------------------------------
struct CompressedQuat { // stand-in for FixedQuat<int16, 4096.f>
    int16_t x{}, y{}, z{}, w{};

    constexpr operator CQuaternion() const {
        return CQuaternion{
            static_cast<float>(x) / 4096.f,
            static_cast<float>(y) / 4096.f,
            static_cast<float>(z) / 4096.f,
            static_cast<float>(w) / 4096.f
        };
    }

    // FixedQuat<int16, 4096.f> semantics: assigning an uncompressed quaternion
    // compresses it. Needed by CAnimManager::LoadAnimFile_ANPK.
    constexpr CompressedQuat& operator=(const CQuaternion& q) {
        x = static_cast<int16_t>(q.x * 4096.f);
        y = static_cast<int16_t>(q.y * 4096.f);
        z = static_cast<int16_t>(q.z * 4096.f);
        w = static_cast<int16_t>(q.w * 4096.f);
        return *this;
    }
};

struct KeyFrame {
    CQuaternion Rot;
    float       DeltaTime; //< Relative to previous key frame
};

struct KeyFrameTrans : KeyFrame {
    CVector Trans;
};

struct KeyFrameCompressed {
    CompressedQuat          Rot;
    FixedFloat<int16_t, 60> DeltaTime;
};

struct KeyFrameTransCompressed : KeyFrameCompressed {
    FixedVector<int16_t, 1024> Trans;
};

// ---------------------------------------------------------------------------
// Animation/AnimationStyleDescriptor.h (0x30)
// ---------------------------------------------------------------------------
struct AnimDescriptor {
    AnimationId AnimId{};
    int32_t     Flags{};
};

struct AnimAssocDefinition {
    constexpr static size_t ANIM_NAME_BUF_SZ = 24;

    char   GroupName[16]{};
    char   BlockName[16]{};
    int32_t ModelIndex{};
    int32_t NumAnims{};

    const char**    AnimNames{}; //< Pointers to heap allocated char arrays (24 each) - the array of pointers itself is heap allocated too
    AnimDescriptor* AnimDescr{};
};

// ---------------------------------------------------------------------------
// Animation/AnimBlock.h (0x20)
// ---------------------------------------------------------------------------
class CAnimBlock {
public:
    char         Name[16];
    bool         IsLoaded;
    int16_t      RefCnt;
    int32_t      FirstAnimIdx;
    uint32_t     NumAnims;
    AssocGroupId GroupId;
};

// ---------------------------------------------------------------------------
// Animation/AnimBlendFrameData.h (0x18)
// ---------------------------------------------------------------------------
struct AnimBlendFrameData {
    union {
        struct {
            bool bf1 : 1;                            //!< doesn't seem to be used
            bool KeyFramesIgnoreNodeOrientation : 1; //!< Whenever orientation
            bool KeyFramesIgnoreNodeTranslation : 1; //!< Whenever translation
            bool HasVelocity : 1;                     //!< If true the translation is used to move the ped
            bool HasZVelocity : 1;                    //!< If true 3D velocity extraction is used, otherwise 2D
            bool NeedsKeyFrameUpdate : 1;             //!< If `RpAnimBlendNodeUpdateKeyFrames` needs to be called on update
            bool IsCompressed : 1;                   //!< Is the anim data for this frame compressed
            bool IsUpdatingFrame : 1;                //!< Doesn't seem to be used
        };
        uint8_t Flags;
    };

    union {
        CVector BonePos;  // For skinned clumps (?)
        CVector FramePos; // For non-skinned clumps (?)
    };

    union {
        RpHAnimBlendInterpFrame* KeyFrame; // For skinned clumps
        RwFrame*                 Frame;    // For non-skinned clumps
    };

    uint32_t BoneTag; // If `BONE_UNKNOWN` (-1) this is a non-skinned clump, otherwise a skinned one
};

// ---------------------------------------------------------------------------
// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
// ---------------------------------------------------------------------------
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(AnimAssocDefinition) == 0x30, "AnimAssocDefinition layout drift");
static_assert(sizeof(CAnimBlock) == 0x20, "CAnimBlock layout drift");
static_assert(sizeof(AnimBlendFrameData) == 0x18, "AnimBlendFrameData layout drift");
static_assert(sizeof(KeyFrame) == 0x14, "KeyFrame layout drift");
static_assert(sizeof(KeyFrameTrans) == 0x20, "KeyFrameTrans layout drift");
static_assert(sizeof(KeyFrameCompressed) == 0xA, "KeyFrameCompressed layout drift");
static_assert(sizeof(KeyFrameTransCompressed) == 0x10, "KeyFrameTransCompressed layout drift");
static_assert(sizeof(CQuaternion) == 0x10, "CQuaternion layout drift");
static_assert(sizeof(CompressedQuat) == 0x8, "CompressedQuat layout drift");
#endif
