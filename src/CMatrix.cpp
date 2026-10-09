// CMatrix.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CMatrix/*.c

#include "CMatrix.h"
#include <cmath>
#include <utility>   // swap

// RenderWare matrix helpers (RW SDK). These are declared here (not in the
// header) so this TU links against whatever RW layer the engine provides.
// Binary behavior: RwMatrixDestroy frees/destroys the matrix; RwMatrixUpdate
// recomputes the RwMatrix's derived fields after the basis vectors change.
extern void RwMatrixDestroy(RwMatrix* matrix);
extern void RwMatrixUpdate(RwMatrix* matrix);

// Euler-order index bytes, read from gta_sa.exe .rdata @ 0x866D94:
//   0x866D94: 01 02 00 01 | 0x866D98: 00 00 00 00 | 0x866D9C: 00 01 02 00
// The decomp names three overlapping "tables" in this blob:
//   B @ 0x866D94, indexed by (e + i)          -> bytes[0..3] = {1, 2, 0, 1}
//   C @ 0x866D95, indexed by (i - e)          -> bytes[1..3] = {2, 0, 1}
//   A @ 0x866D9C, indexed by (uiFlags >> 3)&3 -> bytes[8..11] = {0, 1, 2, 0}
// IMPORTANT: the binary computes (i - e) as UNSIGNED, so for i=0,e=1
// (ORDER_XZY) it wraps to 0xFFFFFFFF and reads the byte BEFORE C,
// i.e. B[0]. The helper below replicates that exact wraparound.
// (Not present in types/globals.json; values verified against the binary.)
static constexpr uint8_t s_EulerTableBytes[12] =
    { 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00 };

// Resolves the (i, e, j, k) index tuple exactly as the binary does.
static void EulerIndices(uint32_t uiFlags, uint32_t& i, uint32_t& e, uint32_t& j, uint32_t& k) {
    i = s_EulerTableBytes[8 + ((uiFlags >> 3) & 3)]; // A @ 0x866D9C
    e = (uiFlags >> 2) & 1;
    j = s_EulerTableBytes[e + i];                    // B @ 0x866D94
    // C @ 0x866D95 with unsigned-wraparound read for (i - e) < 0
    k = s_EulerTableBytes[(e > i) ? 0u : (1u + i - e)];
}

// Gimbal-lock epsilon used by ConvertToEulerAngles (2^-19)
static constexpr float EULER_EPS = 1.9073486e-06f;

CMatrix::CMatrix(const CMatrix& matrix) {
    // decomp: src/CMatrix/CMatrix_0059bcf0.c
    // Zeroes the attach fields, then copies the first 0x40 bytes (the matrix
    // itself, including flags/pads). Does NOT adopt the other's attach matrix.
    m_pAttachMatrix = nullptr;
    m_bOwnsAttachedMatrix = false;
    CopyOnlyMatrix(matrix);
}

CMatrix::CMatrix(RwMatrix* matrix, bool temporary) {
    // decomp: src/CMatrix/CMatrix_0059c050.c
    // Note: the binary calls Attach unconditionally (no null check).
    m_pAttachMatrix = nullptr;
    Attach(matrix, temporary);
}

CMatrix::~CMatrix() {
    // decomp: src/CMatrix/_dtor_CMatrix_0059acd0.c
    if (m_bOwnsAttachedMatrix && m_pAttachMatrix)
        RwMatrixDestroy(m_pAttachMatrix);
}

void CMatrix::Attach(RwMatrix* matrix, bool bOwnsMatrix) {
    // decomp: src/CMatrix/Attach_0059bd10.c
    // Destroys the previously attached matrix if owned, adopts the new one,
    // and copies its basis vectors in (RW up -> m_forward, RW at -> m_up).
    // Note: no null check on `matrix`, matching the binary.
    if (m_pAttachMatrix && m_bOwnsAttachedMatrix)
        RwMatrixDestroy(m_pAttachMatrix);
    m_pAttachMatrix = matrix;
    m_bOwnsAttachedMatrix = bOwnsMatrix;
    m_right.x = matrix->right.x;   m_right.y = matrix->right.y;   m_right.z = matrix->right.z;
    m_forward.x = matrix->up.x;    m_forward.y = matrix->up.y;    m_forward.z = matrix->up.z;
    m_up.x = matrix->at.x;         m_up.y = matrix->at.y;         m_up.z = matrix->at.z;
    m_pos.x = matrix->pos.x;       m_pos.y = matrix->pos.y;       m_pos.z = matrix->pos.z;
}

