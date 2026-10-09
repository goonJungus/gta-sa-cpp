// CTheScripts - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Scripts/TheScripts.h
// Script VM manager: script space, running-script lists, script "things"
// (spheres, checkpoints, searchlights, ...), intro text/rectangles, building
// swaps, mission cleanup, streamed scripts.
//
// Adaptations: stripped plugin-sdk header block, InjectHooks(), StaticRef
// globals (now plain static members; original 1.0 US addresses kept as comments
// and re-resolved in CTheScripts.cpp for the clean-room build), VALIDATE_SIZE ->
// guarded static_assert. The std::span views over ScriptSpace (C++20) are
// replaced with plain pointer accessors — restore spans when the project moves
// past C++17. The NOTSA GetSCMChunk<> template needs SCMChunks.hpp and is
// omitted until that header is ported. notsa::EntityRef is a minimal local
// stand-in (plain pointer wrapper, 0x4) until the entity system ports the real
// validity-checked handle. eFontStyleS32/FONT_SUBTITLES and SCREEN_WIDTH/HEIGHT
// are local placeholders (TODO: port with the font/graphics-core subsystems).
// CText/TheText (needed by GetTextByKeyFromScript) is not ported yet; that
// helper is declared here and stubbed in CTheScripts.cpp.

#pragma once

#include "CVector.h"     // CVector, CVector2D
#include "CRect.h"       // CRect
#include "RenderTypes.h" // CRGBA, GxtChar
#include "CSprite2d.h"   // CSprite2d
#include "CBuilding.h"   // CBuilding
#include "ScriptParam.h" // tScriptParam

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

// ---- Forward declarations (ported in later subsystems) ----
class CRunningScript;
class CCheckpoint;
enum class eCheckpointType : uint32_t;
class CPed;
class CVehicle;
class CObject;
class CEntity;
class FxSystem_c;
class CStreamedScripts;
class CScriptResourceManager;
class CUpsideDownCarCheck;
class CStuckCarCheck;
// Minimal CScriptsForBrains stand-in (added 2026-10-09 for CPed).
// TODO(port): real layout from the scripts batch; byte array so the decomp's
//   `m_aScriptForBrains + id * 0x14` addressing compiles unchanged.
struct CScriptsForBrains {
    uint8_t m_aScriptForBrains[0x14 * 16]{};
};
class CMissionCleanup;
struct RwStream;
// eModelID: canonical minimal enum in eModelID.h (deduped 2026-10-09).
#include "eModelID.h"

// Font alignment enum not yet ported; opaque declaration is a complete type
// (fixed underlying type) so it can be used as a member below.
enum class eFontAlignment : int32_t; // TODO: port with the font subsystem

// Minimal stand-in for gta-reversed's notsa::EntityRef (ported with the entity
// system). The original is a validity-checked entity handle; this keeps the
// 0x4 layout so the structs below compile.
// TODO: replace with the real EntityRef and verify sizeof(notsa::EntityRef<>) == 0x4.
namespace notsa {
template<typename T = CEntity>
struct EntityRef {
    T* m_Entity{};

    EntityRef() = default;
    EntityRef(T* e) : m_Entity(e) {}

    EntityRef& operator=(T* e) { m_Entity = e; return *this; }
    operator T*() const { return m_Entity; }
    T* operator->() const { return m_Entity; }
    T& operator*() const { return *m_Entity; }
    explicit operator bool() const { return m_Entity != nullptr; }
};
} // namespace notsa

// TODO: move to a shared constants header (values from gta-reversed Common.h)
static constexpr float SCREEN_WIDTH = 640.0f;
static constexpr float SCREEN_HEIGHT = 448.0f;

enum class eCrossHairType : uint32_t {
    NONE,
    FIXED_DRAW_CIRCLE,
    FIXED_DRAW_1STPERSON_WEAPON,
};

enum eScriptThingType : uint8_t {
    SCRIPT_THING_SPHERE         = 0,
    SCRIPT_THING_EFFECT_SYSTEM  = 1,
    SCRIPT_THING_SEARCH_LIGHT   = 2,
    SCRIPT_THING_CHECKPOINT     = 3,
    SCRIPT_THING_SEQUENCE_TASK  = 4,
    SCRIPT_THING_FIRE           = 5,
    SCRIPT_THING_2D_EFFECT      = 6,
    SCRIPT_THING_DECISION_MAKER = 7,
    SCRIPT_THING_PED_GROUP      = 8
};

