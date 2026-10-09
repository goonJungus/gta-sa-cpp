// CVehicleModelInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/VehicleModelInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), NOTSA_EXPORT_VTABLE,
//   VALIDATE_SIZE (now guarded static_asserts),
//   StaticRef -> plain static members / globals (defined in the .cpp; original
//   GTA SA 1.0 addresses kept as comments),
//   notsa::mdarray -> std::array,
//   GetModelDummyPosition's C++23 explicit-object-parameter (this auto&&) ->
//       const/non-const overload pair (C++17),
//   strcpy_s -> portable strncpy in SetGameName.
// Replaced includes:
//   "ClumpModelInfo.h" -> "CClumpModelInfo.h"
//   "RenderWare.h", "Vector.h", "Quaternion.h", "RwObjectNameIdAssocation.h",
//     "RGBA.h", "Pool.h" -> "CClumpModelInfo.h" (RwObjectNameIdAssocation),
//     "CVector.h", "CPool.h"; minimal CQuaternion/CRGBA below.
//   "eVehicleClass.h" -> in "CClumpModelInfo.h" (shared with CPedModelInfo.h)
//   "eVehicleType.h", "eCarWheel.h" -> canonical headers (values verified against
//       gta-reversed source/game_sa/Enums/*.h).
//   <extensions/utility.hpp> -> not needed after mdarray replacement.
// TODO: move each enum/struct to its own header when its subsystem is ported.

#pragma once

#include "CClumpModelInfo.h"
#include "CVector.h"
#include "eCarWheel.h" // canonical (was inlined below; values verified identical 2026-10-08)
#include "eVehicleType.h" // canonical (was inlined below; values verified identical 2026-10-09)
#include "CMatrix.h" // CQuaternion (used by UpgradePosnDesc)
#include "CPool.h"

#include <array>
#include <cstdint>
#include <cstring>

// ---- RenderWare forward declarations (clean-room renderer provides these later) ----
struct RwFrame;
struct RwTexture;
struct RwTexDictionary;
struct RpMaterial;

// Minimal RenderWare stand-ins (layout only).
// TODO: replace with the real RW types when the RW layer is ported.
// NOTE: CQuaternion comes from CMatrix.h (converted).
// CRGBA: canonical definition lives in RenderTypes.h (deduped 2026-10-09
// camera batch; was inlined here with an identical layout).
#include "RenderTypes.h"
struct RwSurfaceProperties {
    float ambient{}, specular{}, diffuse{};
};
// RwTextureCallBackFind: canonical typedef in RenderWare.h (moved 2026-10-09).
// Original: typedef RwTexture* (*RwTextureCallBackFind)(const RwChar* name);

class CAnimBlock; // forward declaration (animation subsystem)
class CVehicle;   // forward declaration (vehicle entity, not ported yet)
struct tHandlingData;       // forward declaration (handling.cfg data)
struct tFlyingHandlingData; // forward declaration (flying handling data)

// enum by forkerer (https://github.com/forkerer/)
enum eVehicleDummy {
    DUMMY_LIGHT_FRONT_MAIN      = 0,
    DUMMY_LIGHT_REAR_MAIN       = 1,

    DUMMY_LIGHT_FRONT_SECONDARY = 2,
    DUMMY_LIGHT_REAR_SECONDARY  = 3,

    DUMMY_SEAT_FRONT            = 4,
    DUMMY_SEAT_REAR             = 5,

    DUMMY_EXHAUST               = 6,
    DUMMY_ENGINE                = 7,
    DUMMY_GAS_CAP               = 8,
    DUMMY_TRAILER_ATTACH        = 9,
    DUMMY_HAND_REST             = 10,
    DUMMY_EXHAUST_SECONDARY     = 11,
    DUMMY_WING_AIR_TRAIL        = 12,
    DUMMY_VEHICLE_GUN           = 13,
};

