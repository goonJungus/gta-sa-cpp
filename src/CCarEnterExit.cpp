// CCarEnterExit.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CCarEnterExit/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CCarEnterExit.h"

// Static data members (game addresses recorded from gta-reversed StaticRef)
float CCarEnterExit::ms_fMaxSpeed_CanDragPedOut{}; // game address: 0x86F104
float CCarEnterExit::ms_fMaxSpeed_PlayerCanDragPedOut{}; // game address: 0x86F108
bool CCarEnterExit::ms_bPedOffsetsCalculated{}; // game address: 0xC18C20
CVector CCarEnterExit::ms_vecPedGetUpAnimOffset{}; // game address: 0xC18C3C
CVector CCarEnterExit::ms_vecPedBedLAnimOffset{}; // game address: 0xC18C54
CVector CCarEnterExit::ms_vecPedBedRAnimOffset{}; // game address: 0xC18C60
CVector CCarEnterExit::ms_vecPedDeskAnimOffset{}; // game address: 0xC18C6C
CVector CCarEnterExit::ms_vecPedChairAnimOffset{}; // game address: 0xC18C78
CVector CCarEnterExit::ms_vecPedQuickDraggedOutCarAnimOffset{}; // game address: 0xC18C48

void CCarEnterExit::AddInCarAnim(const CVehicle* vehicle, CPed* ped, bool bAsDriver) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)ped;
    (void)bAsDriver;
}

bool CCarEnterExit::CarHasDoorToClose(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return false;
}

bool CCarEnterExit::CarHasDoorToOpen(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return false;
}

bool CCarEnterExit::CarHasOpenableDoor(const CVehicle* vehicle, int32 doorId_UnusedArg, const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId_UnusedArg;
    (void)ped;
    return false;
}

bool CCarEnterExit::CarHasPartiallyOpenDoor(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return false;
}

int32 CCarEnterExit::ComputeDoorFlag(const CVehicle* vehicle, int32 doorId, bool bCheckVehicleType) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    (void)bCheckVehicleType;
    return 0;
}

int32 CCarEnterExit::ComputeOppositeDoorFlag(const CVehicle* vehicle, int32 doorId, bool bCheckVehicleType) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    (void)bCheckVehicleType;
    return 0;
}

int32 CCarEnterExit::ComputePassengerIndexFromCarDoor(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return 0;
}

CPed* CCarEnterExit::ComputeSlowJackedPed(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return nullptr;
}

int32 CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(const CVehicle* vehicle, int32 nPassengerNum) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)nPassengerNum;
    return 0;
}

int32 CCarEnterExit::ComputeTargetDoorToExit(const CVehicle* vehicle, const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)ped;
    return 0;
}

bool CCarEnterExit::GetNearestCarDoor(const CPed* ped, const CVehicle* vehicle, CVector& outPos, int32& doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
    (void)vehicle;
    (void)outPos;
    (void)doorId;
    return false;
}

bool CCarEnterExit::GetNearestCarPassengerDoor(const CPed* ped, const CVehicle* vehicle, CVector* outVec, int32* doorId, bool CheckIfOccupiedTandemSeat, bool CheckIfDoorIsEnterable, bool CheckIfRoomToGetIn) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
    (void)vehicle;
    (void)outVec;
    (void)doorId;
    (void)CheckIfOccupiedTandemSeat;
    (void)CheckIfDoorIsEnterable;
    (void)CheckIfRoomToGetIn;
    return false;
}

CVector CCarEnterExit::GetPositionToOpenCarDoor(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return {};
}

bool CCarEnterExit::IsCarDoorInUse(const CVehicle* vehicle, int32 firstDoorId, int32 secondDoorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)firstDoorId;
    (void)secondDoorId;
    return false;
}

bool CCarEnterExit::IsCarDoorReady(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return false;
}

bool CCarEnterExit::IsCarQuickJackPossible(CVehicle* vehicle, int32 doorId, const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    (void)ped;
    return false;
}

bool CCarEnterExit::IsCarSlowJackRequired(const CVehicle* vehicle, int32 doorId) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    return false;
}

bool CCarEnterExit::IsClearToDriveAway(const CVehicle* outVehicle) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)outVehicle;
    return false;
}

bool CCarEnterExit::IsPathToDoorBlockedByVehicleCollisionModel(const CPed* ped, const CVehicle* vehicle, const CVector& pos) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
    (void)vehicle;
    (void)pos;
    return false;
}

bool CCarEnterExit::IsPedHealthy(CPed* vehicle) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    return false;
}

bool CCarEnterExit::IsPlayerToQuitCarEnter(const CPed* ped, const CVehicle* vehicle, int32 startTime, CTask* task) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
    (void)vehicle;
    (void)startTime;
    (void)task;
    return false;
}

bool CCarEnterExit::IsRoomForPedToLeaveCar(const CVehicle* vehicle, int32 doorId, const CVector* pos) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)doorId;
    (void)pos;
    return false;
}

bool CCarEnterExit::IsVehicleHealthy(const CVehicle* vehicle) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    return false;
}

bool CCarEnterExit::IsVehicleStealable(const CVehicle* vehicle, const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)ped;
    return false;
}

void CCarEnterExit::MakeUndraggedDriverPedLeaveCar(const CVehicle* vehicle, const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)vehicle;
    (void)ped;
}

void CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(const CVehicle* targetVehicle, const CPed* draggedPed, const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)targetVehicle;
    (void)draggedPed;
    (void)ped;
}

void CCarEnterExit::QuitEnteringCar(CPed* ped, CVehicle* vehicle, int32 doorId, bool bCarWasBeingJacked) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
    (void)vehicle;
    (void)doorId;
    (void)bCarWasBeingJacked;
}

void CCarEnterExit::RemoveCarSitAnim(const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
}

void CCarEnterExit::RemoveGetInAnims(const CPed* ped) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
}

void CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle() {    // TODO: decomp src/CCarEnterExit/*.c
}

bool CCarEnterExit::SetPedInCarDirect(CPed* ped, CVehicle* vehicle, int32 seatNumber, bool bAsDriver) {    // TODO: decomp src/CCarEnterExit/*.c
    (void)ped;
    (void)vehicle;
    (void)seatNumber;
    (void)bAsDriver;
    return false;
}
