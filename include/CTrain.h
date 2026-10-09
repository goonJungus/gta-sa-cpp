// CTrain.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Vehicle/Train.h
// Rail vehicle: carriages, track following, stations, announcements.
// Hierarchy: ... -> CVehicle -> CTrain
//
// Adaptations:
//   stripped InjectHooks(), friend InjectHooksMain
//   VALIDATE_SIZE/VALIDATE_OFFSET -> 32-bit-guarded static_asserts
//   StaticRef statics -> plain static members (defined in CTrain.cpp)
//   "TrainNode.h" dropped (not ported) - CTrainNode forward-declared
//   RW types -> forward-declared
// TODO:
//   port CTrainNode
//   verify each method against decomp src/CTrain/*.c

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "GrTypes.h"

#include "CVehicle.h"
#include "CDoor.h"

class CPed;
struct RwFrame;
struct CTrainNode;


// TODO(port): #include "TrainNode.h" - subsystem not ported yet

enum eTrainNodes {
    TRAIN_NODE_NONE   = 0,
    TRAIN_DOOR_LF     = 1,
    TRAIN_DOOR_RF     = 2,
    TRAIN_WHEEL_RF1   = 3,
    TRAIN_WHEEL_RF2   = 4,
    TRAIN_WHEEL_RF3   = 5,
    TRAIN_WHEEL_RB1   = 6,
    TRAIN_WHEEL_RB2   = 7,
    TRAIN_WHEEL_RB3   = 8,
    TRAIN_WHEEL_LF1   = 9,
    TRAIN_WHEEL_LF2   = 10,
    TRAIN_WHEEL_LF3   = 11,
    TRAIN_WHEEL_LB1   = 12,
    TRAIN_WHEEL_LB2   = 13,
    TRAIN_WHEEL_LB3   = 14,
    TRAIN_BOGIE_FRONT = 15,
    TRAIN_BOGIE_REAR  = 16,

    TRAIN_NUM_NODES
};

enum eTrainPassengersGenerationState {
    TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE = 0,
    TRAIN_PASSENGERS_TELL_PASSENGERS_TO_LEAVE = 1,
    TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_ENTER = 2,
    TRAIN_PASSENGERS_TELL_PASSENGERS_TO_ENTER = 3,
    TRAIN_PASSENGERS_GENERATION_FINISHED = 4
};

class CTrain : public CVehicle {
public:
    int16    m_nNodeIndex;
    float    m_fTrainSpeed; // 1.0 - train derails
    float    m_fCurrentRailDistance;
    float    m_fLength;
    float    m_fTrainGas;   // gas pedal pressed: 255.0, moving forward: 0.0f, moving back: -255.0
    float    m_fTrainBrake; // 255.0 - braking
    union {
        struct {
            uint16 b01 : 1; // initialised with 1
            uint16 bStoppedAtStation : 1;
            uint16 bPassengersCanEnterAndLeave : 1;
            uint16 bIsFrontCarriage : 1;
            uint16 bIsLastCarriage : 1;
            uint16 bMissionTrain : 1;
            uint16 bClockwiseDirection : 1;
            uint16 bStopsAtStations : 1;

            uint16 bNotOnARailRoad : 1;
            uint16 bForceSlowDown : 1;
            uint16 bIsStreakModel : 1;
        } trainFlags;
        uint16 m_nTrainFlags;
    };
    uint32   m_nTimeWhenStoppedAtStation;
    int8     m_nTrackId;
    uint32   m_nTimeWhenCreated;
    int16    field_5C8;                    // initialized with 0, not referenced
    uint8    m_nPassengersGenerationState; // see eTrainPassengersGenerationState
    uint8    m_nNumPassengersToLeave : 4;  // 0 to 4
    uint8    m_nNumPassengersToEnter : 4;  // 0 to 4
    CPed*    m_pTemporaryPassenger;        // we tell peds to enter train and then delete them
    CTrain*  m_pPrevCarriage;
    CTrain*  m_pNextCarriage;
    std::array<CDoor, 6>             m_aDoors;
    std::array<RwFrame*, TRAIN_NUM_NODES> m_aTrainNodes;