struct tBuildingSwap {
    CBuilding* m_pCBuilding;
    int32_t    m_nNewModelIndex;
    int32_t    m_nOldModelIndex;

    tBuildingSwap() { // 0x469270
        Clear();
    }

    tBuildingSwap(CBuilding* building, int32_t nNewModelIndex, int32_t nOldModelIndex) {
        m_pCBuilding     = building;
        m_nNewModelIndex = nNewModelIndex;
        m_nOldModelIndex = nOldModelIndex;
    }

    void Clear() { // todo: +-
        m_pCBuilding     = nullptr;
        m_nNewModelIndex = -1;
        m_nOldModelIndex = -1;
    }
};

struct tScriptSwitchCase {
    int32_t m_nSwitchValue;
    int32_t m_nSwitchLabelAddress;
};

struct tScriptCheckpoint {
    bool         m_bUsed;
    char         m_field_1;
    int16_t      m_nId;
    CCheckpoint* m_Checkpoint;

    tScriptCheckpoint() { // 0x469334
        m_bUsed      = false;
        m_nId        = 1;
        m_Checkpoint = nullptr;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};

struct tScriptEffectSystem {
    bool        m_bUsed;
    int16_t     m_nId;
    FxSystem_c* m_pFxSystem;

    tScriptEffectSystem() {
        m_bUsed     = false;
        m_nId       = 1;
        m_pFxSystem = nullptr;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};

struct tScriptSequence {
    bool    m_bUsed;
    int16_t m_nId;

    tScriptSequence() {
        m_bUsed = false;
        m_nId   = 1;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};

struct tScriptText {
    // Values from 0x4690A8
    CVector2D Scale{ 0.48f, 1.12f };
    CRGBA     Color{ 225, 225, 225, 255 };
    bool      Justify{ false };
    bool      IsCentered{ false }; //!< `HasRightJustify` takes precedence over it. If true, uses `eFontAlignment::ALIGN_CENTER` otherwise `eFontAlignment::ALIGN_LEFT`.
    bool      HasBg{ false };
    bool      HasBgTextOnly{ false }; //!< Unused
    float     WrapX{ SCREEN_HEIGHT };
    float     CentreSize{ SCREEN_WIDTH };
    CRGBA     BgColor{ 128, 128, 128, 128 };
    bool      IsProportional{ true };
    CRGBA     DropShadowColor{ 0, 0, 0, 255 };
    int8_t    DropShadow{ 2 };
    int8_t    TextEdge{ 0 };
    bool      IsDrawBeforeFade{ false };
    bool      HasRightJustify{ false }; //!< Takes priority over `IsCentered`. If true `eFontAlignment::ALIGN_RIGHT` is used.
    int32_t   FontStyle{ 1 }; //!< was eFontStyleS32{FONT_SUBTITLES}; TODO: port font enums
    CVector2D Pos{};
    char      GXTKey[8]{};
    int32_t   NumberToInsert1{ -1 };
    int32_t   NumberToInsert2{ -1 };
};

enum class eScriptRectangleType : int32_t {
    INACTIVE,           //!< This entry is inactive (Not drawn)
    TITLE_AND_MESSAGE,
    TEXT,
    MONOCOLOR,          //!< Mono-color rect
    TEXTURED,           //!< Textured rect (basically a sprite)
    MONOCOLOR_ANGLED,   //!< Angled mono-color rect
};

struct tScriptRectangle {
    eScriptRectangleType m_nType;
    bool                 m_bDrawBeforeFade;
    char                 field_5;
    int16_t              m_nTextureId;
    CVector2D            cornerA; //!< Supposed to be: Top left corner (min x, y) - Sometimes this isn't the case though, for example in scripted videogames...
    CVector2D            cornerB; //!< Supposed to be: Bottom right corner (max x, y) - Sometimes this isn't the case though, for example in scripted videogames...
    float                m_nAngle;
    CRGBA                m_nTransparentColor;
    char                 gxt1[8];
    int16_t              field_28;
    char                 gxt2[8];
    int16_t              field_32;
    eFontAlignment       m_Alignment;
    uint32_t             m_nTextboxStyle;