enum eVehicleUpgradePosn {
    UPGRADE_BONNET,
    UPGRADE_BONNET_LEFT,
    UPGRADE_BONNET_RIGHT,
    UPGRADE_BONNET_DAM,
    UPGRADE_BONNET_LEFT_DAM,
    UPGRADE_BONNET_RIGHT_DAM,
    UPGRADE_SPOILER,
    UPGRADE_SPOILER_DAM,
    UPGRADE_WING_LEFT,
    UPGRADE_WING_RIGHT,
    UPGRADE_FRONTBULLBAR,
    UPGRADE_BACKBULLBAR,
    UPGRADE_LIGHTS,
    UPGRADE_LIGHTS_DAM,
    UPGRADE_ROOF,
    UPGRADE_NITRO,
};

enum class eCarColLineType : uint32_t {
    IGNORED = 0,
    GLOBAL_RGB = 1,
    CAR_2COL = 2,
    CAR_4COL = 3
};

enum class eCarModsLineType : uint32_t {
    IGNORED = 0,
    LINK = 1,
    MODS = 2,
    WHEEL = 3
};

enum eComponentsRules {
    ALLOW_ALWAYS = 1,
    ONLY_WHEN_RAINING = 2,
    MAYBE_HIDE = 3,
    FULL_RANDOM = 4,
};

// eVehicleType: see eVehicleType.h (canonical; moved 2026-10-09 camera batch -
// was inlined here and in CCamera.h with identical values).

// eCarWheel: see include/eCarWheel.h (moved 2026-10-08 vehicle batch).

struct tRestoreEntry {
    void* m_pAddress;
    void* m_pValue;
};

union tVehicleCompsUnion {
    uint32_t m_nComps;
    struct {
        uint32_t nExtraA_comp1 : 4;
        uint32_t nExtraA_comp2 : 4;
        uint32_t nExtraA_comp3 : 4;
        uint32_t : 4;

        uint32_t nExtraB_comp1 : 4;
        uint32_t nExtraB_comp2 : 4;
        uint32_t nExtraB_comp3 : 4;
        uint32_t : 4;
    };
    struct {
        uint32_t nExtraAComp : 12;
        uint32_t nExtraARule : 4;

        uint32_t nExtraBComp : 12;
        uint32_t nExtraBRule : 4;
    };

    struct {
        uint32_t nExtraA : 16;
        uint32_t nExtraB : 16;
    };
};

struct UpgradePosnDesc {
public:
    UpgradePosnDesc() {};
    ~UpgradePosnDesc() {};

public:
    CVector     m_vPosition;
    CQuaternion m_qRotation;
    int32_t     m_nParentComponentId;
};

class CVehicleModelInfo : public CClumpModelInfo {
public:
    RpMaterial*        m_pPlateMaterial;
    char               m_szPlateText[9];
    uint8_t            m_nPlateType;
    char               m_szGameName[8];
    eVehicleType       m_nVehicleType;
    float              m_fWheelSizeFront;
    float              m_fWheelSizeRear;
    int16_t            m_nWheelModelIndex;
    uint16_t           m_nHandlingId;
    uint8_t            m_nNumDoors;
    eVehicleClass      m_nVehicleClass;
    uint8_t            m_nFlags;
    uint8_t            m_nWheelUpgradeClass;
    uint8_t            m_nTimesUsed;
    char               field_51;
    uint16_t           m_nFrq;
    tVehicleCompsUnion m_extraComps;
    float              m_fBikeSteerAngle;

    class CVehicleStructure {
    public:
        CVehicleStructure();
        ~CVehicleStructure();
        static void* operator new(size_t size); // was `unsigned` (MSVC accepts, GCC hard-errors; same fix as CBuilding)
        static void  operator delete(void* data);

    public:
        static constexpr int32_t NUM_DUMMIES = 15;
        static constexpr int32_t NUM_UPGRADES = 18;
        static constexpr int32_t NUM_EXTRAS = 6;

    public:
        std::array<CVector, NUM_DUMMIES>         m_avDummyPos;
        std::array<UpgradePosnDesc, NUM_UPGRADES> m_aUpgrades;
        std::array<RpAtomic*, NUM_EXTRAS>         m_apExtras;
        uint8_t           m_nNumExtras;
        uint32_t          m_nMaskComponentsDamagable;