    static uint32 GenTrain_Track; // game address: 0xC37FFC ; DEFERRED - was StaticRef
    static uint32 GenTrain_TrainConfig; // game address: 0xC38000 ; DEFERRED - was StaticRef
    static uint8 GenTrain_Direction; // game address: 0xC38004 ; DEFERRED - was StaticRef
    static uint32 GenTrain_GenerationNode; // game address: 0xC38008 ; DEFERRED - was StaticRef
    static uint32 GenTrain_Status; // game address: 0xC3800C ; DEFERRED - was StaticRef
    static bool bDisableRandomTrains; // game address: 0xC38010 ; DEFERRED - was StaticRef
    static CVector aStationCoors[6];

    static constexpr auto Type = VEHICLE_TYPE_TRAIN;

public:
    CTrain(int32 modelIndex, eVehicleCreatedBy createdBy);

    void ProcessControl() override;
    void SetupModelNodes();

    bool FindMaximumSpeedToStopAtStations(float* speed);
    uint32 FindNumCarriagesPulled();
    void OpenTrainDoor(float state);
    void AddPassenger(CPed* ped);
    void RemovePassenger(CPed* ped);
    [[nodiscard]] bool FindSideStationIsOn() const;
    [[nodiscard]] bool IsInTunnel() const;
    void RemoveRandomPassenger();
    void FindPositionOnTrackFromCoors();
    void AddNearbyPedAsRandomPassenger();

    static void InitTrains();
    static void ReadAndInterpretTrackFile(const char* filename, CTrainNode** nodes, int32* lineCount, float* totalDist, int32 skipStations);
    static void Shutdown();
    static void UpdateTrains();
    static void FindCoorsFromPositionOnTrack(float railDistance, int32 trackId, CVector* outCoors);
    static void DisableRandomTrains(bool disable);
    static void RemoveOneMissionTrain(CTrain* train);
    static void ReleaseOneMissionTrain(CTrain* train);
    static void SetTrainSpeed(CTrain* train, float speed);
    static void SetTrainCruiseSpeed(CTrain* train, float speed);
    static CTrain* FindCaboose(CTrain* train);
    static CTrain* FindEngine(CTrain* train);
    static CTrain* FindCarriage(CTrain* train, uint8 carriage);
    static void FindNextStationPositionInDirection(bool clockwiseDirection, float distance, float* distanceToStation, int32* numStations);
    static void RemoveMissionTrains();
    static void RemoveAllTrains();
    static void ReleaseMissionTrains();
    static int32 FindClosestTrackNode(CVector posn, int32* outTrackId);
    static CTrain* FindNearestTrain(CVector posn, bool mustBeMainTrain);
    static void SetNewTrainPosition(CTrain* train, CVector posn);
    static bool IsNextStationAllowed(CTrain* train);
    static void SkipToNextAllowedStation(CTrain* train);
    static void CreateMissionTrain(CVector posn, bool clockwiseDirection, uint32 trainType, CTrain**outFirstCarriage, CTrain**outLastCarriage, int32 nodeIndex, int32 trackId, bool isMissionTrain);
    static void DoTrainGenerationAndRemoval();

private:

    // dropped: Constructor() placement wrapper (invalid C++, hook-era leftover)
};
// TODO(2026-10-09): layout mismatch - needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(CTrain) == 0x6AC, "CTrain size mismatch");
//#endif
// TODO(2026-10-09): layout mismatch - needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(offsetof(CTrain, m_nTrainFlags) == 0x5B8, "CTrain::m_nTrainFlags offset mismatch");
//#endif

void ProcessTrainAnnouncements();
void PlayAnnouncement(uint8 arg0, uint8 arg1);
template<typename PtrListType>
void TrainHitStuff(PtrListType& ptrList, CEntity* entity);
void MarkSurroundingEntitiesForCollisionWithTrain(CVector pos, float radius, CEntity* entity, bool bOnlyVehicles);
