// CMatrix.h - GTA SA 1.0 clean-room C++ conversion
// Adapted from gta-reversed (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CVector.h"
#include <cstdint>

// RwMatrix: canonical definition lives in RenderWare.h (deduped 2026-10-09;
// the duplicate here redefined it and was removed). Layout is identical
// (RwV3d rows; CVector adds no data members).
#include "RenderWare.h"

#include "CQuaternion.h" // canonical (deduped 2026-10-09)

enum eMatrixEulerFlags : uint32_t {
    SWAP_XZ = 0x01,

    TAIT_BRYAN_ANGLES = 0x0,
    EULER_ANGLES = 0x2,

    ORDER_XYZ = 0x00,
    ORDER_XZY = 0x04,
    ORDER_YZX = 0x08,
    ORDER_YXZ = 0x0C,
    ORDER_ZXY = 0x10,
    ORDER_ZYX = 0x14,
};

class CMatrix {
public:
    CMatrix() = default;

    CMatrix(const CVector& pos, const CVector& right, const CVector& fwd, const CVector& up) :
        m_right{right},
        m_forward{fwd},
        m_up{up},
        m_pos{pos}
    {
    }

    CMatrix(const CMatrix& matrix);
    CMatrix(RwMatrix* matrix, bool temporary = false);

    ~CMatrix();

    // Returns an identity matrix
    static CMatrix Unity() {
        CMatrix mat{};
        mat.SetUnity();
        return mat;
    }

// NOTE(2026-10-09): members public to match the original CMatrix layout;
// the decomp-converted .cpps access m_pos/m_up/m_right/m_forward directly.
public:
    CVector m_right;        // 0x0
    uint32_t flags;         // 0xC
    CVector m_forward;      // 0x10
    uint32_t pad1;          // 0x1C
    CVector m_up;           // 0x20
    uint32_t pad2;          // 0x2C
    CVector m_pos;          // 0x30
    uint32_t pad3;          // 0x3C

public:
    RwMatrix* m_pAttachMatrix{};       // 0x40
    bool      m_bOwnsAttachedMatrix{}; // 0x44

public:
    CVector& GetRight() { return m_right; }
    const CVector& GetRight() const { return m_right; }
    CVector GetLeft() const { return -m_right; }

    CVector& GetForward() { return m_forward; }
    const CVector& GetForward() const { return m_forward; }
    CVector GetBackward() const { return -m_forward; }

    CVector& GetUp() { return m_up; }
    const CVector& GetUp() const { return m_up; }
    CVector GetDown() const { return -m_up; }

    CVector& GetPosition() { return m_pos; }
    const CVector& GetPosition() const { return m_pos; }

    void Attach(RwMatrix* matrix, bool bOwnsMatrix);
    void Detach();
    void CopyOnlyMatrix(const CMatrix& matrix);
    void Update();
    void UpdateRW();
    void UpdateRwMatrix(RwMatrix* matrix) const;
    void UpdateMatrix(RwMatrix* rwMatrix);
    void SetUnity();
    void ResetOrientation();
    void SetScale(float scale);
    void SetScale(float x, float y, float z);
    void SetTranslateOnly(CVector translation);
    void SetTranslate(CVector translation);
    void SetRotateXOnly(float angle);
    void SetRotateYOnly(float angle);
    void SetRotateZOnly(float angle);
    void SetRotateX(float angle);
    void SetRotateY(float angle);
    void SetRotateZ(float angle);
    void SetRotate(float x, float y, float z);
    void SetRotate(const CVector& rot) { SetRotate(rot.x, rot.y, rot.z); }
    // gta-reversed CMatrix.h: SetRotateKeepPos (added 2026-10-09 for CAutomobile).
    // TODO(port): real implementation (sets rotation, preserves position).
    void SetRotateKeepPos(const CVector& rot) { SetRotate(rot); }
    void RotateX(float angle, bool bKeepPos = false);
    void RotateY(float angle, bool bKeepPos = false);
    void RotateZ(float angle, bool bKeepPos = false);
    void Rotate(CVector rotation);
    void Reorthogonalise();
    void CopyToRwMatrix(RwMatrix* matrix) const;
    void SetRotate(const CQuaternion& quat);
    void Scale(float scale);
    void ForceUpVector(CVector vecUp);
    void ConvertToEulerAngles(float* pX, float* pY, float* pZ, uint32_t uiFlags);
    void ConvertFromEulerAngles(float x, float y, float z, uint32_t uiFlags);

    // Inverse of this matrix
    CMatrix Inverted() const {
        CMatrix o;
        o.m_right   = CVector{ m_right.x, m_forward.x, m_up.x };
        o.m_forward = CVector{ m_right.y, m_forward.y, m_up.y };
        o.m_up      = CVector{ m_right.z, m_forward.z, m_up.z };
        o.m_pos     = -o.TransformVector(m_pos);
        return o;
    }

    // Transform a point (includes translation)
    CVector TransformPoint(CVector pt) const {
        return TransformVector(pt) + m_pos;
    }

    // Transform a direction vector (no translation)
    CVector TransformVector(CVector v) const {
        return v.x * m_right + v.y * m_forward + v.z * m_up;
    }

    // Inverse transform a point
    CVector InverseTransformPoint(CVector pt) const {
        return InverseTransformVector(pt - m_pos);
    }

    // Inverse transform a vector
    CVector InverseTransformVector(CVector v) const {
        return { m_right.Dot(v), m_forward.Dot(v), m_up.Dot(v) };
    }

    void operator=(const CMatrix& right);
    void operator+=(const CMatrix& right);
    void operator*=(const CMatrix& right);

    // Similar to Scale but also scales the position vector
    void ScaleAll(float mult) {
        Scale(mult);
        GetPosition() *= mult;
    }

private:
    friend CMatrix operator*(const CMatrix& a, const CMatrix& b);
    friend CMatrix operator+(const CMatrix& a, const CMatrix& b);
};

CMatrix operator*(const CMatrix& a, const CMatrix& b);
CMatrix operator+(const CMatrix& a, const CMatrix& b);

CMatrix& Invert(CMatrix& in, CMatrix& out);
CMatrix  Invert(const CMatrix& in);
CMatrix  Lerp(CMatrix from, CMatrix to, float t);
