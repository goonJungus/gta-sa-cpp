// CCarEnterExit.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/CarEnterExit.h
// Static helpers for peds entering/exiting vehicles (doors, jacking, anims).
// Hierarchy: standalone static utility class
//
// Adaptations:
//   stripped InjectHooks()
//   StaticRef statics -> plain static members (defined in CCarEnterExit.cpp)
//   CPed/CVehicle/CTask -> forward-declared
// TODO:
//   port CPed/CTask (unblocks real signatures)
//   verify each method against decomp src/CCarEnterExit/*.c

#pragma once

#include <cstdint>

#include "GrTypes.h"

#include "CVector.h"

class CPed;
class CVehicle;
class CTask;



class CPed;
class CVehicle;
class CTask;

class CCarEnterExit {
public:
    static float ms_fMaxSpeed_CanDragPedOut; // game address: 0x86F104 ; DEFERRED - was StaticRef (was: 0.1)
    static float ms_fMaxSpeed_PlayerCanDragPedOut; // game address: 0x86F108 ; DEFERRED - was StaticRef (was: 0.2)

    static bool ms_bPedOffsetsCalculated; // game address: 0xC18C20 ; DEFERRED - was StaticRef
    static CVector ms_vecPedGetUpAnimOffset; // game address: 0xC18C3C ; DEFERRED - was StaticRef
    static CVector ms_vecPedBedLAnimOffset; // game address: 0xC18C54 ; DEFERRED - was StaticRef
    static CVector ms_vecPedBedRAnimOffset; // game address: 0xC18C60 ; DEFERRED - was StaticRef
    static CVector ms_vecPedDeskAnimOffset; // game address: 0xC18C6C ; DEFERRED - was StaticRef
    static CVector ms_vecPedChairAnimOffset; // game address: 0xC18C78 ; DEFERRED - was StaticRef
    static CVector ms_vecPedQuickDraggedOutCarAnimOffset; // game address: 0xC18C48 ; DEFERRED - was StaticRef

public:

    static void AddInCarAnim(const CVehicle* vehicle, CPed* ped, bool bAsDriver);
    static bool CarHasDoorToClose(const CVehicle* vehicle, int32 doorId);
    static bool CarHasDoorToOpen(const CVehicle* vehicle, int32 doorId);
    static bool CarHasOpenableDoor(const CVehicle* vehicle, int32 doorId_UnusedArg, const CPed* ped);
    static bool CarHasPartiallyOpenDoor(const CVehicle* vehicle, int32 doorId);
    static int32 ComputeDoorFlag(const CVehicle* vehicle, int32 doorId, bool bCheckVehicleType);
    static int32 ComputeOppositeDoorFlag(const CVehicle* vehicle, int32 doorId, bool bCheckVehicleType);
    static int32 ComputePassengerIndexFromCarDoor(const CVehicle* vehicle, int32 doorId);
    static CPed* ComputeSlowJackedPed(const CVehicle* vehicle, int32 doorId);
    static int32 ComputeTargetDoorToEnterAsPassenger(const CVehicle* vehicle, int32 nPassengerNum);
    static int32 ComputeTargetDoorToExit(const CVehicle* vehicle, const CPed* ped);
    static bool GetNearestCarDoor(const CPed* ped, const CVehicle* vehicle, CVector& outPos, int32& doorId);
    static bool GetNearestCarPassengerDoor(const CPed* ped, const CVehicle* vehicle, CVector* outVec, int32* doorId, bool CheckIfOccupiedTandemSeat, bool CheckIfDoorIsEnterable, bool CheckIfRoomToGetIn);
    static CVector GetPositionToOpenCarDoor(const CVehicle* vehicle, int32 doorId);
    static bool IsCarDoorInUse(const CVehicle* vehicle, int32 firstDoorId, int32 secondDoorId);
    static bool IsCarDoorReady(const CVehicle* vehicle, int32 doorId);
    static bool IsCarQuickJackPossible(CVehicle* vehicle, int32 doorId, const CPed* ped);
    static bool IsCarSlowJackRequired(const CVehicle* vehicle, int32 doorId);
    static bool IsClearToDriveAway(const CVehicle* outVehicle);
    static bool IsPathToDoorBlockedByVehicleCollisionModel(const CPed* ped, const CVehicle* vehicle, const CVector& pos);
    static bool IsPedHealthy(CPed* vehicle);
    static bool IsPlayerToQuitCarEnter(const CPed* ped, const CVehicle* vehicle, int32 startTime, CTask* task);
    static bool IsRoomForPedToLeaveCar(const CVehicle* vehicle, int32 doorId, const CVector* pos = nullptr);
    static bool IsVehicleHealthy(const CVehicle* vehicle);
    static bool IsVehicleStealable(const CVehicle* vehicle, const CPed* ped);
    static void MakeUndraggedDriverPedLeaveCar(const CVehicle* vehicle, const CPed* ped);
    static void MakeUndraggedPassengerPedsLeaveCar(const CVehicle* targetVehicle, const CPed* draggedPed, const CPed* ped);
    static void QuitEnteringCar(CPed* ped, CVehicle* vehicle, int32 doorId, bool bCarWasBeingJacked);
    static void RemoveCarSitAnim(const CPed* ped);
    static void RemoveGetInAnims(const CPed* ped);
    static void SetAnimOffsetForEnterOrExitVehicle();
    static bool SetPedInCarDirect(CPed* ped, CVehicle* vehicle, int32 seatNumber, bool bAsDriver);
};
