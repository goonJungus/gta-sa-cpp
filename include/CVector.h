// CVector.h - GTA SA 1.0 clean-room C++ conversion
// Adapted from gta-reversed (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstddef>

// Minimal RenderWare vector replacement (avoids RW SDK dependency)
struct RwV3d {
    float x{}, y{}, z{};
};

class CVector2D; // forward decl
class CMatrix;   // forward decl

class CVector : public RwV3d {
public:
    constexpr CVector() = default;
    constexpr CVector(float X, float Y, float Z) : RwV3d{ X, Y, Z } {}
    constexpr CVector(RwV3d rwVec) { x = rwVec.x; y = rwVec.y; z = rwVec.z; }
    constexpr explicit CVector(float value) { x = y = z = value; }
    explicit CVector(const CVector2D& v2, float z = 0.f);

public:
    static CVector Random(float min, float max);
    static CVector Random(CVector min, CVector max);

    // Returns length of vector
    float Magnitude() const;

    // Returns length of 2d vector
    float Magnitude2D() const;

    // Normalises a vector in-place
    void Normalise();

    // Normalises a vector and returns length (in-place)
    float NormaliseAndMag();

    // Get a normalized copy of this vector
    CVector Normalized(float* outMag = nullptr) const {
        CVector cpy = *this;
        const float mag = cpy.NormaliseAndMag();
        if (outMag) {
            *outMag = mag;
        }
        return cpy;
    }

    // Dot product
    float Dot(const CVector& o) const;

    // 2D dot product
    float Dot2D(const CVector& o) const;

    // Cross product (returns new vector)
    CVector Cross(const CVector& other) const;

    // Original in-place cross (addr 0x70F890)
    void Cross_OG(const CVector& a, const CVector& b);

    // Adds left + right and stores result
    void Sum(const CVector& left, const CVector& right);

    // Subtracts left - right and stores result
    void Difference(const CVector& left, const CVector& right);

    CVector& operator=(const RwV3d& right) {
        x = right.x;
        y = right.y;
        z = right.z;
        return *this;
    }
    CVector& operator=(const CVector& right) {
        x = right.x;
        y = right.y;
        z = right.z;
        return *this;
    }
    void operator+=(const CVector& right);
    void operator-=(const CVector& right);
    void operator*=(const CVector& right);
    void operator*=(float multiplier);
    void operator/=(float divisor);

    // matrix * vector multiplication
    void FromMultiply(const CMatrix& matrix, const CVector& vector);
    void FromMultiply3x3(const CMatrix& matrix, const CVector& vector);

    inline void Set(float X, float Y, float Z) {
        x = X;
        y = Y;
        z = Z;
    }

    void Reset() {
        Set(0.f, 0.f, 0.f);
    }

    inline float ComponentwiseSum() const {
        return x + y + z;
    }

    inline float SquaredMagnitude() const {
        return x * x + y * y + z * z;
    }

    inline float SquaredMagnitude2D() const {
        return x * x + y * y;
    }

    inline bool IsZero() const {
        return x == 0.0F && y == 0.0F && z == 0.0F;
    }

    float operator[](size_t i) const {
        return (&x)[i];
    }

    float& operator[](size_t i) {
        return (&x)[i];
    }

    // Unit Z axis vector (0,0,1)
    static CVector ZAxisVector() { return CVector{ 0.f, 0.f, 1.f }; }

    float Heading(bool reMapRangeTo0To2Pi = false) const;

    friend constexpr CVector operator*(const CVector& vec, float multiplier) {
        return { vec.x * multiplier, vec.y * multiplier, vec.z * multiplier };
    }
};

// Free operators
constexpr inline CVector operator-(const CVector& vecOne, const CVector& vecTwo) {
    return { vecOne.x - vecTwo.x, vecOne.y - vecTwo.y, vecOne.z - vecTwo.z };
}

constexpr inline CVector operator+(const CVector& vecOne, const CVector& vecTwo) {
    return { vecOne.x + vecTwo.x, vecOne.y + vecTwo.y, vecOne.z + vecTwo.z };
}

constexpr inline CVector operator*(const CVector& vecOne, const CVector& vecTwo) {
    return { vecOne.x * vecTwo.x, vecOne.y * vecTwo.y, vecOne.z * vecTwo.z };
}

constexpr inline bool operator!=(const CVector& vecOne, const CVector& vecTwo) {
    return vecOne.x != vecTwo.x || vecOne.y != vecTwo.y || vecOne.z != vecTwo.z;
}

constexpr inline bool operator!=(const CVector& vec, float notEqualTo) {
    return vec.x != notEqualTo || vec.y != notEqualTo || vec.z != notEqualTo;
}

constexpr inline bool operator==(const CVector& vec, float equalTo) {
    return vec.x == equalTo && vec.y == equalTo && vec.z == equalTo;
}

constexpr inline bool operator==(const CVector& vecLeft, const CVector& vecRight) {
    return vecLeft.x == vecRight.x && vecLeft.y == vecRight.y && vecLeft.z == vecRight.z;
}

constexpr inline CVector operator/(const CVector& vec, float dividend) {
    return { vec.x / dividend, vec.y / dividend, vec.z / dividend };
}

constexpr inline CVector operator*(float multiplier, const CVector& vec) {
    return { vec.x * multiplier, vec.y * multiplier, vec.z * multiplier };
}

constexpr inline CVector operator-(const CVector& vec) {
    return { -vec.x, -vec.y, -vec.z };
}

inline float DistanceBetweenPoints(const CVector& pointOne, const CVector& pointTwo) {
    return (pointTwo - pointOne).Magnitude();
}

inline float DistanceBetweenPointsSquared(const CVector& pointOne, const CVector& pointTwo) {
    return (pointTwo - pointOne).SquaredMagnitude();
}

// 2D vector (minimal)
class CVector2D {
public:
    float x{}, y{};
    constexpr CVector2D() = default;
    constexpr CVector2D(float X, float Y) : x{X}, y{Y} {}
};
