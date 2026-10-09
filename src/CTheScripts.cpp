// CTheScripts.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CTheScripts. Adapted from gta-reversed for clean-room C++ build.
//
// All methods below are TODO stubs: verify each against the decomp
// (C:\Users\fufid\Documents\Decomps\gta-sa decomp\src\CTheScripts\*.c)
// and gta-reversed/source/game_sa/Scripts/TheScripts.cpp before implementing.
// Priority: Init, Process, StartNewScript, Save/Load, the script-thing
// Add/Remove helpers (checkpoints, spheres, searchlights, effect systems).

#include "CTheScripts.h"
#include "CRunningScript.h" // full CRunningScript type (StartNewScript)

// Static member definitions (original 1.0 US addresses in comments).
// TODO: re-resolve for the clean-room build.
uint8_t CTheScripts::ScriptSpace[CTheScripts::SCRIPT_SPACE_SIZE]{}; // 0xA49960

std::array<tScriptSwitchCase, MAX_NUM_SwitchJumpTable> CTheScripts::SwitchJumpTable{}; // 0xA43CF8
uint16_t CTheScripts::NumberOfEntriesInSwitchTable{};        // 0xA43F50
uint16_t CTheScripts::NumberOfEntriesStillToReadForSwitch{}; // 0xA43F60

std::array<int16_t, MAX_NUM_CARDS> CTheScripts::CardStack{}; // 0xA44218
std::array<int32_t, MAX_NUM_MISSION_SCRIPTS> CTheScripts::MultiScriptArray{}; // 0xA444C8
std::array<tScriptConnectLodsObject, MAX_NUM_SCRIPT_CONNECT_LODS_OBJECTS> CTheScripts::ScriptConnectLodsObjects{}; // 0xA44800
std::array<tScriptAttachedAnimGroup, MAX_NUM_SCRIPT_ATTACHED_ANIM_GROUPS> CTheScripts::ScriptAttachedAnimGroups{}; // 0xA44850
std::array<eModelID, MAX_NUM_VEHICLE_MODELS_BLOCKED_BY_SCRIPT> CTheScripts::VehicleModelsBlockedByScript{}; // 0xA448F0
std::array<eModelID, MAX_NUM_SUPPRESSED_VEHICLE_MODELS> CTheScripts::SuppressedVehicleModels{}; // 0xA44940
std::array<CEntity*, MAX_NUM_INVISIBILITY_SETTINGS> CTheScripts::InvisibilitySettingArray{}; // 0xA449E0

std::array<tScriptParam, MAX_NUM_LOCAL_VARIABLES_FOR_CURRENT_MISSION> CTheScripts::LocalVariablesForCurrentMission{}; // 0xA48960
uint32_t CTheScripts::LargestNumberOfMissionScriptLocalVariables{}; // 0xA444B4

std::array<tBuildingSwap, MAX_NUM_BUILDING_SWAPS> CTheScripts::BuildingSwapArray{}; // 0xA44A30

std::array<tUsedObject, MAX_NUM_USED_OBJECTS> CTheScripts::UsedObjectArray{}; // 0xA44B70
uint16_t CTheScripts::NumberOfUsedObjects{}; // 0xA44B6C

std::array<tScriptBrainWaitEntity, MAX_NUM_ENTITIES_WAITING_FOR_SCRIPT_BRAIN> CTheScripts::EntitiesWaitingForScriptBrain{}; // 0xA476B0
std::array<tScriptText, MAX_NUM_INTRO_TEXT_LINES> CTheScripts::IntroTextLines{}; // 0xA913E8
uint16_t CTheScripts::NumberOfIntroTextLinesThisFrame{}; // 0xA44B68

std::array<tScriptRectangle, MAX_NUM_SCRIPT_RECTANGLES> CTheScripts::IntroRectangles{}; // 0xA92D68
uint16_t CTheScripts::NumberOfIntroRectanglesThisFrame{}; // 0xA44B5C

std::array<CSprite2d, MAX_NUM_SCRIPT_SPRITES> CTheScripts::ScriptSprites{}; // 0xA94B68

uint16_t CTheScripts::NumberOfExclusiveMissionScripts{}; // 0xA444B8
uint16_t CTheScripts::NumberOfMissionScripts{};          // 0xA444BC

std::array<tScriptSphere, MAX_NUM_SCRIPT_SPHERES> CTheScripts::ScriptSphereArray{}; // 0xA91268
std::array<tScriptEffectSystem, MAX_NUM_SCRIPT_EFFECT_SYSTEMS> CTheScripts::ScriptEffectSystemArray{}; // 0xA44110
std::array<tScriptSearchlight, MAX_NUM_SCRIPT_SEARCH_LIGHT> CTheScripts::ScriptSearchLightArray{}; // 0xA94D68