    public:
        // StaticRef<CPool<CVehicleStructure>*>(0xB4E680) in the original.
        static CPool<CVehicleStructure>* m_pInfoPool; // 0xB4E680

    public: // Helpers
        [[nodiscard]] bool IsDummyActive(eVehicleDummy dummy) const {
            return !m_avDummyPos[static_cast<size_t>(dummy)].IsZero();
        }

        [[nodiscard]] bool IsComponentDamageable(int32_t nodeIndex) const {
            return m_nMaskComponentsDamagable & (1 << nodeIndex);
        }

    } * m_pVehicleStruct;

    char        field_60[464];
    std::array<RpMaterial*, 32> m_apDirtMaterials;
    std::array<uint8_t, 8>      m_anPrimaryColors;
    std::array<uint8_t, 8>      m_anSecondaryColors;
    std::array<uint8_t, 8>      m_anTertiaryColors;
    std::array<uint8_t, 8>      m_anQuaternaryColors;
    uint8_t       m_nNumColorVariations;
    uint8_t       m_nLastColorVariation;
    uint8_t       m_nCurrentPrimaryColor;
    uint8_t       m_nCurrentSecondaryColor;
    uint8_t       m_nCurrentTertiaryColor;
    uint8_t       m_nCurrentQuaternaryColor;
    std::array<int16_t, 18> m_anUpgrades;
    std::array<int16_t, 4>  m_anRemapTxds;

    union {
        CAnimBlock* m_pAnimBlock;
        char* m_animBlockFileName;
        uint32_t m_nAnimBlockIndex;
    };

    class CLinkedUpgradeList {
    public:
        std::array<int16_t, 30> m_anUpgrade1;
        std::array<int16_t, 30> m_anUpgrade2;
        uint32_t m_nLinksCount;

    public:
        // add upgrade with components upgrade1 and upgrade2
        void AddUpgradeLink(int16_t upgrade1, int16_t upgrade2);
        // find linked upgrade for this upgrade. In this case upgrade param could be upgrade1 or upgrade2
        int16_t FindOtherUpgrade(int16_t upgrade);
    };

    // StaticRef<CLinkedUpgradeList>(0xB4E6D8) in the original.
    static CLinkedUpgradeList ms_linkedUpgrades; // 0xB4E6D8

    // vehicle components description tables
    // static RwObjectNameIdAssocation ms_vehicleDescs[12];
    static constexpr int32_t NUM_VEHICLE_MODEL_DESCS = 12;
    // StaticRef<RwObjectNameIdAssocation*[NUM_VEHICLE_MODEL_DESCS]>(0x8A7740); use eVehicleType to access
    static RwObjectNameIdAssocation* ms_vehicleDescs[NUM_VEHICLE_MODEL_DESCS]; // 0x8A7740

    // remap texture
    // StaticRef<RwTexture*>(0xB4E47C) in the original.
    static RwTexture* ms_pRemapTexture; // 0xB4E47C
    // vehiclelights128 texture
    // StaticRef<RwTexture*>(0xB4E68C) in the original.
    static RwTexture* ms_pLightsTexture; // 0xB4E68C
    // vehiclelightson128 texture
    // StaticRef<RwTexture*>(0xB4E690) in the original.
    static RwTexture* ms_pLightsOnTexture; // 0xB4E690

    // color of currently rendered car
    // static uint8 ms_currentCol[4];
    static constexpr int32_t NUM_CURRENT_COLORS = 4;
    // StaticRef<uint8[NUM_CURRENT_COLORS]>(0xB4E3F0) in the original.
    static uint8_t ms_currentCol[NUM_CURRENT_COLORS]; // 0xB4E3F0

    // number of wheel upgrades available
    // static int16 ms_numWheelUpgrades[4];
    static constexpr int32_t NUM_WHEELS = 4;
    // StaticRef<int16[NUM_WHEELS]>(0xB4E470) in the original.
    static int16_t ms_numWheelUpgrades[NUM_WHEELS]; // 0xB4E470