void CMatrix::Detach() {
    // decomp: src/CMatrix/Detach_0059acf0.c
    // Note: the binary does NOT clear m_bOwnsAttachedMatrix here.
    if (m_pAttachMatrix && m_bOwnsAttachedMatrix)
        RwMatrixDestroy(m_pAttachMatrix);
    m_pAttachMatrix = nullptr;
}

void CMatrix::CopyOnlyMatrix(const CMatrix& matrix) {
    // decomp: src/CMatrix/CopyOnlyMatrix_0059add0.c
    // Copies the 0x40-byte matrix block, i.e. the four vectors AND the
    // flags/pad dwords interleaved between them. Does not touch attach state.
    m_right   = matrix.m_right;
    flags     = matrix.flags;
    m_forward = matrix.m_forward;
    pad1      = matrix.pad1;
    m_up      = matrix.m_up;
    pad2      = matrix.pad2;
    m_pos     = matrix.m_pos;
    pad3      = matrix.pad3;
}

void CMatrix::Update() {
    // decomp: src/CMatrix/Update_0059bb60.c
    // Pulls the attached RwMatrix's basis vectors into this CMatrix.
    // Note: no null check on m_pAttachMatrix, matching the binary.
    RwMatrix* m = m_pAttachMatrix;
    m_right.x = m->right.x;   m_right.y = m->right.y;   m_right.z = m->right.z;
    m_forward.x = m->up.x;    m_forward.y = m->up.y;    m_forward.z = m->up.z;
    m_up.x = m->at.x;         m_up.y = m->at.y;         m_up.z = m->at.z;
    m_pos.x = m->pos.x;       m_pos.y = m->pos.y;       m_pos.z = m->pos.z;
}

void CMatrix::UpdateRW() {
    // decomp: src/CMatrix/UpdateRW_0059bbb0.c
    // Pushes this CMatrix's basis vectors out to the attached RwMatrix.
    if (m_pAttachMatrix)
        UpdateRwMatrix(m_pAttachMatrix);
}

void CMatrix::UpdateRwMatrix(RwMatrix* matrix) const {
    // decomp: src/CMatrix/UpdateRwMatrix_0059ad70.c
    // Note: no null check, matching the binary.
    matrix->right.x = m_right.x;     matrix->right.y = m_right.y;     matrix->right.z = m_right.z;
    matrix->up.x = m_forward.x;      matrix->up.y = m_forward.y;      matrix->up.z = m_forward.z;
    matrix->at.x = m_up.x;           matrix->at.y = m_up.y;           matrix->at.z = m_up.z;
    matrix->pos.x = m_pos.x;         matrix->pos.y = m_pos.y;         matrix->pos.z = m_pos.z;
    RwMatrixUpdate(matrix);
}

void CMatrix::UpdateMatrix(RwMatrix* rwMatrix) {
    // decomp: src/CMatrix/UpdateMatrix_0059ad20.c
    // Same direction as Update() but takes the RwMatrix explicitly.
    m_right.x = rwMatrix->right.x;   m_right.y = rwMatrix->right.y;   m_right.z = rwMatrix->right.z;
    m_forward.x = rwMatrix->up.x;    m_forward.y = rwMatrix->up.y;    m_forward.z = rwMatrix->up.z;
    m_up.x = rwMatrix->at.x;         m_up.y = rwMatrix->at.y;         m_up.z = rwMatrix->at.z;
    m_pos.x = rwMatrix->pos.x;       m_pos.y = rwMatrix->pos.y;       m_pos.z = rwMatrix->pos.z;
}