    tScriptRectangle() { // 0x4691C8
        m_nType             = eScriptRectangleType::INACTIVE;
        m_bDrawBeforeFade   = false;
        m_nTextureId        = -1;
        cornerA             = CVector2D();
        cornerB             = CVector2D();
        m_nAngle            = 0;
        m_nTransparentColor = CRGBA{ 255, 255, 255, 255 };
        gxt1[0]             = '\0';
        m_nTextboxStyle     = 3;
    }
};

struct tScriptAttachedAnimGroup {
    int32_t m_nModelID;
    char    m_IfpName[16];

    tScriptAttachedAnimGroup() {
        m_nModelID = -1;
        m_IfpName[0] = 0;
    }
};

enum class eScriptSearchLightState : uint8_t {
    STATE_0,
    STATE_1,
    STATE_2,
    STATE_3,
    STATE_4,
};

struct tScriptSearchlight {
    bool                    m_bUsed{};
    bool                    m_bClipIfColliding{};
    bool                    m_bEnableShadow{};
    eScriptSearchLightState m_nCurrentState : 7; // (C++17: no NSDMI on bit-fields; zero-initialized via value-init)
    bool                    m_SomethingFlag : 1;   // (same as above)
    int16_t                 m_nId{};
    CVector                 m_Origin{};
    CVector                 m_Target{};
    float                   m_fTargetRadius{};
    float                   m_fBaseRadius{};
    CVector                 m_PathCoord1{};
    CVector                 m_PathCoord2{};
    float                   m_fPathSpeed{};
    notsa::EntityRef<>      m_AttachedEntity{};
    notsa::EntityRef<>      m_FollowingEntity{};
    notsa::EntityRef<>      m_Tower{};
    notsa::EntityRef<>      m_Housing{};
    notsa::EntityRef<>      m_Bulb{};
    CVector                 m_TargetSpot{};
    CVector                 vf64{};
    CVector                 vf70{};

    //! Script thing ID
    auto GetId() { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};

struct tUsedObject {
    char    szModelName[24];
    int32_t nModelIndex;

    tUsedObject() = default; // 0x468F20
};

struct tScriptSphere {
    bool     m_bUsed;
    char     m_f1;
    int16_t  m_nUniqueId;
    uint32_t m_nId;
    CVector  m_vCoords;
    float    m_fRadius;

    tScriptSphere() { // 0x469060
        m_vCoords   = CVector();
        m_bUsed     = false;
        m_nUniqueId = 1;
        m_nId       = 0;
        m_fRadius   = 0.0f;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nUniqueId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};

struct tStoredLine {
    CVector  vecInf;
    CVector  vecSup;
    uint32_t color1;
    uint32_t color2;
};

struct tScriptBrainWaitEntity {
    notsa::EntityRef<> m_pEntity{};
    int16_t            m_ScriptBrainIndex{ -1 };
    int16_t            field_6{};

    tScriptBrainWaitEntity() = default; // 0x468E12

    // NOTSA
    void Clear() {
        m_pEntity = nullptr;
        m_ScriptBrainIndex = -1;
    }
};

struct tScriptConnectLodsObject {
    int32_t a;
    int32_t b;

