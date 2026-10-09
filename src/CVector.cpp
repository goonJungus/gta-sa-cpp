// CVector.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CVector/*.c

#include "CVector.h"
#include "CMatrix.h"
#include <cstdlib>
#include <cmath>

CVector::CVector(const CVector2D& v2, float z) {
    // No direct .c in src/CVector for this ctor; straightforward init.
    x = v2.x; y = v2.y; this->z = z;
}

CVector CVector::Random(float min, float max) {
    // No decomp .c found for Random; per plugin-sdk/gta-reversed convention
    // each component gets an independent random value in [min, max].
    const float range = max - min;
    return CVector(
        min + static_cast<float>(rand()) / RAND_MAX * range,
        min + static_cast<float>(rand()) / RAND_MAX * range,
        min + static_cast<float>(rand()) / RAND_MAX * range
    );
}

CVector CVector::Random(CVector min, CVector max) {
    // No decomp .c found for Random; per plugin-sdk/gta-reversed convention.
    return CVector(
        min.x + static_cast<float>(rand()) / RAND_MAX * (max.x - min.x),
        min.y + static_cast<float>(rand()) / RAND_MAX * (max.y - min.y),
        min.z + static_cast<float>(rand()) / RAND_MAX * (max.z - min.z)
    );
}

float CVector::Magnitude() const {
    // decomp: src/CVector/Magnitude_004082c0.c
    return std::sqrt(x*x + y*y + z*z);
}

float CVector::Magnitude2D() const {
    // decomp: src/CVector/Magnitude2D_00406d50.c
    return std::sqrt(x*x + y*y);
}

void CVector::Normalise() {
    // decomp: src/CVector/Normalise_0059c910.c
    // Note the Ghidra condition `fVar1 < 0.0 != (fVar1 == 0.0)` reduces to
    // (magSq == 0.0) since a sum of squares is never negative.
    // Zero-magnitude case sets ONLY x = 1.0 (y/z untouched), per the binary.
    const float magSq = x*x + y*y + z*z;
    if (magSq == 0.0f) {
        x = 1.0f;
        return;
    }
    const float invMag = 1.0f / std::sqrt(magSq);
    x *= invMag; y *= invMag; z *= invMag;
}

float CVector::NormaliseAndMag() {
    // decomp: src/CVector/NormaliseAndMag_0059c970.c
    // Same zero-magnitude quirk as Normalise; returns 1.0 in that case,
    // otherwise returns 1.0/invMag (== magnitude, kept as reciprocal
    // to match the binary's float behavior).
    const float magSq = x*x + y*y + z*z;
    if (magSq == 0.0f) {
        x = 1.0f;
        return 1.0f;
    }
    const float invMag = 1.0f / std::sqrt(magSq);
    x *= invMag; y *= invMag; z *= invMag;
    return 1.0f / invMag;
}

float CVector::Dot(const CVector& o) const {
    return x*o.x + y*o.y + z*o.z;
}

float CVector::Dot2D(const CVector& o) const {
    return x*o.x + y*o.y;
}

CVector CVector::Cross(const CVector& other) const {
    // Returns new vector (NOTSA helper; no decomp .c, standard cross product)
    return CVector(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

void CVector::Cross_OG(const CVector& a, const CVector& b) {
    // decomp: src/CVector/Cross_OG_0070f890.c (addr 0x70F890, in-place)
    x = a.y * b.z - a.z * b.y;
    y = a.z * b.x - a.x * b.z;
    z = a.x * b.y - a.y * b.x;
}

void CVector::Sum(const CVector& left, const CVector& right) {
    // decomp: src/CVector/Sum_0040fdd0.c
    x = left.x + right.x;
    y = left.y + right.y;
    z = left.z + right.z;
}

void CVector::Difference(const CVector& left, const CVector& right) {
    // decomp: src/CVector/Difference_0040fe00.c
    x = left.x - right.x;
    y = left.y - right.y;
    z = left.z - right.z;
}

void CVector::operator+=(const CVector& right) {
    x += right.x; y += right.y; z += right.z;
}

void CVector::operator-=(const CVector& right) {
    x -= right.x; y -= right.y; z -= right.z;
}

void CVector::operator*=(const CVector& right) {
    x *= right.x; y *= right.y; z *= right.z;
}

void CVector::operator*=(float multiplier) {
    x *= multiplier; y *= multiplier; z *= multiplier;
}

void CVector::operator/=(float divisor) {
    x /= divisor; y /= divisor; z /= divisor;
}

void CVector::FromMultiply(const CMatrix& matrix, const CVector& vector) {
    // decomp: src/CVector/FromMultiply_0059c670.c
    // (matrix * vector, including translation). Public getters used since
    // CMatrix's basis vectors are private; evaluation order mirrors the .c.
    x = matrix.GetRight().x   * vector.x + matrix.GetForward().x * vector.y +
        matrix.GetUp().x      * vector.z + matrix.GetPosition().x;
    y = matrix.GetForward().y * vector.y + matrix.GetRight().y   * vector.x +
        matrix.GetUp().y      * vector.z + matrix.GetPosition().y;
    z = matrix.GetForward().z * vector.y + matrix.GetRight().z   * vector.x +
        matrix.GetUp().z      * vector.z + matrix.GetPosition().z;
}

void CVector::FromMultiply3x3(const CMatrix& matrix, const CVector& vector) {
    // decomp: src/CVector/FromMultiply3x3_0059c6d0.c
    // (matrix * vector, rotation only, no translation)
    x = matrix.GetRight().x   * vector.x + matrix.GetForward().x * vector.y +
        matrix.GetUp().x      * vector.z;
    y = matrix.GetForward().y * vector.y + matrix.GetRight().y   * vector.x +
        matrix.GetUp().y      * vector.z;
    z = matrix.GetForward().z * vector.y + matrix.GetRight().z   * vector.x +
        matrix.GetUp().z      * vector.z;
}

float CVector::Heading(bool reMapRangeTo0To2Pi) const {
    // No decomp .c found for Heading; per plugin-sdk/gta-reversed convention.
    constexpr float TWO_PI = 2.0f * 3.14159265f;
    const float h = std::atan2(-x, y);
    if (reMapRangeTo0To2Pi && h < 0.0f)
        return h + TWO_PI;
    return h;
}