void CMatrix::SetUnity() {
    // decomp: src/CMatrix/SetUnity_0059ae70.c
    m_right.Set(1.0f, 0.0f, 0.0f);
    m_forward.Set(0.0f, 1.0f, 0.0f);
    m_up.Set(0.0f, 0.0f, 1.0f);
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

void CMatrix::ResetOrientation() {
    // decomp: src/CMatrix/ResetOrientation_0059aea0.c
    // Resets only the orientation axes; position is preserved.
    m_right.Set(1.0f, 0.0f, 0.0f);
    m_forward.Set(0.0f, 1.0f, 0.0f);
    m_up.Set(0.0f, 0.0f, 1.0f);
}

void CMatrix::SetScale(float scale) {
    // decomp: src/CMatrix/SetScale_0059aed0.c
    // Overwrites with a scaled identity (not a multiply); zeroes position.
    m_right.Set(scale, 0.0f, 0.0f);
    m_forward.Set(0.0f, scale, 0.0f);
    m_up.Set(0.0f, 0.0f, scale);
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

void CMatrix::SetScale(float x, float y, float z) {
    // decomp: src/CMatrix/SetScale_0059af00.c
    m_right.Set(x, 0.0f, 0.0f);
    m_forward.Set(0.0f, y, 0.0f);
    m_up.Set(0.0f, 0.0f, z);
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

void CMatrix::SetTranslateOnly(CVector translation) {
    // decomp: src/CMatrix/SetTranslateOnly_0059af80.c
    m_pos = translation;
}

void CMatrix::SetTranslate(CVector translation) {
    // decomp: src/CMatrix/SetTranslate_0059af40.c
    m_right.Set(1.0f, 0.0f, 0.0f);
    m_forward.Set(0.0f, 1.0f, 0.0f);
    m_up.Set(0.0f, 0.0f, 1.0f);
    m_pos = translation;
}

void CMatrix::SetRotateXOnly(float angle) {
    // decomp: src/CMatrix/SetRotateXOnly_0059afa0.c
    // Sets the rotation part only; position untouched.
    // The binary computes sin/cos in x87 80-bit (float10); use double here.
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    m_right.Set(1.0f, 0.0f, 0.0f);
    m_forward.x = 0.0f;
    m_up.x = 0.0f;
    m_forward.y = static_cast<float>(c);
    m_forward.z = static_cast<float>(s);
    m_up.y = static_cast<float>(-s);
    m_up.z = static_cast<float>(c);
}

void CMatrix::SetRotateYOnly(float angle) {
    // decomp: src/CMatrix/SetRotateYOnly_0059afe0.c
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    m_right.y = 0.0f;
    m_forward.Set(0.0f, 1.0f, 0.0f);
    m_up.y = 0.0f;
    m_right.x = static_cast<float>(c);
    m_right.z = static_cast<float>(-s);
    m_up.x = static_cast<float>(s);
    m_up.z = static_cast<float>(c);
}

void CMatrix::SetRotateZOnly(float angle) {
    // decomp: src/CMatrix/SetRotateZOnly_0059b020.c
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    m_right.z = 0.0f;
    m_forward.z = 0.0f;
    m_up.x = 0.0f;
    m_up.y = 0.0f;
    m_up.z = 1.0f;
    m_right.x = static_cast<float>(c);
    m_right.y = static_cast<float>(s);
    m_forward.x = static_cast<float>(-s);
    m_forward.y = static_cast<float>(c);
}

void CMatrix::SetRotateX(float angle) {
    // decomp: src/CMatrix/SetRotateX_0059b060.c
    // Same as SetRotateXOnly but also zeroes the position.
    SetRotateXOnly(angle);
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

void CMatrix::SetRotateY(float angle) {
    // decomp: src/CMatrix/SetRotateY_0059b0a0.c
    SetRotateYOnly(angle);
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

void CMatrix::SetRotateZ(float angle) {
    // decomp: src/CMatrix/SetRotateZ_0059b0e0.c
    SetRotateZOnly(angle);
    m_pos.Set(0.0f, 0.0f, 0.0f);
}

void CMatrix::SetRotate(float x, float y, float z) {
    // decomp: src/CMatrix/SetRotate_0059b120.c
    // Builds the basis from ZYX-style euler angles; zeroes position.
    const double cx = std::cos(static_cast<double>(x));
    const double sx = std::sin(static_cast<double>(x));
    const double cy = std::cos(static_cast<double>(y));
    const double sy = std::sin(static_cast<double>(y));
    const double cz = std::cos(static_cast<double>(z));
    const double sz = std::sin(static_cast<double>(z));
    m_pos.Set(0.0f, 0.0f, 0.0f);
    m_right.x = static_cast<float>(cz * cy - sz * sx * sy);
    m_right.y = static_cast<float>(sz * cy + cz * sx * sy);
    m_right.z = static_cast<float>(-(sy * cx));
    m_forward.x = static_cast<float>(-(sz * cx));
    m_forward.y = static_cast<float>(cz * cx);
    m_forward.z = static_cast<float>(sx);
    m_up.x = static_cast<float>(sz * sx * cy + cz * sy);
    m_up.y = static_cast<float>(sz * sy - cz * sx * cy);
    m_up.z = static_cast<float>(cy * cx);
}

// Rotates the Y/Z components of a vector: y' = c*y - s*z, z' = c*z + s*y
static void RotateYZ(CVector& v, double c, double s) {
    const float y = v.y;
    v.y = static_cast<float>(c * v.y - s * v.z);
    v.z = static_cast<float>(c * v.z + s * y);
}

// Rotates the X/Z components of a vector: x' = c*x + s*z, z' = c*z - s*x
static void RotateXZ(CVector& v, double c, double s) {
    const float x = v.x;
    v.x = static_cast<float>(c * v.x + s * v.z);
    v.z = static_cast<float>(c * v.z - s * x);
}

// Rotates the X/Y components of a vector: x' = c*x - s*y, y' = s*x + c*y
static void RotateXY(CVector& v, double c, double s) {
    const float x = v.x;
    v.x = static_cast<float>(c * v.x - s * v.y);
    v.y = static_cast<float>(s * x + c * v.y);
}

void CMatrix::RotateX(float angle, bool bKeepPos) {
    // decomp: src/CMatrix/RotateX_0059b1e0.c
    // The binary rotates all four vectors (including position); there is no
    // bKeepPos parameter in the decompiled function, so it is emulated by
    // restoring the position afterwards.
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    const CVector savedPos = m_pos;
    RotateYZ(m_right, c, s);
    RotateYZ(m_forward, c, s);
    RotateYZ(m_up, c, s);
    RotateYZ(m_pos, c, s);
    if (bKeepPos)
        m_pos = savedPos;
}

void CMatrix::RotateY(float angle, bool bKeepPos) {
    // decomp: src/CMatrix/RotateY_0059b2c0.c
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    const CVector savedPos = m_pos;
    RotateXZ(m_right, c, s);
    RotateXZ(m_forward, c, s);
    RotateXZ(m_up, c, s);
    RotateXZ(m_pos, c, s);
    if (bKeepPos)
        m_pos = savedPos;
}

void CMatrix::RotateZ(float angle, bool bKeepPos) {
    // decomp: src/CMatrix/RotateZ_0059b390.c
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    const CVector savedPos = m_pos;
    RotateXY(m_right, c, s);
    RotateXY(m_forward, c, s);
    RotateXY(m_up, c, s);
    RotateXY(m_pos, c, s);
    if (bKeepPos)
        m_pos = savedPos;
}

void CMatrix::Rotate(CVector rotation) {
    // decomp: src/CMatrix/Rotate_0059b460.c
    // Premultiplies all four vectors by the euler rotation matrix built from
    // (x, y, z) - the same matrix SetRotate(x, y, z) installs as the basis.
    const double cx = std::cos(static_cast<double>(rotation.x));
    const double sx = std::sin(static_cast<double>(rotation.x));
    const double cy = std::cos(static_cast<double>(rotation.y));
    const double sy = std::sin(static_cast<double>(rotation.y));
    const double cz = std::cos(static_cast<double>(rotation.z));
    const double sz = std::sin(static_cast<double>(rotation.z));

    const float r00 = static_cast<float>(cz * cy - sz * sx * sy);
    const float r01 = static_cast<float>(-(sz * cx));
    const float r02 = static_cast<float>(sz * sx * cy + cz * sy);
    const float r10 = static_cast<float>(sz * cy + cz * sx * sy);
    const float r11 = static_cast<float>(cz * cx);
    const float r12 = static_cast<float>(sz * sy - cz * sx * cy);
    const float r20 = static_cast<float>(-(sy * cx));
    const float r21 = static_cast<float>(sx);
    const float r22 = static_cast<float>(cy * cx);

    CVector* vecs[4] = { &m_right, &m_forward, &m_up, &m_pos };
    for (CVector* v : vecs) {
        const float x = v->x, y = v->y, z = v->z;
        v->x = r00 * x + r01 * y + r02 * z;
        v->y = r10 * x + r11 * y + r12 * z;
        v->z = r20 * x + r21 * y + r22 * z;
    }
}

void CMatrix::Reorthogonalise() {
    // decomp: src/CMatrix/Reorthogonalise_0059b6a0.c
    // up = normalize(right x forward); right = normalize(up x forward);
    // forward = right x up. All reads happen before writes (locals), so the
    // interleaved stores in the .c are safe to reorder.
    float ux = m_right.y * m_forward.z - m_forward.y * m_right.z;
    float uy = m_forward.x * m_right.z - m_right.x * m_forward.z;
    float uz = m_forward.y * m_right.x - m_forward.x * m_right.y;
    float inv = 1.0f / std::sqrt(ux * ux + uy * uy + uz * uz);
    ux *= inv; uy *= inv; uz *= inv;

    float rx = uz * m_forward.y - uy * m_forward.z;
    float ry = ux * m_forward.z - uz * m_forward.x;
    float rz = uy * m_forward.x - ux * m_forward.y;
    inv = 1.0f / std::sqrt(rx * rx + ry * ry + rz * rz);
    rx *= inv; ry *= inv; rz *= inv;

    m_forward.x = rz * uy - ry * uz;
    m_forward.y = rx * uz - rz * ux;
    m_forward.z = ry * ux - rx * uy;
    m_right.x = rx; m_right.y = ry; m_right.z = rz;
    m_up.x = ux;    m_up.y = uy;    m_up.z = uz;
}

void CMatrix::CopyToRwMatrix(RwMatrix* matrix) const {
    // decomp: src/CMatrix/CopyToRwMatrix_0059b8b0.c
    // Copies only the x/y/z components (no flags/pads), then RwMatrixUpdate.
    // Note: no null check, matching the binary.
    matrix->right.x = m_right.x;     matrix->right.y = m_right.y;     matrix->right.z = m_right.z;
    matrix->up.x = m_forward.x;      matrix->up.y = m_forward.y;      matrix->up.z = m_forward.z;
    matrix->at.x = m_up.x;           matrix->at.y = m_up.y;           matrix->at.z = m_up.z;
    matrix->pos.x = m_pos.x;         matrix->pos.y = m_pos.y;         matrix->pos.z = m_pos.z;
    RwMatrixUpdate(matrix);
}

void CMatrix::SetRotate(const CQuaternion& quat) {
    // decomp: src/CMatrix/SetRotate_0059bbf0.c (quaternion overload)
    const float x = quat.x, y = quat.y, z = quat.z, w = quat.w;
    const float y2 = y + y;
    const float z2 = z + z;
    const float x2x = (x + x) * x; // 2x^2
    const float x2w = (x + x) * w; // 2xw
    m_right.x   = 1.0f - (z2 * z + y2 * y);
    m_right.y   = y2 * x + z2 * w;
    m_right.z   = z2 * x - y2 * w;
    m_forward.x = y2 * x - z2 * w;
    m_forward.y = 1.0f - (z2 * z + x2x);
    m_forward.z = x2w + z2 * y;
    m_up.x      = y2 * w + z2 * x;
    m_up.y      = z2 * y - x2w;
    m_up.z      = 1.0f - (y2 * y + x2x);
}

void CMatrix::Scale(float scale) {
    // decomp: src/CMatrix/Scale_00459350.c
    // Scales the three basis vectors only (position untouched).
    // NOTE: this is Scale, not SetScale - the old stub wrongly called SetScale.
    m_right *= scale;
    m_forward *= scale;
    m_up *= scale;
}

void CMatrix::ForceUpVector(CVector vecUp) {
    // decomp: src/CMatrix/ForceUpVector_0059b7e0.c
    // right = vecUp x forward; forward = right x vecUp; up = vecUp.
    // The .c interleaves stores, but every store reads only locals or vecUp,
    // so plain sequential writes are equivalent.
    const float rx = vecUp.z * m_forward.y - vecUp.y * m_forward.z;
    const float ry = vecUp.x * m_forward.z - vecUp.z * m_forward.x;
    const float rz = vecUp.y * m_forward.x - vecUp.x * m_forward.y;
    m_right.x = rx; m_right.y = ry; m_right.z = rz;
    m_forward.x = rz * vecUp.y - ry * vecUp.z;
    m_forward.y = rx * vecUp.z - rz * vecUp.x;
    m_forward.z = ry * vecUp.x - rx * vecUp.y;
    m_up = vecUp;
}

void CMatrix::ConvertToEulerAngles(float* pX, float* pY, float* pZ, uint32_t uiFlags) {
    // decomp: src/CMatrix/ConvertToEulerAngles_0059a840.c
    // The .c's fpatan(A, B) maps to atan2f(A, B) (same argument order).
    const float m[9] = {
        m_right.x,   m_right.y,   m_right.z,
        m_forward.x, m_forward.y, m_forward.z,
        m_up.x,      m_up.y,      m_up.z
    };

    uint32_t i, e, j, k;
    EulerIndices(uiFlags, i, e, j, k);

    if ((uiFlags >> 1) & 1) { // EULER_ANGLES
        const int32_t i1 = static_cast<int32_t>(i * 3 + k);
        const int32_t i2 = static_cast<int32_t>(i * 3 + j);
        const float cy = std::sqrt(m[i1] * m[i1] + m[i2] * m[i2]);
        if (cy > EULER_EPS) {
            *pX = std::atan2(m[i2], m[i1]);
            *pY = std::atan2(cy, m[i * 4]);
            *pZ = std::atan2(m[i + j * 3], -m[i + k * 3]);
        } else { // gimbal lock
            *pX = std::atan2(-m[k + j * 3], m[j * 4]);
            *pY = std::atan2(cy, m[i * 4]);
            *pZ = 0.0f;
        }
    } else { // TAIT_BRYAN_ANGLES
        const int32_t i1 = static_cast<int32_t>(j * 3 + i);
        const float sy = std::sqrt(m[i * 4] * m[i * 4] + m[i1] * m[i1]);
        if (sy > EULER_EPS) {
            *pX = std::atan2(m[j + k * 3], m[k * 4]);
            *pY = std::atan2(-m[k * 3 + i], sy);
            *pZ = std::atan2(m[i1], m[i * 4]);
        } else { // gimbal lock
            *pX = std::atan2(-m[j * 3 + k], m[j * 4]);
            *pY = std::atan2(-m[i + k * 3], sy);
            *pZ = 0.0f;
        }
    }

    if (e == 1) {
        *pX = -*pX;
        *pY = -*pY;
        *pZ = -*pZ;
    }
    if (uiFlags & SWAP_XZ) {
        std::swap(*pX, *pZ);
    }
}

void CMatrix::ConvertFromEulerAngles(float x, float y, float z, uint32_t uiFlags) {
    // decomp: src/CMatrix/ConvertFromEulerAngles_0059aa40.c
    // The binary computes sin/cos in x87 80-bit (float10); double used here.
    float tz = z;
    uint32_t i, e, j, k;
    EulerIndices(uiFlags, i, e, j, k);

    if (uiFlags & SWAP_XZ) {
        z = x;
        x = tz;
    }

    double dx = x, dy = y, dz = z;
    if (e == 1) {
        dx = -dx;
        dy = -dy;
        dz = -dz;
    }
    const float cx = static_cast<float>(std::cos(dx));
    const float sx = static_cast<float>(std::sin(dx));
    const float cyy = static_cast<float>(std::cos(dy));
    const float syy = static_cast<float>(std::sin(dy));
    const float cz = static_cast<float>(std::cos(dz));
    const float sz = static_cast<float>(std::sin(dz));

    // Shared subexpressions (named after the .c's fVarN)
    const float f8 = cz * cx;        // cos(z)*cos(x)
    const float f6 = cx * sz;        // cos(x)*sin(z)
    const float f9 = sx * cz;        // sin(x)*cos(z)
    float f7 = sx * sz;              // sin(x)*sin(z); reassigned below in EULER branch

    float m[9];
    if ((uiFlags >> 1) & 1) { // EULER_ANGLES
        m[i * 4]     = cyy;
        m[i * 3 + j] = syy * sx;
        m[i * 3 + k] = syy * cx;
        m[j * 3 + i] = sz * syy;
        m[j * 4]     = f8 - f7 * cyy;
        m[j * 3 + k] = -(f6 * cyy) - f9;
        m[i + k * 3] = -(syy * cz);
        m[k * 3 + j] = f9 * cyy + f6;
        f7 = f8 * cyy - f7;
    } else { // TAIT_BRYAN_ANGLES
        m[i * 4]     = cz * cyy;
        m[i * 3 + j] = f9 * syy - f6;
        m[i * 3 + k] = f8 * syy + f7;
        m[j * 3 + i] = sz * cyy;
        m[j * 4]     = f7 * syy + f8;
        m[j * 3 + k] = f6 * syy - f9;
        m[i + k * 3] = -syy;
        m[k * 3 + j] = sx * cyy;
        f7 = cyy * cx;
    }
    m[k * 4] = f7;

    m_right.x = m[0];   m_right.y = m[1];   m_right.z = m[2];
    m_forward.x = m[3]; m_forward.y = m[4]; m_forward.z = m[5];
    m_up.x = m[6];      m_up.y = m[7];      m_up.z = m[8];
}

void CMatrix::operator=(const CMatrix& right) {
    // decomp: src/CMatrix/operator__0059bbc0.c
    // Copies the matrix block, then pushes it to the attached RwMatrix.
    RwMatrix* attach = m_pAttachMatrix;
    CopyOnlyMatrix(right);
    if (attach)
        UpdateRwMatrix(attach);
}

void CMatrix::operator+=(const CMatrix& right) {
    // decomp: src/CMatrix/operator___0059adf0.c
    // Component-wise add of the four vectors.
    m_right += right.m_right;
    m_forward += right.m_forward;
    m_up += right.m_up;
    m_pos += right.m_pos;
}

void CMatrix::operator*=(const CMatrix& right) {
    // decomp: src/CMatrix/Scale_0156ff00.c (relocated .HOODLUM body of
    // operator*=, original stub at 0x00411A80):
    //   tmp = operator*(*this, right); operator=(tmp); return *this;
    *this = operator*(*this, right);
}

CMatrix operator*(const CMatrix& a, const CMatrix& b) {
    // No direct .c for the free operator* in src/CMatrix; the 0x59C890
    // fragment (mislabeled Scale) computes the translation-included row
    // (out.x = a.x*b.x + a.fwd.x*b.y + a.up.x*b.z + a.pos.x), i.e. the
    // TransformPoint part. Basis rows follow the standard GTA convention:
    // out basis = a.TransformVector(b basis), out pos = a.TransformPoint(b pos).
    CMatrix out;
    out.m_right   = a.TransformVector(b.m_right);
    out.m_forward = a.TransformVector(b.m_forward);
    out.m_up      = a.TransformVector(b.m_up);
    out.m_pos     = a.TransformPoint(b.m_pos);
    return out;
}

CMatrix operator+(const CMatrix& a, const CMatrix& b) {
    // No decomp .c for the free operator+; component-wise add per convention.
    CMatrix out;
    out.m_right   = a.m_right + b.m_right;
    out.m_forward = a.m_forward + b.m_forward;
    out.m_up      = a.m_up + b.m_up;
    out.m_pos     = a.m_pos + b.m_pos;
    return out;
}

CMatrix& Invert(CMatrix& in, CMatrix& out) {
    // No decomp .c; delegates to the header's Inverted().
    out = in.Inverted();
    return out;
}

CMatrix Invert(const CMatrix& in) {
    return in.Inverted();
}

CMatrix Lerp(CMatrix from, CMatrix to, float t) {
    // No decomp .c for Lerp; per-vector linear interpolation.
    // (Lerp is not a friend, so this uses the public accessors.)
    CMatrix out;
    out.GetRight()    = from.GetRight()    + (to.GetRight()    - from.GetRight())    * t;
    out.GetForward()  = from.GetForward()  + (to.GetForward()  - from.GetForward())  * t;
    out.GetUp()       = from.GetUp()       + (to.GetUp()       - from.GetUp())       * t;
    out.GetPosition() = from.GetPosition() + (to.GetPosition() - from.GetPosition()) * t;
    return out;
}