    tScriptConnectLodsObject() {
        a = -1;
        b = -1;
    }
};
// NOTE: the original header has VALIDATE_SIZE(tScriptBrainWaitEntity, 0x8) here
// (a copy-paste bug); the assert below targets the right struct.

enum {
    MAX_NUM_SCRIPTS                             = 96,
    MAX_NUM_SCRIPT_SPRITES                      = 128,
    MAX_NUM_SCRIPT_SPHERES                      = 16,
    MAX_NUM_SCRIPT_RECTANGLES                   = 128,
    MAX_NUM_SCRIPT_SEARCH_LIGHT                 = 8,
    MAX_NUM_SCRIPT_SEQUENCE_TASKS               = 64,
    MAX_NUM_SCRIPT_CHECKPOINTS                  = 20,
    MAX_NUM_SCRIPT_EFFECT_SYSTEMS               = 32,
    MAX_NUM_SCRIPT_CONNECT_LODS_OBJECTS         = 10,
    MAX_NUM_SCRIPT_ATTACHED_ANIM_GROUPS         = 8,
    MAX_NUM_ENTITIES_WAITING_FOR_SCRIPT_BRAIN   = 150,
    MAX_NUM_VEHICLE_MODELS_BLOCKED_BY_SCRIPT    = 20,
    MAX_NUM_MISSION_SCRIPTS                     = 200,
    MAX_NUM_LOCAL_VARIABLES_FOR_CURRENT_MISSION = 1024,
    MAX_NUM_USED_OBJECTS                        = 395,
    MAX_NUM_BUILDING_SWAPS                      = 25,
    MAX_NUM_INVISIBILITY_SETTINGS               = 20,
    MAX_NUM_INTRO_TEXT_LINES                    = 96,
    MAX_NUM_STORED_LINES                        = 1024,
    MAX_NUM_SwitchJumpTable                     = 75,
    MAX_NUM_CARDS                               = 312,
    MAX_NUM_SUPPRESSED_VEHICLE_MODELS           = 40,
};

enum class ScriptSavedObjectType : uint32_t {
    NONE = 0,
    INVISIBLE = 1, // ?
    BUILDING = 2,
    OBJECT = 3,
    DUMMY = 4,
};

static constexpr uint32_t SCRIPT_VAR_TIMERA = 32, SCRIPT_VAR_TIMERB = 33;
static constexpr uint32_t MISSION_SCRIPT_SIZE = 69000;

static inline bool gAllowScriptedFixedCameraCollision = false;

class CTheScripts {
public:
    static constexpr uint32_t MAIN_SCRIPT_SIZE         = 200'000;
    static constexpr uint32_t SCRIPT_SPACE_SIZE        = MAIN_SCRIPT_SIZE + MISSION_SCRIPT_SIZE;
    static constexpr uint32_t MAX_SAVED_GVAR_PART_SIZE = 51'200;

    //! Lower `MAIN_SCRIPT_SIZE` is where MAIN.SCM is, remaining `MISSION_SCRIPT_SIZE` is for other loaded scripts.
    static uint8_t ScriptSpace[SCRIPT_SPACE_SIZE]; // 0xA49960; definition in CTheScripts.cpp

    //! Reference to ScriptSpace's lower portion for MAIN.SCM - Prefer this over `&ScriptSpace[0]`
    // TODO: std::span is C++20; restore span views when the project moves past C++17.
    static uint8_t* MainSCMBlock() { return ScriptSpace; }

    //! Reference to ScriptSpace's upper portion for other scripts - Prefer this over `&ScriptSpace[MAIN_SCRIPT_SIZE]`
    static uint8_t* MissionBlock() { return ScriptSpace + MAIN_SCRIPT_SIZE; }

    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CTheScripts.cpp.
    // TODO: re-resolve for the clean-room build.
    static std::array<tScriptSwitchCase, MAX_NUM_SwitchJumpTable> SwitchJumpTable; // 0xA43CF8
    static uint16_t NumberOfEntriesInSwitchTable;        // 0xA43F50
    static uint16_t NumberOfEntriesStillToReadForSwitch; // 0xA43F60

    static std::array<int16_t, MAX_NUM_CARDS> CardStack; // 0xA44218
    static std::array<int32_t, MAX_NUM_MISSION_SCRIPTS> MultiScriptArray; // 0xA444C8
    static std::array<tScriptConnectLodsObject, MAX_NUM_SCRIPT_CONNECT_LODS_OBJECTS> ScriptConnectLodsObjects; // 0xA44800
    static std::array<tScriptAttachedAnimGroup, MAX_NUM_SCRIPT_ATTACHED_ANIM_GROUPS> ScriptAttachedAnimGroups; // 0xA44850
    static std::array<eModelID, MAX_NUM_VEHICLE_MODELS_BLOCKED_BY_SCRIPT> VehicleModelsBlockedByScript; // 0xA448F0
    static std::array<eModelID, MAX_NUM_SUPPRESSED_VEHICLE_MODELS> SuppressedVehicleModels; // 0xA44940
    static std::array<CEntity*, MAX_NUM_INVISIBILITY_SETTINGS> InvisibilitySettingArray; // 0xA449E0

