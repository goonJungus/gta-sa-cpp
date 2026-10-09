// CQuaternion.h - canonical quaternion definition (deduped 2026-10-09)
// Was defined in both CMatrix.h and AnimTypes.h; canonical version from AnimTypes.h
// (has Slerp/Conjugated). CMatrix.h now includes this.
#pragma once

class CQuaternion {
public:
    float x{}, y{}, z{}, w{};

    constexpr CQuaternion() = default;
    constexpr CQuaternion(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}

    void Slerp(const CQuaternion& from, const CQuaternion& to, float theta, float invSinTheta, float t);

    constexpr CQuaternion Conjugated() const { return CQuaternion{ -x, -y, -z, w }; }

    constexpr CQuaternion operator*(float s) const { return CQuaternion{ x * s, y * s, z * s, w * s }; }
    CQuaternion& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
};
