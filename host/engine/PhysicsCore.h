#pragma once
// Shared by CarPhysics.cpp and BikePhysics.cpp: CPhysical in game units (speeds per 1/50 s).

#include <cmath>
#include <cstring>

#include "Vehicle.h"

namespace physics {

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() = default;
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit V3(const float* p) : x(p[0]), y(p[1]), z(p[2]) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator/(float s) const { return {x / s, y / s, z / s}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    void To(float* p) const { p[0] = x; p[1] = y; p[2] = z; }
};
inline V3 operator*(float s, const V3& v) { return v * s; }
inline float Dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(const V3& a, const V3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Len2(const V3& v) { return Dot(v, v); }
inline float Len(const V3& v) { return std::sqrt(Dot(v, v)); }
inline V3 Norm(const V3& v) { const float l = Len(v); return l > 1e-9f ? v / l : V3(1, 0, 0); }

// CGeneral::GetRandomNumber for the tyre wobble and the burst-tyre sag.
inline unsigned g_physRng = 12345u;
inline int RandInt(int n) { g_physRng = g_physRng * 1664525u + 1013904223u; return n > 0 ? (int)((g_physRng >> 8) % (unsigned)n) : 0; }
inline float RandRange(float lo, float hi) { return lo + (hi - lo) * (float)RandInt(65536) / 65535.0f; }

// The car as CPhysical sees it for one step: its matrix, the speeds, mass and centre of mass.
struct Body {
    V3 right, fwd, up, pos;
    V3 mv, tv, fmv, ftv;
    float mass = 1500.0f, turnMass = 3000.0f;
    V3 com;   // car space
    V3 Rot(const V3& l) const { return right * l.x + fwd * l.y + up * l.z; }   // CMatrix::TransformVector
    V3 ComW() const { return Rot(com); }
    // Points are relative to the car's position, as retail passes them.
    void ApplyForce(const V3& f, const V3& p) { mv += f / mass; tv += Cross(p - ComW(), f) / turnMass; }
    void ApplyMoveForce(const V3& f) { mv += f / mass; }
    void ApplyTurnForce(const V3& f, const V3& p) { tv += Cross(p - ComW(), f) / turnMass; }
    void ApplyFrictionForce(const V3& f, const V3& p) { fmv += f / mass; ftv += Cross(p - ComW(), f) / turnMass; }
    V3 Speed(const V3& p) const { return Cross(tv + ftv, p - ComW()) + mv + fmv; }   // CPhysical::GetSpeed
    float MassAt(const V3& p, const V3& dir) const { return 1.0f / (Len2(Cross(p, dir)) / turnMass + 1.0f / mass); }   // GetMass(pos, dir)
};

inline Body Load(const VehicleInstance& c, const HandlingData& h) {
    Body b;
    b.right = V3(&c.rot[0]); b.fwd = V3(&c.rot[3]); b.up = V3(&c.rot[6]);
    b.pos = V3(c.x, c.y, c.z);
    b.mv = V3(c.phys.mv); b.tv = V3(c.phys.tv); b.fmv = V3(c.phys.fmv); b.ftv = V3(c.phys.ftv);
    b.mass = h.mass; b.turnMass = h.turnMass;
    b.com = V3(h.com);
    return b;
}

inline void Store(const Body& b, VehicleInstance& c) {
    b.right.To(&c.rot[0]); b.fwd.To(&c.rot[3]); b.up.To(&c.rot[6]);
    c.x = b.pos.x; c.y = b.pos.y; c.z = b.pos.z;
    b.mv.To(c.phys.mv); b.tv.To(c.phys.tv); b.fmv.To(c.phys.fmv); b.ftv.To(c.phys.ftv);
}

// Row-vector rotations (rows: right, forward, up), as CMatrix::SetRotateX / SetRotateY / SetRotateZ.
inline void RotX(float a, float m[16]) { const float c = std::cos(a), s = std::sin(a); const float r[16] = {1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1}; std::memcpy(m, r, sizeof(r)); }
inline void RotY(float a, float m[16]) { const float c = std::cos(a), s = std::sin(a); const float r[16] = {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1}; std::memcpy(m, r, sizeof(r)); }
inline void RotZ(float a, float m[16]) { const float c = std::cos(a), s = std::sin(a); const float r[16] = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; std::memcpy(m, r, sizeof(r)); }
inline void Mul4(const float* a, const float* b, float* o) {
    float t[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) t[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
    std::memcpy(o, t, sizeof(t));
}

}  // namespace physics