    static std::array<tScriptParam, MAX_NUM_LOCAL_VARIABLES_FOR_CURRENT_MISSION> LocalVariablesForCurrentMission; // 0xA48960
    static uint32_t LargestNumberOfMissionScriptLocalVariables; // 0xA444B4

    static std::array<tBuildingSwap, MAX_NUM_BUILDING_SWAPS> BuildingSwapArray; // 0xA44A30

    static std::array<tUsedObject, MAX_NUM_USED_OBJECTS> UsedObjectArray; // 0xA44B70
    static uint16_t NumberOfUsedObjects; // 0xA44B6C

    static std::array<tScriptBrainWaitEntity, MAX_NUM_ENTITIES_WAITING_FOR_SCRIPT_BRAIN> EntitiesWaitingForScriptBrain; // 0xA476B0
    static std::array<tScriptText, MAX_NUM_INTRO_TEXT_LINES> IntroTextLines; // 0xA913E8
    static uint16_t NumberOfIntroTextLinesThisFrame; // 0xA44B68

    static std::array<tScriptRectangle, MAX_NUM_SCRIPT_RECTANGLES> IntroRectangles; // 0xA92D68
    static uint16_t NumberOfIntroRectanglesThisFrame; // 0xA44B5C

    static std::array<CSprite2d, MAX_NUM_SCRIPT_SPRITES> ScriptSprites; // 0xA94B68

    static uint16_t NumberOfExclusiveMissionScripts; // 0xA444B8
    static uint16_t NumberOfMissionScripts;          // 0xA444BC

    //
    // Script things
    //

    static std::array<tScriptSphere, MAX_NUM_SCRIPT_SPHERES> ScriptSphereArray; // 0xA91268
    static std::array<tScriptEffectSystem, MAX_NUM_SCRIPT_EFFECT_SYSTEMS> ScriptEffectSystemArray; // 0xA44110
    static std::array<tScriptSearchlight, MAX_NUM_SCRIPT_SEARCH_LIGHT> ScriptSearchLightArray; // 0xA94D68

    static std::array<tScriptSequence, MAX_NUM_SCRIPT_SEQUENCE_TASKS> ScriptSequenceTaskArray; // 0xA43F68
    static uint16_t NumberOfScriptSearchLights; // 0xA90830

    static std::array<tScriptCheckpoint, MAX_NUM_SCRIPT_CHECKPOINTS> ScriptCheckpointArray; // 0xA44070
    static uint16_t NumberOfScriptCheckpoints; // 0xA44068
    static uint32_t UnknownDebugStuff;         // 0xA95190; used in DO_DEBUG_STUFF command