    // StaticRef<int32[NUM_WHEELS]>(0x8A7770) in the original.
    static int32_t ms_wheelFrameIDs[NUM_WHEELS]; // 0x8A7770

    // wheels upgrades data
    static constexpr int32_t NUM_WHEEL_UPGRADES = 15;
    // StaticRef<notsa::mdarray<int16, NUM_WHEELS, NUM_WHEEL_UPGRADES>>(0xB4E3F8) in the original.
    static std::array<std::array<int16_t, NUM_WHEEL_UPGRADES>, NUM_WHEELS> ms_upgradeWheels; // 0xB4E3F8

    // Light states for currently rendered car
    static constexpr int32_t NUM_LIGHTS = 4;
    // StaticRef<uint8[NUM_LIGHTS]>(0xB4E3E8) in the original.
    static uint8_t ms_lightsOn[NUM_LIGHTS]; // 0xB4E3E8

    // extras ids for next-spawned car
    // static char ms_compsUsed[2];
    static constexpr int32_t NUM_COMPS_USAGE = 2;
    // StaticRef<int8[NUM_COMPS_USAGE]>(0xB4E478) in the original.
    static int8_t ms_compsUsed[NUM_COMPS_USAGE]; // 0xB4E478
    // StaticRef<int8[NUM_COMPS_USAGE]>(0x8A6458) in the original.
    static int8_t ms_compsToUse[NUM_COMPS_USAGE]; // 0x8A6458

    // vehicle colours from carcols.dat
    // static CRGBA ms_vehicleColourTable[128];
    static constexpr int32_t NUM_VEHICLE_COLORS = 128;
    // StaticRef<CRGBA[NUM_VEHICLE_COLORS]>(0xB4E480) in the original.
    static CRGBA ms_vehicleColourTable[NUM_VEHICLE_COLORS]; // 0xB4E480

    // StaticRef<RwTextureCallBackFind>(0xB4E6A0) in the original.
    static RwTextureCallBackFind SavedTextureFindCallback; // 0xB4E6A0

public:
    CVehicleModelInfo();
    ~CVehicleModelInfo() override = default; // 0x4C5920;

    ModelInfoType GetModelType() override;
    void Init() override;
    void DeleteRwObject() override;
    RwObject* CreateInstance() override;
    void SetAnimFile(const char* filename) override;
    void ConvertAnimFileIndex() override;
    int32_t GetAnimFileIndex() override;
    void SetClump(RpClump* clump) override;

    // VTable implementations