std::array<tScriptSequence, MAX_NUM_SCRIPT_SEQUENCE_TASKS> CTheScripts::ScriptSequenceTaskArray{}; // 0xA43F68
uint16_t CTheScripts::NumberOfScriptSearchLights{}; // 0xA90830

std::array<tScriptCheckpoint, MAX_NUM_SCRIPT_CHECKPOINTS> CTheScripts::ScriptCheckpointArray{}; // 0xA44070
uint16_t CTheScripts::NumberOfScriptCheckpoints{}; // 0xA44068
uint32_t CTheScripts::UnknownDebugStuff{};         // 0xA95190

CTheScripts::eUseTextCommandState CTheScripts::UseTextCommands{}; // 0xA44B67
bool     CTheScripts::DbgFlag{};                     // 0x859CF8
int32_t  CTheScripts::SwitchDefaultAddress{};        // 0xA43F54
bool     CTheScripts::SwitchDefaultExists{};         // 0xA43F58
int32_t  CTheScripts::ValueToCheckInSwitchStatement{}; // 0xA43F5C
int16_t  CTheScripts::CardStackPosition{};           // 0xA44210
bool     CTheScripts::bDrawSubtitlesBeforeFade{};    // 0xA44488
bool     CTheScripts::bDrawOddJobTitleBeforeFade{};  // 0xA44489
bool     CTheScripts::bScriptHasFadedOut{};          // 0xA4448A
bool     CTheScripts::bAddNextMessageToPreviousBriefs{}; // 0xA4448B
int32_t  CTheScripts::ForceRandomCarModel{};         // 0xA4448C
eCrossHairType CTheScripts::bDrawCrossHair{};        // 0xA44490
bool     CTheScripts::bEnableCraneRelease{};         // 0xA44494
bool     CTheScripts::bEnableCraneLower{};           // 0xA44495
bool     CTheScripts::bEnableCraneRaise{};           // 0xA44496
float    CTheScripts::fCameraHeadingStepWhenPlayerIsAttached{}; // 0xA44498
float    CTheScripts::fCameraHeadingWhenPlayerIsAttached{};     // 0xA4449C
bool     CTheScripts::bDisplayHud{};                 // 0xA444A0
bool     CTheScripts::HideAllFrontEndMapBlips{};     // 0xA444A1
bool     CTheScripts::RadarShowBlipOnAllLevels{};    // 0xA444A2
uint8_t  CTheScripts::RadarZoomValue{};              // 0xA444A3
bool     CTheScripts::bPlayerIsOffTheMap{};          // 0xA444A4
char     CTheScripts::RiotIntensity{};               // 0xA444A5
bool     CTheScripts::bPlayerHasMetDebbieHarry{};    // 0xA444A6
bool     CTheScripts::bDisplayNonMiniGameHelpMessages{}; // 0xA444A7
bool     CTheScripts::bMiniGameInProgress{};         // 0xA444A8
int32_t  CTheScripts::ScriptPickupCycleIndex{};      // 0xA444AC
int8_t   CTheScripts::FailCurrentMission{};          // 0xA444B0
bool     CTheScripts::bAlreadyRunningAMissionScript{}; // 0xA444B1
uint32_t CTheScripts::LargestMissionScriptSize{};    // 0xA444C0
uint32_t CTheScripts::MainScriptSize{};              // 0xA444C4
bool     CTheScripts::bUsingAMultiScriptFile{};      // 0xA447E8
int32_t  CTheScripts::StoreVehicleIndex{};           // 0xA447EC
bool     CTheScripts::StoreVehicleWasRandom{};       // 0xA447F0
uint16_t CTheScripts::CommandsExecuted{};            // 0xA447F4
uint16_t CTheScripts::ScriptsUpdated{};              // 0xA447F8
uint16_t CTheScripts::MessageWidth{};                // 0xA44B60
uint16_t CTheScripts::MessageCentre{};               // 0xA44B64
bool     CTheScripts::bUseMessageFormatting{};       // 0xA44B66
int32_t  CTheScripts::LastRandomPedId{};             // 0xA476A4
uint32_t CTheScripts::LastMissionPassedTime{};       // 0xA476A8
int32_t  CTheScripts::OnAMissionFlag{};              // 0xA476AC

CRunningScript* CTheScripts::pIdleScripts{};   // 0xA8B428
CRunningScript* CTheScripts::pActiveScripts{}; // 0xA8B42C

// TODO: define the remaining static members (StreamedScripts,
// ScriptResourceManager, UpsideDownCars, MissionCleanUp, StuckCars,
// ScriptsForBrains) when their classes are ported — they are incomplete
// types here, so only the declarations in CTheScripts.h exist for now.