    enum class eUseTextCommandState : uint8_t {
        DISABLED,
        DISABLE_NEXT_FRAME,
        ENABLED_BY_SCRIPT
    };
    static eUseTextCommandState UseTextCommands; // 0xA44B67
    static bool     DbgFlag;                     // 0x859CF8
    static int32_t  SwitchDefaultAddress;        // 0xA43F54
    static bool     SwitchDefaultExists;         // 0xA43F58
    static int32_t  ValueToCheckInSwitchStatement; // 0xA43F5C
    static int16_t  CardStackPosition;           // 0xA44210
    static bool     bDrawSubtitlesBeforeFade;    // 0xA44488
    static bool     bDrawOddJobTitleBeforeFade;  // 0xA44489
    static bool     bScriptHasFadedOut;          // 0xA4448A
    static bool     bAddNextMessageToPreviousBriefs; // 0xA4448B
    static int32_t  ForceRandomCarModel;         // 0xA4448C
    static eCrossHairType bDrawCrossHair;        // 0xA44490
    static bool     bEnableCraneRelease;         // 0xA44494
    static bool     bEnableCraneLower;           // 0xA44495
    static bool     bEnableCraneRaise;           // 0xA44496
    static float    fCameraHeadingStepWhenPlayerIsAttached; // 0xA44498
    static float    fCameraHeadingWhenPlayerIsAttached;     // 0xA4449C
    static bool     bDisplayHud;                 // 0xA444A0
    static bool     HideAllFrontEndMapBlips;     // 0xA444A1
    static bool     RadarShowBlipOnAllLevels;    // 0xA444A2
    static uint8_t  RadarZoomValue;              // 0xA444A3
    static bool     bPlayerIsOffTheMap;          // 0xA444A4
    static char     RiotIntensity;               // 0xA444A5
    static bool     bPlayerHasMetDebbieHarry;    // 0xA444A6
    static bool     bDisplayNonMiniGameHelpMessages; // 0xA444A7
    static bool     bMiniGameInProgress;         // 0xA444A8
    static int32_t  ScriptPickupCycleIndex;      // 0xA444AC
    static int8_t   FailCurrentMission;          // 0xA444B0
    static bool     bAlreadyRunningAMissionScript; // 0xA444B1
    static uint32_t LargestMissionScriptSize;    // 0xA444C0
    static uint32_t MainScriptSize;              // 0xA444C4
    static bool     bUsingAMultiScriptFile;      // 0xA447E8
    static int32_t  StoreVehicleIndex;           // 0xA447EC
    static bool     StoreVehicleWasRandom;       // 0xA447F0
    static uint16_t CommandsExecuted;            // 0xA447F4
    static uint16_t ScriptsUpdated;              // 0xA447F8
    static uint16_t MessageWidth;                // 0xA44B60
    static uint16_t MessageCentre;               // 0xA44B64
    static bool     bUseMessageFormatting;       // 0xA44B66
    static int32_t  LastRandomPedId;             // 0xA476A4
    static uint32_t LastMissionPassedTime;       // 0xA476A8
    static int32_t  OnAMissionFlag;              // 0xA476AC; Refers to the offset of OM flag in script space.

    // Declared only: these classes are not ported yet (incomplete types), so
    // their static storage is defined when their subsystem is ported.
    // TODO: define in CTheScripts.cpp alongside the class ports.
    static CStreamedScripts      StreamedScripts;      // 0xA47B60
    static CScriptResourceManager ScriptResourceManager; // 0xA485A8
    static CUpsideDownCarCheck   UpsideDownCars;       // 0xA4892C
    static CRunningScript*       pIdleScripts;         // 0xA8B428
    static CRunningScript*       pActiveScripts;       // 0xA8B42C
    static CMissionCleanup       MissionCleanUp;       // 0xA90850
    static CStuckCarCheck        StuckCars;            // 0xA90AB0
    static CScriptsForBrains     ScriptsForBrains;     // 0xA90CF0

public:
    static void Init();
    static void InitialiseAllConnectLodObjects();
    static void InitialiseConnectLodObjects(uint16_t index);
    static void InitialiseSpecialAnimGroup(uint16_t index);
    static void InitialiseSpecialAnimGroupsAttachedToCharModels();
    static void ReadObjectNamesFromScript();
    static void UpdateObjectIndices();
    static void ReadMultiScriptFileOffsetsFromScript();

    static uint32_t AddScriptCheckpoint(CVector at, CVector pointTo, float radius, eCheckpointType type);
    static uint32_t AddScriptEffectSystem(FxSystem_c* system);
    static uint32_t AddScriptSearchLight(CVector start, CEntity* entity, CVector target, float targetRadius, float baseRadius);
    static uint32_t AddScriptSphere(uint32_t id, CVector posn, float radius);

    static void AddToBuildingSwapArray(CBuilding* building, int32_t oldModelId, int32_t newModelId);
    static void AddToInvisibilitySwapArray(CEntity* entity, bool bVisible);
    static void AddToListOfConnectedLodObjects(CObject* obj1, CObject* obj2);
    static void AddToListOfSpecialAnimGroupsAttachedToCharModels(int32_t modelId, const char* ifpName); // original had a 'Const' typo
    static void AddToSwitchJumpTable(int32_t switchValue, int32_t switchLabelLocalAddress);
    static void AddToVehicleModelsBlockedByScript(eModelID modelIndex);
    static void AddToWaitingForScriptBrainArray(CEntity* entity, int16_t arg2);