    // Class methods
    // setup model render callbacks
    void SetAtomicRenderCallbacks();
    // set component flags
    void SetVehicleComponentFlags(RwFrame* component, uint32_t flags);
    // get wheel position. Wheel is wheel id [0-3]. Local - get local offset (if false it will get world position)
    void GetWheelPosn(int32_t wheel, CVector& outVec, bool local) const;
    // get component local offset. Component is a frame hierarchy id. Returns true if component present
    bool GetOriginalCompPosition(CVector& outVec, int32_t component);
    // get vehicle extra with rules. Returns extra id.
    int32_t ChooseComponent();
    // get vehicle second extra with rules. Returns extra id.
    int32_t ChooseSecondComponent();
    // check if upgrade is available
    bool IsUpgradeAvailable(eVehicleUpgradePosn upgrade);
    // set current vehicle colour for model
    void SetVehicleColour(uint8_t prim, uint8_t sec, uint8_t tert, uint8_t quat);
    // get color for car. variationShift determines how many color variations to skip.
    // For example, 1 will simply give you next color variation.
    void ChooseVehicleColour(uint8_t& prim, uint8_t& sec, uint8_t& tert, uint8_t& quat, int32_t variationShift);
    // get num remaps in this model
    int32_t GetNumRemaps();
    // add remap to model. Txd is id of tex dictionary.
    void AddRemap(int32_t txd);
    // setups rendering pipelines for atomics in model (CCustomCarEnvMapPipeline::CustomPipeAtomicSetup)
    void SetRenderPipelines();
    // gets car plate text
    char* GetCustomCarPlateText();
    // sets plate text
    void SetCustomCarPlateText(char* text);
    // remove some unused materials in model?
    void ReduceMaterialsInVehicle();
    // setup vehicle model components
    void PreprocessHierarchy();
    // setup custom plate
    void SetCarCustomPlate();
    // disable environment map effect on model
    void DisableEnvMap();
    // setup environment map intensity for model
    void SetEnvMapCoeff(float coeff);
    // get num doors in this model
    int32_t GetNumDoors();
    // get position of dummy in model-space
    // NOTE: original was a C++23 explicit-object-parameter NOTSA helper
    // (`auto&& GetModelDummyPosition(this auto&& self, ...)`); spelled as an
    // overload pair for C++17.
    CVector& GetModelDummyPosition(eVehicleDummy dummy) { return m_pVehicleStruct->m_avDummyPos[dummy]; }
    const CVector& GetModelDummyPosition(eVehicleDummy dummy) const { return m_pVehicleStruct->m_avDummyPos[dummy]; }
    // Static method's
    // setup lights states for currently rendered vehicle
    static void SetupLightFlags(class CVehicle* vehicle);
    // destroying vehiclelights textures
    static void ShutdownLightTexture();
    // find remap texture with name
    static RwTexture* FindTextureCB(const char* name);
    // start using special finding callback
    static void UseCommonVehicleTexDicationary();
    // stop using special finding callback
    static void StopUsingCommonVehicleTexDicationary();
    // set new parent frame for object. Data is actually RwFrame *
    static RwObject* MoveObjectsCB(RwObject* object, void* data);
    // change colors and settings of material according to vehicle color and lights states.  Data
    // contains pointer to restore entries
    static RpMaterial* SetEditableMaterialsCB(RpMaterial* material, void* data);
    // execute SetEditableMaterialsCB(RpMaterial *, void *) for atomic materials and also remove
    // vehicle window if needed. Data contains pointer to restore entries
    static RpAtomic* SetEditableMaterialsCB(RpAtomic* atomic, void* data);
    // execute SetEditableMaterialsCB(RpAtomic *, void *) for atomics in clump. This one is called
    // before vehicle rendering
    static void SetEditableMaterials(RpClump* clump);
    // reset materials settings. This one is called after vehicle rendering
    static void ResetEditableMaterials(RpClump* clump);
    // this is used to disable _dam atomic and "enable" _ok atomic at vehicle model setup. Data is unused
    static RpAtomic* HideDamagedAtomicCB(RpAtomic* atomic, void* data);
    // hide all atomics with state data (data is actually uint8)
    static RpAtomic* HideAllComponentsAtomicCB(RpAtomic* atomic, void* data);
    // check if material has alpha. Boolean result is stored to data (data is actually bool *)
    static RpMaterial* HasAlphaMaterialCB(RpMaterial* material, void* data);
    // setup atomic renderer. Data is unused
    static RpAtomic* SetAtomicRendererCB(RpAtomic* atomic, void* data);
    // setup heli renderer. Data is unused
    static RpAtomic* SetAtomicRendererCB_RealHeli(RpAtomic* atomic, void* data);
    // setup plane renderer. Data is unused
    static RpAtomic* SetAtomicRendererCB_Plane(RpAtomic* atomic, void* data);
    // setup boat renderer. Data is unused
    static RpAtomic* SetAtomicRendererCB_Boat(RpAtomic* atomic, void* data);
    // setup heli renderer. Data is unused
    static RpAtomic* SetAtomicRendererCB_Heli(RpAtomic* atomic, void* data);
    // setup train renderer. Data is unused
    static RpAtomic* SetAtomicRendererCB_Train(RpAtomic* atomic, void* data);
    // setup objects flag. Data is actually flag (uint16)
    static RwObject* SetAtomicFlagCB(RwObject* object, void* data);
    // clear all atomic flag. Data is actually flag (uint16)
    static RwObject* ClearAtomicFlagCB(RwObject* object, void* data);
    // adds wheel upgrade. This one is called from LoadVehicleUpgrades()
    static void AddWheelUpgrade(int32_t wheelSetNumber, int32_t modelId);
    // gets num upgrades for this set
    static int32_t GetWheelUpgrade(int32_t wheelSetNumber, int32_t wheelUpgradeNumber);
    // gets wheel upgrade
    static int32_t GetNumWheelUpgrades(int32_t wheelSetNumber);
    // do nothing
    static void DeleteVehicleColourTextures();
    // Set vehicle dirt textures
    static void SetDirtTextures(CVehicleModelInfo* info, int32_t dirtLevel);
    // unloads 'white' texture
    static void ShutdownEnvironmentMaps();
    // gets mat effect of this material. Data is actually int32 *
    static RpMaterial* GetMatFXEffectMaterialCB(RpMaterial* material, void* data);
    // disables MatFX on the material (CCustomCarPlateMgr-adjacent helper; TODO: verify owner subsystem)
    static RpMaterial* DisableMatFx(RpMaterial* material, void* data);
    // sets mat effect for this material. Data is actually int32
    static RpMaterial* SetEnvironmentMapCB(RpMaterial* material, void* data);
    // sets environment map intensity. Data is actually uint32
    static RpMaterial* SetEnvMapCoeffCB(RpMaterial* material, void* data);
    // do nothing
    static RpAtomic* SetRenderPipelinesCB(RpAtomic* atomic, void* data);
    // gets max number of passengers for model
    static int32_t GetMaximumNumberOfPassengersFromNumberOfDoors(int32_t modelId);
    // move all objects from data (it is actually RwFrame *) to frame
    static RwFrame* CollapseFramesCB(RwFrame* frame, void* data);
    // setup environment map for atomic's materials. Data is actually int32 and it represents effect id
    static RpAtomic* SetEnvironmentMapAtomicCB(RpAtomic* atomic, void* data);
    // setup environment map intensity for atomic with data (uint32)
    static RpAtomic* SetEnvMapCoeffAtomicCB(RpAtomic* atomic, void* data);
    static void AssignRemapTxd(const char* name, int16_t txdSlot);
    static RpAtomic* StoreAtomicUsedMaterialsCB(RpAtomic* atomic, void* data); // data is RpMaterialList**