// ---------------------------------------------------------------------------
// Method implementations
// ---------------------------------------------------------------------------
// Decomp reference: <decomp>/src/CTheScripts/*.c
// Only the subsystem-independent methods are implemented below. Init(),
// Process(), Save(), Load() and the ~40 script-thing Add/Remove helpers
// need unported subsystems (CGame, CReplay, CStreaming, pools, ...) and are
// listed as TODOs at the bottom with their decomp file references.

// @ 0156e5f0 [.HOODLUM] (original entry stub at 00470370)
void CTheScripts::ReinitialiseSwitchStatementData() {
    NumberOfEntriesStillToReadForSwitch = 0;
    ValueToCheckInSwitchStatement = 0;
    SwitchDefaultExists = false;
    SwitchDefaultAddress = 0;
    NumberOfEntriesInSwitchTable = 0;
}

// @ 00470390 [.text]
void CTheScripts::AddToSwitchJumpTable(int32_t switchValue, int32_t switchLabelLocalAddress) {
    SwitchJumpTable[NumberOfEntriesInSwitchTable].m_nSwitchValue = switchValue;
    SwitchJumpTable[NumberOfEntriesInSwitchTable].m_nSwitchLabelAddress = switchLabelLocalAddress;
    NumberOfEntriesInSwitchTable++;
}

// @ 01561b80 [.HOODLUM] (original entry stub at 004703c0)
// Binary-searches the switch jump table for ValueToCheckInSwitchStatement,
// writes the matching label address (or the default) to
// `switchLabelAddress`, then resets the switch state.
void CTheScripts::UseSwitchJumpTable(int32_t& switchLabelAddress) {
    switchLabelAddress = 0;
    // NOTE: the original keeps the bounds in a byte and an int register;
    // the table is small (MAX_NUM_SwitchJumpTable), so int is equivalent.
    int lo = 0;
    int hi = static_cast<int>(NumberOfEntriesInSwitchTable) - 1;
    while (true) {
        if (hi - lo < 2) {
            if (ValueToCheckInSwitchStatement == SwitchJumpTable[hi].m_nSwitchValue)
                switchLabelAddress = SwitchJumpTable[hi].m_nSwitchLabelAddress;
            else if (ValueToCheckInSwitchStatement == SwitchJumpTable[lo].m_nSwitchValue)
                switchLabelAddress = SwitchJumpTable[lo].m_nSwitchLabelAddress;
            else
                switchLabelAddress = SwitchDefaultAddress;
            ReinitialiseSwitchStatementData();
            return;
        }
        const uint32_t mid = (static_cast<uint32_t>(hi) + static_cast<uint32_t>(lo)) / 2;
        if (ValueToCheckInSwitchStatement == SwitchJumpTable[mid].m_nSwitchValue) {
            // Found: the original stashes the label in SwitchDefaultAddress
            // and falls through to the default-address path.
            switchLabelAddress = SwitchJumpTable[mid].m_nSwitchLabelAddress;
            ReinitialiseSwitchStatementData();
            return;
        }
        if (SwitchJumpTable[mid].m_nSwitchValue < ValueToCheckInSwitchStatement)
            lo = static_cast<int>(mid);
        else
            hi = static_cast<int>(mid);
    }
}

// @ 0156aa10 [.HOODLUM] (original entry stub at 00464d50)
// NOTE: the original returns a packed int (OnAMissionFlag's high bits with
// the boolean in the low byte); the header declares bool, which is the
// meaningful part.
bool CTheScripts::IsPlayerOnAMission() {
    return OnAMissionFlag != 0 &&
           *reinterpret_cast<int32_t*>(ScriptSpace + OnAMissionFlag) == 1;
}

// @ 01569f10 [.HOODLUM] (original entry stub at 00464bb0)
void CTheScripts::WipeLocalVariableMemoryForMissionScript() {
    for (auto& var : LocalVariablesForCurrentMission)
        var.uParam = 0;
}

// @ 0156a2a0 [.HOODLUM] (original entry stub at 00464c20)
// Takes the first script from the idle list, initialises it, and pushes it
// onto the active list with m_IP = startIP.
CRunningScript* CTheScripts::StartNewScript(uint8_t* startIP) {
    CRunningScript* newScript = pIdleScripts;
    // Unlink from the idle list.
    if (newScript->m_pPrev == nullptr)
        pIdleScripts = newScript->m_pNext;
    else
        newScript->m_pPrev->m_pNext = newScript->m_pNext;
    if (newScript->m_pNext != nullptr)
        newScript->m_pNext->m_pPrev = newScript->m_pPrev;
    newScript->Init();
    newScript->m_IP = startIP;
    // Push onto the head of the active list.
    newScript->m_pNext = pActiveScripts;
    newScript->m_pPrev = nullptr;
    if (pActiveScripts != nullptr)
        pActiveScripts->m_pPrev = newScript;
    pActiveScripts = newScript;
    newScript->m_IsActive = true;
    return newScript;
}