    static void AttachSearchlightToSearchlightObject(int32_t searchLightId, CObject* tower, CObject* housing, CObject* bulb, CVector offset);
    static bool CheckStreamedScriptVersion(RwStream* stream, const char* filename);
    static void CleanUpThisObject(CObject* obj);
    static void CleanUpThisPed(CPed* ped);
    static void CleanUpThisVehicle(CVehicle* vehicle);
    static void ClearAllSuppressedCarModels();
    static void ClearAllVehicleModelsBlockedByScript();
    static void ClearSpaceForMissionEntity(const CVector& pos, CEntity* entity);
    static void DoScriptSetupAfterPoolsHaveLoaded();

    static int32_t GetActualScriptThingIndex(int32_t ref, eScriptThingType type);
    static int32_t GetNewUniqueScriptThingIndex(int32_t index, eScriptThingType type);
    static int32_t GetScriptIndexFromPointer(CRunningScript* thread);
    static int32_t GetUniqueScriptThingIndex(int32_t playerGroup, eScriptThingType type);

    static bool IsEntityWithinAnySearchLight(CEntity* entity, int32_t* pIndex);
    static bool IsEntityWithinSearchLight(uint32_t index, CEntity* entity);
    static bool IsPedStopped(CPed* ped);
    static bool IsPlayerOnAMission();
    static bool IsPointWithinSearchLight(const CVector& pointPosn, int32_t index);
    static bool IsVehicleStopped(CVehicle* veh);

    static void Load();
    static void Save();

    static void MoveSearchLightBetweenTwoPoints(int32_t index, float x1, float y1, float z1, float x2, float y2, float z2, float pathSpeed);
    static void MoveSearchLightToEntity(int32_t index, CEntity* entity, float pathSpeed);
    static void MoveSearchLightToPointAndStop(int32_t index, float x, float y, float z, float pathSpeed);

    static void Process();
    static void ProcessAllSearchLights();
    static void ProcessWaitingForScriptBrainArray();

    static void ReinitialiseSwitchStatementData();

    static void RemoveFromVehicleModelsBlockedByScript(int32_t modelIndex);
    static void RemoveFromWaitingForScriptBrainArray(CEntity* entity, int16_t modelIndex);
    static void RemoveScriptCheckpoint(int32_t scriptIndex);
    static void RemoveScriptEffectSystem(int32_t scriptIndex);
    static void RemoveScriptSearchLight(int32_t scriptIndex);
    static void RemoveScriptSphere(int32_t scriptIndex);
    static void RemoveScriptTextureDictionary();
    static void RemoveThisPed(CPed* ped);

    static void RenderAllSearchLights();
    static bool ScriptAttachAnimGroupToCharModel(int32_t modelId, const char* ifpName);
    static void ScriptConnectLodsFunction(int32_t objectHandle1, int32_t objectHandle2);
    static void ScriptDebugCircle2D(float x, float y, float width, float height, CRGBA color);
    static CRunningScript* StartNewScript(uint8_t* startIP);
    static CRunningScript* StartNewScript(uint8_t* startIP, uint16_t index);
    static void StartTestScript();
    static void UndoBuildingSwaps();
    static void UndoEntityInvisibilitySettings();
    static void UseSwitchJumpTable(int32_t& switchLabelAddress);
    static void WipeLocalVariableMemoryForMissionScript();

    static bool HasCarModelBeenSuppressed(eModelID carModelId);
    static bool HasVehicleModelBeenBlockedByScript(eModelID carModelId);

    // DEBUG
    static void ScriptDebugLine3D(const CVector& start, const CVector& end, uint32_t color1, uint32_t color2);
    static void RenderTheScriptDebugLines();

    static void PrintListSizes();

