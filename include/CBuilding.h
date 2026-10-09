// CBuilding - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Building.h
// Static world geometry. NOTE: derives from CEntity directly, NOT from CPhysical
// (buildings have no physics state) - matches gta-reversed.
// Hierarchy: CPlaceable -> CEntity -> CBuilding
//
// Adaptations: stripped InjectHooks(), VALIDATE_SIZE, StaticRef (gBuildings counter
// moves to the building-pool module when ported).

#pragma once

#include "CEntity.h"

#include <cstdint>

class CBuilding : public CEntity {
public:
    CBuilding();
    static void* operator new(size_t size); // original used `unsigned`; size_t for portability
    static void operator delete(void* data);

public:
    void ReplaceWithNewModel(int32_t newModelIndex); // TODO: verify from decomp
};

// TODO: static_assert(sizeof(CBuilding) == 0x38) once CEntity layout is verified.

bool IsBuildingPointerValid(CBuilding* building); // TODO: verify from decomp
