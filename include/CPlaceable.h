// CPlaceable - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Placeable.h
// Base of the entity hierarchy: position/heading plus an optional full matrix.
//
// Hierarchy (must be preserved):
//   CPlaceable -> CEntity -> CPhysical -> CObject
//                                     -> CVehicle, CPed, ... (later subsystems)
//                        -> CBuilding
//                        -> CDummy
//
// Adaptations: stripped InjectHooks(), NOTSA_EXPORT_VTABLE, VALIDATE_SIZE
// (replaced with static_assert below).

#pragma once

#include "CSimpleTransform.h"
#include "CMatrixLink.h"

class CBox;   // defined when the collision subsystem is ported (used by CPlaceable.cpp)
class CRect;  // full definition in CRect.h (used by CPlaceable.cpp)

class CPlaceable {
public:
    CSimpleTransform m_placement;
    CMatrixLink*     m_matrix{};

public:
    CPlaceable();
    virtual ~CPlaceable();

    CMatrix& GetMatrix();

    static void ShutdownMatrixArray();
    static void InitMatrixArray();

    CVector GetRightVector();
    CVector GetForwardVector();
    CVector GetUpVector();

    void FreeStaticMatrix();
    void SetPosn(float x, float y, float z);
    void SetPosn(const CVector& posn);
    void SetOrientation(float x, float y, float z);
    void SetOrientation(CVector radians) { SetOrientation(radians.x, radians.y, radians.z); } // TODO: Replace method above with this
    void GetOrientation(float& x, float& y, float& z);
    void SetHeading(float heading);
    float GetHeading() const;
    float GetRoll() const;
    bool IsWithinArea(float x1, float y1, float x2, float y2) const;
    bool IsWithinArea(float x1, float y1, float z1, float x2, float y2, float z2) const;
    void RemoveMatrix();
    void AllocateStaticMatrix();
    void AllocateMatrix();
    void SetMatrix(CMatrix& matrix);

    // NOTSA
    bool IsPointInRange(const CVector& point, float range);
    bool IsEntityInRange(const CPlaceable* entity, float range) { return IsPointInRange(entity->GetPosition(), range); }
public:
    static constexpr uint32_t NUM_MATRICES_TO_CREATE = 900;

    inline CVector& GetRight() const { return m_matrix->GetRight(); }
    inline CVector& GetForward() const { return m_matrix->GetForward(); }
    inline CVector& GetUp() const { return m_matrix->GetUp(); }
    inline const CVector& GetPosition() const { return m_matrix ? m_matrix->GetPosition() : m_placement.m_vPosn; }
    inline CVector& GetPosition() { return m_matrix ? m_matrix->GetPosition() : m_placement.m_vPosn; }
    // Original built this via CVector2D(CVector); spelled out here since our
    // CVector2D has no CVector constructor yet.
    inline CVector2D GetPosition2D() { const auto& p = GetPosition(); return { p.x, p.y }; }
};

// Size checks below are for the 32-bit (Win32) target, matching the original binary.
// On 64-bit hosts pointers are 8 bytes, so these are skipped there.
#if INTPTR_MAX == INT32_MAX
// vtable(4) + CSimpleTransform(0x10) + CMatrixLink*(4) = 0x18, matches original VALIDATE_SIZE
static_assert(sizeof(CPlaceable) == 0x18, "CPlaceable layout changed");
#endif