    static void DrawScriptSpheres();
    static void HighlightImportantArea(uint32_t markerId, float fromX, float fromY, float toX, float toY, float height);
    static void HighlightImportantArea(uint32_t markerId, const CRect& area, float height) { HighlightImportantArea(markerId, area.left, area.top, area.right, area.bottom, height); } // NOTSA
    static void HighlightImportantAngledArea(uint32_t markerId, float fromX, float fromY, float toX, float toY, float angledToX, float angledToY, float angledFromX, float angledFromY, float height);
    static void DrawDebugSquare(float, float, float, float);
    static void DrawDebugSquare(const CRect& area) { DrawDebugSquare(area.left, area.top, area.right, area.bottom); }
    static void DrawDebugAngledSquare(const CVector2D& inf, const CVector2D& sup, const CVector2D& rotSup, const CVector2D& rotInf);
    static void DrawDebugCube(const CVector& inf, const CVector& sup);
    static void DrawDebugAngledCube(const CVector& inf, const CVector& sup, const CVector2D& rotSup, const CVector2D& rotInf);
    static void DrawScriptSpritesAndRectangles(bool drawBeforeFade);

    // NOTSA GetSCMChunk<> template omitted: needs SCMChunks.hpp (ported with a
    // later subsystem). Original walks the SCM header chunks to find a chunk
    // by type; see gta-reversed Scripts/TheScripts.h.

    static int32_t* GetPointerToScriptVariable(uint32_t offset) {
        // TODO: find out how this method changed between re3 and GTA:SA
        assert(offset >= 8 && offset < GetSizeOfVariableSpace());
        return (int32_t*)&ScriptSpace[offset];
    }

    static int8_t Read1ByteFromScript(uint8_t*& ip) {
        int8_t retval = *reinterpret_cast<int8_t*>(ip);
        ip += 1;
        return retval;
    }

    static int16_t Read2BytesFromScript(uint8_t*& ip) {
        int16_t retval = *reinterpret_cast<int16_t*>(ip);
        ip += 2;
        return retval;
    }

    static int32_t Read4BytesFromScript(uint8_t*& ip) {
        int32_t retval = *reinterpret_cast<int32_t*>(ip); // big-endian unfriendly/unaligned mem access
        ip += 4;
        return retval;
    }

    static float ReadFloatFromScript(uint8_t*& ip) {
        int32_t retval = Read4BytesFromScript(ip);
        return *reinterpret_cast<float*>(&retval);
    }

#define KEY_LENGTH_IN_SCRIPT (8)

    static void ReadTextLabelFromScript(uint8_t*& ip, char* buf) {
        // strcpy_s/strncpy_s are MSVC-only; TODO: portability wrapper when the
        // project targets other compilers.
        strncpy_s(buf, KEY_LENGTH_IN_SCRIPT, (const char*)&ScriptSpace[*ip], KEY_LENGTH_IN_SCRIPT);
    }

    // TODO: needs CText/TheText from the text subsystem; body stubbed in CTheScripts.cpp.
    static GxtChar* GetTextByKeyFromScript(uint8_t*& ip);

    static uint32_t GetSizeOfVariableSpace() {
        uint8_t* tmp = MainSCMBlock() + 3;
        return Read4BytesFromScript(tmp);
    }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tScriptSwitchCase) == 0x8, "tScriptSwitchCase layout changed");
static_assert(sizeof(tScriptCheckpoint) == 0x8, "tScriptCheckpoint layout changed");
static_assert(sizeof(tScriptEffectSystem) == 0x8, "tScriptEffectSystem layout changed");
static_assert(sizeof(tScriptSequence) == 0x4, "tScriptSequence layout changed");
static_assert(sizeof(tScriptText) == 0x44, "tScriptText layout changed");
static_assert(sizeof(tScriptRectangle) == 0x3C, "tScriptRectangle layout changed");
static_assert(sizeof(tScriptSearchlight) == 0x7C, "tScriptSearchlight layout changed");
static_assert(sizeof(tUsedObject) == 0x1C, "tUsedObject layout changed");
static_assert(sizeof(tScriptSphere) == 0x18, "tScriptSphere layout changed");
static_assert(sizeof(tStoredLine) == 0x20, "tStoredLine layout changed");
static_assert(sizeof(tScriptBrainWaitEntity) == 0x8, "tScriptBrainWaitEntity layout changed");
static_assert(sizeof(tScriptConnectLodsObject) == 0x8, "tScriptConnectLodsObject layout changed");
#endif
