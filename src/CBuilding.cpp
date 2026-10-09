// CBuilding.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CBuilding (static world geometry).
// Adapted from gta-reversed/source/game_sa/Entity/Building.cpp and verified
// against the decomp (src/CBuilding/*.c).
//
// Hierarchy: CPlaceable -> CEntity -> CBuilding (NOT CPhysical - buildings
// have no physics state). Inherits all CEntity virtuals; no new virtuals.
//
// Clean-room notes:
// - operator new/delete use the global heap (TODO: building pool).

#include "CBuilding.h"

// 0x156CFD0
CBuilding::CBuilding() : CEntity() {
    SetTypeBuilding();
    SetUsesCollision(true);
}

// Pool not ported: global heap for now.
void* CBuilding::operator new(size_t size) {
    return ::operator new(size);
}

void CBuilding::operator delete(void* data) {
    ::operator delete(data);
}

// 0x403EC0
void CBuilding::ReplaceWithNewModel(int32_t newModelIndex) {
    DeleteRwObject();
    // TODO(port streaming): if (!CModelInfo::GetModelInfo(GetModelIndex())->m_nRefCount)
    //   CStreaming::RemoveModel(GetModelIndex());
    m_nModelIndex = static_cast<uint16_t>(newModelIndex);
}

// 0x4040E0
bool IsBuildingPointerValid(CBuilding* building) {
    // TODO(port pool): GetBuildingPool()->IsObjectValid(building).
    return building != nullptr;
}