    static void SetupCommonData();
    static void LoadVehicleColours();
    static void LoadVehicleUpgrades();
    // loads 'white' texture
    static void LoadEnvironmentMaps();

    // inlined in Android
    const CVector& GetFrontSeatPosn() { return m_pVehicleStruct->m_avDummyPos[IsBoat() ? 0 : 4]; } // TODO: 0/4 ?
    const CVector& GetBackSeatPosn() { return m_pVehicleStruct->m_avDummyPos[5]; } // TODO: 5 ?

    // Helpers
    // ctrl+c, ctrl+v from CVehicle
    [[nodiscard]] bool IsVehicleTypeValid()     const { return m_nVehicleType != VEHICLE_TYPE_IGNORE; }
    [[nodiscard]] bool IsAutomobile()           const { return m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE; }
    [[nodiscard]] bool IsMonsterTruck()        const { return m_nVehicleType == VEHICLE_TYPE_MTRUCK; }
    [[nodiscard]] bool IsQuad()                const { return m_nVehicleType == VEHICLE_TYPE_QUAD; }
    [[nodiscard]] bool IsHeli()                const { return m_nVehicleType == VEHICLE_TYPE_HELI; }
    [[nodiscard]] bool IsPlane()               const { return m_nVehicleType == VEHICLE_TYPE_PLANE; }
    [[nodiscard]] bool IsBoat()                const { return m_nVehicleType == VEHICLE_TYPE_BOAT; }
    [[nodiscard]] bool IsTrain()               const { return m_nVehicleType == VEHICLE_TYPE_TRAIN; }
    [[nodiscard]] bool IsFakeAircraft()        const { return m_nVehicleType == VEHICLE_TYPE_FHELI || m_nVehicleType == VEHICLE_TYPE_FPLANE; }
    [[nodiscard]] bool IsBike()                const { return m_nVehicleType == VEHICLE_TYPE_BIKE; }
    [[nodiscard]] bool IsBMX()                 const { return m_nVehicleType == VEHICLE_TYPE_BMX; }
    [[nodiscard]] bool IsTrailer()             const { return m_nVehicleType == VEHICLE_TYPE_TRAILER; }