// @ 0156e210 [.HOODLUM] (original entry stub at 00464c90)
// Indexed variant of StartNewScript: takes the specific idle-list script at
// slot `index` (into the CRunningScript pool).
// TODO: needs ScriptsArray (the CRunningScript pool, not ported yet).
// Decomp: src/CTheScripts/StartNewScript_0156e210.c
CRunningScript* CTheScripts::StartNewScript(uint8_t* startIP, uint16_t index) {
    (void)startIP;
    (void)index;
    // Faithful body once ScriptsArray is ported: walk pIdleScripts until the
    // entry matching &ScriptsArray[index], unlink it, Init(), set m_IP, push
    // onto pActiveScripts, return it (nullptr if not found in the idle list).
    return nullptr;
}

// @ 01562030 [.HOODLUM] (original entry stub at 00464d20)
// Returns the pool index of a CRunningScript.
// TODO: needs ScriptsArray (not ported). Original:
//   return (thread - 0xA8B430 /* ScriptsArray */) / sizeof(CRunningScript);
// Decomp: src/CTheScripts/GetScriptIndexFromPointer_01562030.c
int32_t CTheScripts::GetScriptIndexFromPointer(CRunningScript* thread) {
    (void)thread;
    return -1;
}

// ---------------------------------------------------------------------------
// TODO: remaining CTheScripts methods (need unported subsystems)
// ---------------------------------------------------------------------------
// - Init()              @ 00468d50 — decomp: src/CTheScripts/Init_00468d50.c
//   Needs: ScriptsArray (96x CRunningScript pool), CMissionCleanup,
//   CUpsideDownCarCheck, CStuckCarCheck, CScriptsForBrains,
//   CScriptResourceManager, CStreamedScripts, CGame, CMenuManager, CFileMgr,
//   CText, CCredits, CTxdStore, CScripted2dEffects, CTaskSequences,
//   CPedGroups, CInformFriendsEventQueue.
// - Process()           @ 0046a000 — decomp: src/CTheScripts/Process_0046a000.c
//   Needs: CReplay, CGeneral, CLoadingScreen, CCredits, CPool (ped pool),
//   CPedIntelligence. Core loop: iterate pActiveScripts, bump the two
//   timer locals, call CRunningScript::Process() on each.
// - Save() / Load()     @ 005d4c40 / 005d4fd0 — decomp:
//   src/CTheScripts/Save_005d4c40.c, src/CTheScripts/Load_005d4fd0.c
// - Script-thing helpers: Add/RemoveScriptCheckpoint, Add/RemoveScriptSphere,
//   Add/RemoveScriptSearchLight (+Move/Attach/Process/Render/Is*),
//   Add/RemoveScriptEffectSystem, AddToBuildingSwapArray, UndoBuildingSwaps,
//   AddToInvisibilitySwapArray, UndoEntityInvisibilitySettings,
//   AddToListOfConnectedLodObjects, ScriptConnectLodsFunction,
//   AddToListOfSpecialAnimGroupsAttachedToCharModels,
//   ScriptAttachAnimGroupToCharModel, AddToVehicleModelsBlockedByScript (+
//   Remove/Has/Clear), ClearAllSuppressedCarModels, HasCarModelBeenSuppressed,
//   AddToWaitingForScriptBrainArray (+Remove/Process), CleanUpThisPed,
//   CleanUpThisVehicle, CleanUpThisObject, RemoveThisPed,
//   ClearSpaceForMissionEntity, GetNewUniqueScriptThingIndex,
//   GetActualScriptThingIndex, GetUniqueScriptThingIndex,
//   DrawScriptSpheres, DrawScriptSpritesAndRectangles, HighlightImportantArea,
//   HighlightImportantAngledArea, DrawDebugSquare/Cube/Angled variants,
//   ScriptDebugCircle2D, ScriptDebugLine3D, RenderTheScriptDebugLines,
//   DoScriptSetupAfterPoolsHaveLoaded, ReadObjectNamesFromScript,
//   UpdateObjectIndices, ReadMultiScriptFileOffsetsFromScript,
//   CheckStreamedScriptVersion, StartTestScript, PrintListSizes,
//   IsPedStopped, IsVehicleStopped.
//   Decomp files: src/CTheScripts/*.c (named per function).
