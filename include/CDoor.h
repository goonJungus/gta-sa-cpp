// CDoor.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Door.h
// Vehicle door: open/close angles, axis, angular velocity.
// Hierarchy: standalone (member of CAutomobile/CBoat/CTrain)
//
// Adaptations:
//   stripped InjectHooks(), friend InjectHooksMain
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   plugin-sdk `Const` -> const
// TODO:
//   verify each method against decomp src/CDoor/*.c

#pragma once

#include <cstdint>

#include "GrTypes.h"

#include "CVector.h"
#include "CMatrix.h"

class CVehicle;



class CVehicle;

enum eDoorAxis : uint8 {
    DOOR_AXIS_X     = 0, // +X axis
    DOOR_AXIS_Y     = 1, // +Y axis
    DOOR_AXIS_Z     = 2, // +Z axis

    DOOR_AXIS_NEG_X = 3, // -X axis
    DOOR_AXIS_NEG_Y = 4, // -Y axis
    DOOR_AXIS_NEG_Z = 5, // -Z axis
};

enum eDoorExtraFlags : uint16 {
    DOOR_EXTRA_NONE        = 0x0,   // No door?

    // eDoorAxis

    DOOR_EXTRA_DIRN_MASK   = 0x0F,  // Masks out direction from m_nDirn (eDoorAxis)

    DOOR_EXTRA_BASED       = 0x10,  // Standard door locking behavior
    DOOR_EXTRA_LOW_GRAVITY = 0x20,  // Reduced gravity on door (used for bonnet)
    DOOR_EXTRA_CHASSIS     = 0x40,  // Chassis suspension door (swinging chassis)
    DOOR_EXTRA_FIXEDSTATE  = 0x80,  // Fixed state door (chassis doors)
    DOOR_EXTRA_FIRETRUCK   = 0x100, // Limit angular speed
};

enum eDoorState : uint8 {
    DOOR_NOTHING,
    DOOR_HIT_MAX_END,
    DOOR_HIT_MIN_END,
    DOOR_POP_OPEN,
    DOOR_SLAM_SHUT
};

class CDoor {
public:
    float m_openAngle{};
    float m_closedAngle{};

    uint16     m_dirn{}; // Def a bitfield. 2 x 4 bits + some more
    eDoorAxis  m_axis{}; // X, Y, Z - Right/left, front/back, up/down
    eDoorState m_doorState{};

    float m_angle{};
    float m_prevAngle{};
    float m_angVel{};    // Angular velocity

    inline static float DOOR_SPEED_MAX_CAPPED = 0.5f; // 0x8D3950

public:
    CDoor() = default;

    void Init(float open, float closed, eDoorAxis dirn, eDoorAxis axis, uint16 /*eDoorExtraFlags*/ extraFlags) {
        m_openAngle = open;  
        m_closedAngle = closed;
        m_axis = axis;  

        m_dirn = dirn | extraFlags;
    }

    bool Process(CVehicle* vehicle, const CVector& oldMoveSpeed, const CVector& oldTurnSpeed, const CVector& offset);
    bool ProcessImpact(CVehicle* vehicle, const CVector& oldMoveSpeed, const CVector& oldTurnSpeed, const CVector& offset);
    void Open(float angRatio);

    // There was a typo in the names of some functions
    // it has been corrected: `Ret` has been changed to `Get`

    float GetDoorAngVel() const { return m_angVel; } // RetDoorAngVel
    float GetDoorAngle() const { return m_angle; } // RetDoorAngle
    float GetAngleWhenClosed() const { return m_closedAngle; } // RetAngleWhenClosed
    float GetAngleWhenOpen() const { return m_openAngle; } // RetAngleWhenOpen
    float GetAngleOpenRatio() const;
    bool IsClosed() const;
    bool IsFullyOpen() const;

    eDoorAxis GetAxes() const { return m_axis; } // RetAxes
    eDoorState GetDoorState() const { return m_doorState; } // RetDoorState

    void SetExtraWheelPositions(float wheel1, float wheel2, float wheel3, float wheel4) {
        m_openAngle = wheel1;
        m_closedAngle = wheel2;
        m_angle = wheel3;
        m_prevAngle = wheel4;
    }

    void GetExtraWheelPositions(float& wheel1, float& wheel2, float& wheel3, float& wheel4) {
        wheel1 = m_openAngle;
        wheel2 = m_closedAngle;
        wheel3 = m_angle;
        wheel4 = m_prevAngle;
    }

    // NOTSA
    void UpdateFrameMatrix(CMatrix& mat);
    void OpenFully() { m_angle = m_openAngle; }
    void CloseFully() { m_angle = m_closedAngle; }

private:

    CVector GetRotation() const;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CDoor) == 0x18, "CDoor size mismatch");
#endif