    // These were probably inlined:
    void SetWheelSizes(float front, float rear) {
        m_fWheelSizeFront = front;
        m_fWheelSizeRear = rear;
    }
    void SetGameName(const char* name) {
        // Original used strcpy_s; portable equivalent.
        std::strncpy(m_szGameName, name, sizeof(m_szGameName) - 1);
        m_szGameName[sizeof(m_szGameName) - 1] = '\0';
    }
    void SetHandlingId(const char* handlingName);

    // These two should probably be moved to a better place..
    [[nodiscard]] bool IsFrontWheel(eCarWheel wheel) const {
        switch (wheel) {
        case eCarWheel::CAR_WHEEL_FRONT_LEFT:
        case eCarWheel::CAR_WHEEL_FRONT_RIGHT:
            return true;
        }
        return false;
    }

    [[nodiscard]] bool IsRearWheel(eCarWheel door) const {
        return !IsFrontWheel(door);
    }

    // Return size of give wheel. If it's a front wheel `m_fWheelSizeFront` is returned, otherwise `m_fWheelSizeRear`
    [[nodiscard]] float GetSizeOfWheel(eCarWheel wheel) const {
        return IsFrontWheel(wheel) ? m_fWheelSizeFront : m_fWheelSizeRear;
    }

    float GetWheelSize(bool front) { return front ? m_fWheelSizeFront : m_fWheelSizeRear; } // 0x6A06F0

    tHandlingData& GetHandlingData() const;
    tFlyingHandlingData& GetFlyingHandlingData() const;

    auto GetVehicleStruct() const { return m_pVehicleStruct; }
};

// File-scope globals. In the original these were `extern T&` references bound to
// StaticRef<T>(addr) in the .cpp; here they are plain globals defined in the .cpp
// (original GTA SA 1.0 addresses kept as comments).
static constexpr int32_t NUM_RESTORE_ENTRIES = 256;
extern RwTexDictionary* vehicleTxd;                          // 0xB4E688
extern RwFrame* carFrame;                                     // 0xB4E6B8
extern RwSurfaceProperties gLightSurfProps;                   // 0x8A645C
extern tRestoreEntry gRestoreEntries[NUM_RESTORE_ENTRIES];    // 0xB4DBE8
extern RwTexture* gpWhiteTexture;                             // 0xB4E3EC
extern float fEnvMapDefaultCoeff;                             // 0x8A7780
extern float fRearDoubleWheelOffsetFactor;                    // 0x8A7784

// Free functions (were `static` at header scope in the original).
bool IsValidCompRule(int32_t nRule);
int32_t ChooseComponent(int32_t rule, int32_t comps);
int32_t CountCompsInRule(int32_t comps);
int32_t GetListOfComponentsNotUsedByRules(uint32_t compRules, int32_t numExtras, int32_t* outList);
RpMaterial* RemoveWindowAlphaCB(RpMaterial* material, void* data); // data is RpMaterialList**
RwObject* GetOkAndDamagedAtomicCB(RwObject* object, void* data);   // data is &RpAtomic[2]

// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tRestoreEntry) == 0x8, "tRestoreEntry layout drift");
static_assert(sizeof(tVehicleCompsUnion) == 0x4, "tVehicleCompsUnion layout drift");
static_assert(sizeof(UpgradePosnDesc) == 0x20, "UpgradePosnDesc layout drift");
static_assert(sizeof(CVehicleModelInfo::CVehicleStructure) == 0x314, "CVehicleStructure layout drift");
static_assert(sizeof(CVehicleModelInfo) == 0x308, "CVehicleModelInfo layout drift");
#endif
