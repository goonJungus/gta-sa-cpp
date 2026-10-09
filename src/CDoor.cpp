// CDoor.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CDoor/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CDoor.h"

bool CDoor::Process(CVehicle* vehicle, const CVector& oldMoveSpeed, const CVector& oldTurnSpeed, const CVector& offset) {    // TODO: decomp src/CDoor/*.c
    (void)vehicle;
    (void)oldMoveSpeed;
    (void)oldTurnSpeed;
    (void)offset;
    return false;
}

bool CDoor::ProcessImpact(CVehicle* vehicle, const CVector& oldMoveSpeed, const CVector& oldTurnSpeed, const CVector& offset) {    // TODO: decomp src/CDoor/*.c
    (void)vehicle;
    (void)oldMoveSpeed;
    (void)oldTurnSpeed;
    (void)offset;
    return false;
}

void CDoor::Open(float angRatio) {    // TODO: decomp src/CDoor/*.c
    (void)angRatio;
}

float CDoor::GetAngleOpenRatio() const {    // TODO: decomp src/CDoor/*.c
    return 0.0f;
}

bool CDoor::IsClosed() const {    // TODO: decomp src/CDoor/*.c
    return false;
}

bool CDoor::IsFullyOpen() const {    // TODO: decomp src/CDoor/*.c
    return false;
}

void CDoor::UpdateFrameMatrix(CMatrix& mat) {    // TODO: decomp src/CDoor/*.c
    (void)mat;
}

CVector CDoor::GetRotation() const {    // TODO: decomp src/CDoor/*.c
    return {};
}
