// CCollision - adapted from gta-reversed for clean-room C++ build
// Method bodies verified against src/CCollision/*.c and
// gta-reversed/source/game_sa/Collision/Collision.cpp.
// NOTE: the NOTSA helpers at the bottom (GetClosestPtOnLine, bary-coords, ...)
// have no decomp .c files; they were added by gta-reversed contributors.

#include "CCollision.h"
#include "CColLine.h"
#include "CColDisk.h"
#include "CColTriangle.h"
#include "CColTrianglePlane.h"
#include "CSurfaceInfos.h"
#include "CColStore.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

// Static member definitions.
// Original GTA SA 1.0 addresses (from gta-reversed StaticRef) kept as comments.
// TODO: re-resolve these for the clean-room build.
CLinkList<CCollisionData*> CCollision::ms_colModelCache{}; // 0x96592C
uint32_t CCollision::ms_iProcessLineNumCrossings = 0;      // 0x9655D0
uint32_t CCollision::ms_collisionInMemory = 0;             // 0x9655D4
bool  CCollision::bCamCollideWithVehicles = true;          // 0x8A5B14
bool  CCollision::bCamCollideWithObjects = true;           // 0x8A5B15
bool  CCollision::bCamCollideWithPeds = true;              // 0x8A5B17
bool  CCollision::bCamCollideWithBuildings = true;        // 0x8A5B16
float CCollision::relVelCamCollisionVehiclesSqr = 0.01f;   // 0x8A5B18
// s_DebugSettings: plain static (see header note) - value-initialized here.
CCollision::DebugSettings CCollision::s_DebugSettings{};

// Surface database (see CSurfaceInfos.h). The collision subsystem only reads
// the see-through / shoot-through flag bits.
CSurfaceInfos g_surfaceInfos{};

namespace {
// Surface gate shared by ProcessLineOfSight/TestLineOfSight.
// Matches the decomp branching (src/CCollision/ProcessLineOfSight_00417950.c):
// with doSeeThroughCheck, only NON-see-through primitives are tested;
// otherwise all primitives are tested except shoot-through ones when
// doShootThroughCheck is set.
bool ShouldTestSurface(eColSurfaceType material, bool doSeeThroughCheck, bool doShootThroughCheck) {
    const uint32_t id = static_cast<uint32_t>(material);
    if (doSeeThroughCheck)
        return !g_surfaceInfos.IsSeeThrough(id);
    return !doShootThroughCheck || !g_surfaceInfos.IsShootThrough(id);
}

// Decompress one collision vertex (1/128 fixed point).
CVector UncompressVert(const CompressedVector& v) {
    return CVector{ v.x, v.y, v.z };
}

// Shared plane-cross + projected 2D edge test used by TestLineTriangle and
// ProcessLineTriangle. Verbatim logic from
// src/CCollision/ProcessLineTriangle_004140f0.c and
// src/CCollision/TestLineTriangle_00413ac0.c.
bool ProcessLineTriangleInternal(const CColLine& line, const CompressedVector* verts,
                                 const CColTriangle& tri, const CColTrianglePlane& triPlane,
                                 float& outT, CVector& outNormal) {
    CVector normal;
    triPlane.GetNormal(&normal); // normal stored compressed x4096 (src/CColTrianglePlane/GetNormal_00411610.c)
    const float planeOffset = (float)triPlane.m_normalOffset * 0.0078125f; // decomp: short, x128 (src/CCollision/TestLineTriangle_00413ac0.c)

    const float startDist = line.m_vecStart.Dot(normal) - planeOffset;
    const float endDist = line.m_vecEnd.Dot(normal) - planeOffset;
    if (startDist * endDist >= 0.0f)
        return false; // the line must strictly cross the triangle plane

    const CVector dir = line.m_vecEnd - line.m_vecStart;
    const float t = -startDist / dir.Dot(normal);
    const CVector ip = line.m_vecStart + dir * t;

    const CVector a = UncompressVert(verts[tri.vA]);
    const CVector b = UncompressVert(verts[tri.vB]);
    const CVector c = UncompressVert(verts[tri.vC]);

    // Project onto the dominant plane. The NEG_* orientations swap the B and
    // C vertices to flip the winding (as the decomp's switch does).
    struct UV { float u, v; };
    UV aUV, bUV, cUV, pUV;
    switch (triPlane.m_orientation) {
    case CColTrianglePlane::POS_X:
        aUV = { a.y, a.z }; bUV = { b.y, b.z }; cUV = { c.y, c.z }; pUV = { ip.y, ip.z };
        break;
    case CColTrianglePlane::NEG_X:
        aUV = { a.y, a.z }; bUV = { c.y, c.z }; cUV = { b.y, b.z }; pUV = { ip.y, ip.z };
        break;
    case CColTrianglePlane::POS_Y:
        aUV = { a.z, a.x }; bUV = { b.z, b.x }; cUV = { c.z, c.x }; pUV = { ip.z, ip.x };
        break;
    case CColTrianglePlane::NEG_Y:
        aUV = { a.z, a.x }; bUV = { c.z, c.x }; cUV = { b.z, b.x }; pUV = { ip.z, ip.x };
        break;
    case CColTrianglePlane::POS_Z:
        aUV = { a.x, a.y }; bUV = { b.x, b.y }; cUV = { c.x, c.y }; pUV = { ip.x, ip.y };
        break;
    case CColTrianglePlane::NEG_Z:
        aUV = { a.x, a.y }; bUV = { c.x, c.y }; cUV = { b.x, b.y }; pUV = { ip.x, ip.y };
        break;
    default:
        return false;
    }

    const float e1 = (cUV.u - aUV.u) * (pUV.v - aUV.v) - (cUV.v - aUV.v) * (pUV.u - aUV.u);
    const float e2 = (bUV.u - aUV.u) * (pUV.v - aUV.v) - (bUV.v - aUV.v) * (pUV.u - aUV.u);
    const float e3 = (bUV.u - cUV.u) * (pUV.v - cUV.v) - (bUV.v - cUV.v) * (pUV.u - cUV.u);
    if (!(e1 >= 0.0f && e2 <= 0.0f && e3 >= 0.0f))
        return false;

    outT = t;
    outNormal = normal;
    return true;
}

// Closest point on triangle (a, b, c) to `center`, and its distance.
// Shared by TestSphereTriangle and ProcessSphereTriangle; the case analysis
// (3 edge tests -> vertex/edge/face regions) is verbatim from
// src/CCollision/TestSphereTriangle_004165b0.c.
bool SphereTriangleClosestPoint(const CVector& center,
                                const CVector& a, const CVector& b, const CVector& c,
                                const CVector& normal, float planeDist,
                                float& outDist, CVector& outClosest) {
    const CVector edgeAB = b - a;
    const float edgeLen = edgeAB.Magnitude();
    const CVector e = edgeAB * (1.0f / edgeLen);
    const CVector perp = e.Cross(normal); // in-plane perpendicular

    const CVector ac = c - a;
    const CVector ap = center - a;
    const float acE = ac.Dot(e);
    const float acP = ac.Dot(perp);
    const float pE = ap.Dot(e);
    const float pP = ap.Dot(perp);

    const bool t1 = pP * edgeLen >= 0.0f;
    const bool t2 = pE * acP - pP * acE >= 0.0f;
    const bool t3 = (acE - edgeLen) * pP - (pE - edgeLen) * acP >= 0.0f;
    const int hits = (t1 ? 1 : 0) + (t2 ? 1 : 0) + (t3 ? 1 : 0);

    if (hits == 3) {
        // Face region: closest point is the plane projection.
        outClosest = center - normal * planeDist;
        outDist = std::fabs(planeDist);
        return true;
    }
    if (hits == 0)
        return false;

    if (hits == 1) {
        // Vertex region.
        outClosest = t1 ? c : (t2 ? b : a);
        outDist = (center - outClosest).Magnitude();
        return true;
    }

    // Edge regions (two tests passed): project onto the edge, clamp to it.
    if (t1 && t2) {
        // Edge B-C.
        const CVector bc = c - b;
        const float bcLenSq = bc.SquaredMagnitude();
        const CVector bp = center - b;
        const float s = bp.Dot(bc) / bcLenSq;
        if (s <= 0.0f) {
            outClosest = b;
        } else if (s >= 1.0f) {
            outClosest = c;
        } else {
            outClosest = b + bc * s;
            const float inPlane = (pP * (acE - edgeLen) - (pE - edgeLen) * acP) / std::sqrt(bcLenSq);
            outDist = std::sqrt(planeDist * planeDist + inPlane * inPlane);
            return true;
        }
    } else if (t1) {
        // Edge A-C.
        const float acLenSq = acE * acE + acP * acP;
        const float s = (pE * acE + pP * acP) / acLenSq;
        if (s <= 0.0f) {
            outClosest = a;
        } else if (s >= 1.0f) {
            outClosest = c;
        } else {
            outClosest = a + ac * s;
            const float inPlane = (pP * acE - pE * acP) / std::sqrt(acLenSq);
            outDist = std::sqrt(planeDist * planeDist + inPlane * inPlane);
            return true;
        }
    } else {
        // Edge A-B.
        const float s = pE * edgeLen / (edgeLen * edgeLen);
        if (s <= 0.0f) {
            outClosest = a;
        } else if (s >= 1.0f) {
            outClosest = b;
        } else {
            outClosest = a + edgeAB * s;
            outDist = std::sqrt(planeDist * planeDist + pP * pP);
            return true;
        }
    }
    outDist = (center - outClosest).Magnitude();
    return true;
}
} // anonymous namespace

void CCollision::Tests(int32_t i) {
    // TODO: no named .c in src/CCollision/ - identify from unk_*.c / binary.
    // (gta-reversed has a unit-test harness here; the clean-room build has no
    // test infra for it yet.)
    (void)i;
}

// 0x416260 (src/CCollision/Init_00416260.c)
void CCollision::Init() {
    ms_colModelCache.Init(50);
    ms_collisionInMemory = 0;
    CColStore::Initialise();
}

// 0x4162E0 (src/CCollision/Shutdown_004162e0.c)
void CCollision::Shutdown() {
    // (Only the triangle-plane cache part of the original; the CColStore pool
    // and quadtree teardown in the binary belong to those subsystems.)
    for (auto i = ms_colModelCache.freeListTail.prev; i != &ms_colModelCache.usedListHead; i = i->prev) {
        if (i->data) {
            RemoveTrianglePlanes(i->data);
        }
    }
    ms_colModelCache.Shutdown();
    CColStore::Shutdown();
}

void CCollision::Update() {
    // 0x411E20 (src/CCollision/Update_00411e20.c): empty in the original.
}

void CCollision::SortOutCollisionAfterLoad() {
    // TODO: src/CCollision/SortOutCollisionAfterLoad_015678d0.c - the original
    // loads collision around TheCamera's position and flushes the streaming
    // queue. TheCamera is not converted yet, so the position source is missing.
    // CColStore::LoadCollision(pos, false); CStreaming::LoadAllRequestedModels(false);
}

// 0x411E70 (src/CCollision/TestSphereSphere_00411e70.c)
// NOTE: strict `<`, as in the binary (gta-reversed used `<=` here).
bool CCollision::TestSphereSphere(CColSphere const& sphere1, CColSphere const& sphere2) {
    const CVector d = sphere1.m_vecCenter - sphere2.m_vecCenter;
    const float r = sphere1.m_fRadius + sphere2.m_fRadius;
    return d.SquaredMagnitude() < r * r;
}

// 0x4120C0 (src/CCollision/TestSphereBox_004120c0.c)
// Tests if the box is fully inside the sphere.
bool CCollision::TestSphereBox(CSphere const& sphere, CBox const& box) {
    for (uint32_t i = 0; i < 3; i++) {
        if (sphere.m_vecCenter[i] + sphere.m_fRadius < box.m_vecMin[i] ||
            sphere.m_vecCenter[i] - sphere.m_fRadius > box.m_vecMax[i]) {
            return false;
        }
    }
    return true;
}

// 0x411EC0 - free function (no named .c; logic documented from gta-reversed,
// which annotates it as the original 0x411EC0 behavior)
void CalculateColPointInsideBox(CBox const& box, CVector const& point, CColPoint& colPoint) {
    const CVector center = (box.m_vecMin + box.m_vecMax) * 0.5f;
    const CVector pointToCenter = point - center;

    // Point's component-wise distance to each face.
    const CVector pointToClosest{
        pointToCenter.x <= 0.0f ? point.x - box.m_vecMin.x : box.m_vecMax.x - point.x,
        pointToCenter.y <= 0.0f ? point.y - box.m_vecMin.y : box.m_vecMax.y - point.y,
        pointToCenter.z <= 0.0f ? point.z - box.m_vecMin.z : box.m_vecMax.z - point.z,
    };

    colPoint = CColPoint{};
    colPoint.m_vecPoint = point;

    // Pick the axis with the SMALLEST distance to a face (shallowest exit);
    // ties go to x first, then y (matching the binary's chained comparisons).
    if (pointToClosest.x < pointToClosest.y && pointToClosest.x < pointToClosest.z) {
        colPoint.m_vecNormal = CVector{ pointToCenter.x <= 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
        colPoint.m_fDepth = pointToClosest.x;
    } else if (pointToClosest.y < pointToClosest.x && pointToClosest.y < pointToClosest.z) {
        colPoint.m_vecNormal = CVector{ 0.0f, pointToCenter.y <= 0.0f ? -1.0f : 1.0f, 0.0f };
        colPoint.m_fDepth = pointToClosest.y;
    } else {
        colPoint.m_vecNormal = CVector{ 0.0f, 0.0f, pointToCenter.z <= 0.0f ? -1.0f : 1.0f };
        colPoint.m_fDepth = pointToClosest.z;
    }
}

// 0x412130 (src/CCollision/ProcessSphereBox_00412130.c)
// (Behaviorally equivalent simplification of the original's 3x3x3 if-chain,
// as documented by gta-reversed.)
bool CCollision::ProcessSphereBox(CColSphere const& sph, CColBox const& box, CColPoint& colp, float& minDistSq) {
    if (!TestSphereBox(sph, box))
        return false;

    enum class ClosestCorner { INSIDE, MIN, MAX };
    ClosestCorner axes[3];
    for (int i = 0; i < 3; i++) {
        axes[i] = sph.m_vecCenter[i] < box.m_vecMin[i] ? ClosestCorner::MIN :
                  sph.m_vecCenter[i] > box.m_vecMax[i] ? ClosestCorner::MAX :
                  ClosestCorner::INSIDE;
    }

    if (axes[0] == ClosestCorner::INSIDE && axes[1] == ClosestCorner::INSIDE && axes[2] == ClosestCorner::INSIDE) {
        // Sphere center inside the box.
        CColPoint boxCP{};
        CalculateColPointInsideBox(box, sph.m_vecCenter, boxCP);

        colp.m_vecNormal     = boxCP.m_vecNormal;
        colp.m_vecPoint      = sph.m_vecCenter - boxCP.m_vecNormal * sph.m_fRadius;
        colp.m_fDepth        = boxCP.m_fDepth + sph.m_fRadius;

        colp.m_nSurfaceTypeA = sph.m_Surface.m_nMaterial;
        colp.m_nLightingA    = sph.m_Surface.m_nLighting;
        colp.m_nSurfaceTypeB = box.m_Surface.m_nMaterial;
        colp.m_nLightingB    = box.m_Surface.m_nLighting;

        minDistSq            = 0.0f; // the original sets it to 0 for inside hits
        return true;
    }

    // Sphere center outside on at least one axis: closest box point.
    const CVector p{
        axes[0] == ClosestCorner::MIN ? box.m_vecMin.x : axes[0] == ClosestCorner::MAX ? box.m_vecMax.x : sph.m_vecCenter.x,
        axes[1] == ClosestCorner::MIN ? box.m_vecMin.y : axes[1] == ClosestCorner::MAX ? box.m_vecMax.y : sph.m_vecCenter.y,
        axes[2] == ClosestCorner::MIN ? box.m_vecMin.z : axes[2] == ClosestCorner::MAX ? box.m_vecMax.z : sph.m_vecCenter.z,
    };
    const CVector dir = sph.m_vecCenter - p;
    const float distSq = dir.SquaredMagnitude();
    if (distSq < minDistSq) {
        const float dist = std::sqrt(distSq);
        if (dist >= sph.m_fRadius)
            return false;

        colp.m_vecNormal     = dir / dist;
        colp.m_vecPoint      = p;
        colp.m_fDepth        = sph.m_fRadius - dist;

        colp.m_nSurfaceTypeA = sph.m_Surface.m_nMaterial;
        colp.m_nLightingA    = sph.m_Surface.m_nLighting;
        colp.m_nSurfaceTypeB = box.m_Surface.m_nMaterial;
        colp.m_nLightingB    = box.m_Surface.m_nLighting;

        minDistSq            = distSq;
        return true;
    }
    return false;
}

// 0x412700 (src/CCollision/PointInTriangle_00412700.c)
bool __stdcall CCollision::PointInTriangle(CVector const& point, CVector const* triPoints) {
    // Make everything relative to the 0th vertex of the triangle.
    const CVector v1 = triPoints[1] - triPoints[0];
    const CVector v2 = triPoints[2] - triPoints[0];
    const CVector p  = point        - triPoints[0];

    // NOTE: no vectors are normalized, so all offset products are scaled;
    // the original compensates by multiplying with the squared magnitudes.
    const float v1_dot_v2 = v1.Dot(v2);

    const float v2_dot_p = v2.Dot(p);
    const float v2_magSq = v2.SquaredMagnitude();

    const float v1_dot_p = v1.Dot(p);
    const float v1_magSq = v1.SquaredMagnitude();

    const float a = (v2_magSq * v1_dot_p) - (v1_dot_v2 * v2_dot_p);
    if (a >= 0.0f) {
        const float b = (v1_magSq * v2_dot_p) - (v1_dot_v2 * v1_dot_p);
        if (b >= 0.0f) {
            const float c = (v1_magSq * v2_magSq) - (v1_dot_v2 * v1_dot_v2);
            return c >= (a + b);
        }
    }
    return false;
}

// 0x412850 (src/CCollision/DistToLineSqr_00412850.c)
float CCollision::DistToLineSqr(CVector const& lineStart, CVector const& lineEnd, CVector const& point) {
    // Make the line end and the point relative to lineStart (lineStart becomes
    // the space origin).
    const CVector l = lineEnd - lineStart;
    const CVector p = point - lineStart;

    const float ll = l.Dot(l); // line magnitude squared
    const float pl = p.Dot(l);

    if (pl <= 0.0f) // before the origin: distance to the origin
        return p.SquaredMagnitude();

    if (pl >= ll) // past the end: distance to the end
        return (p - l).SquaredMagnitude();

    // Pythagorean: a^2 = c^2 - b^2, with b^2 = pl^2 / ll (avoids a sqrt).
    const float cSq = p.Dot(p);
    const float bSq = pl * pl / ll;
    return cSq - bSq;
}

// 0x417610 (src/CCollision/DistToLine_00417610.c)
float CCollision::DistToLine(const CVector& lineStart, const CVector& lineEnd, const CVector& point) {
    return std::sqrt(DistToLineSqr(lineStart, lineEnd, point));
}

// 0x412970 (src/CCollision/DistToMathematicalLine_00412970.c)
// Like DistToLineSqr, but always measures to the projected point on the
// infinite line.
float CCollision::DistToMathematicalLine(CVector const* lineStart, CVector const* lineEnd, CVector const* point) {
    const CVector l = *lineEnd - *lineStart;
    const CVector p = *point - *lineStart;

    const float pMagSq = p.SquaredMagnitude();
    const float cSq = pMagSq;
    const float dp = p.Dot(l);
    const float bSq = pMagSq > 0.0f ? dp * dp / pMagSq : 0.0f; // guard against 0/0

    const float aSq = cSq - bSq;
    return aSq > 0.0f ? std::sqrt(aSq) : 0.0f;
}

// 0x412A30 (src/CCollision/DistToMathematicalLine2D_00412a30.c)
// 2D version. NOTE the original uses lineEnd directly, not (lineEnd -
// lineStart) - the quirk is preserved verbatim.
float CCollision::DistToMathematicalLine2D(float lineStartX, float lineStartY, float lineEndX, float lineEndY, float pointX, float pointY) {
    const float px = pointX - lineStartX;
    const float py = pointY - lineStartY;
    const float dot = px * lineEndX + py * lineEndY;
    const float distSq = px * px + py * py - dot * dot;
    return distSq > 0.0f ? std::sqrt(distSq) : 0.0f;
}

// 0x412A80 (src/CCollision/DistAlongLine2D_00412a80.c)
float CCollision::DistAlongLine2D(float lineX, float lineY, float lineDirX, float lineDirY, float pointX, float pointY) {
    return (pointX - lineX) * lineDirX + (pointY - lineY) * lineDirY;
}

// 0x412AA0 (src/CCollision/ProcessLineSphere_00412aa0.c)
bool CCollision::ProcessLineSphere(const CColLine& line, const CColSphere& sphere, CColPoint& colPoint, float& depth) {
    const CVector d = line.m_vecEnd - line.m_vecStart;
    const float a = d.SquaredMagnitude();
    const CVector m = sphere.m_vecCenter - line.m_vecStart;
    const float mdotd = m.Dot(d);
    const float discr = mdotd * mdotd - (m.SquaredMagnitude() - sphere.m_fRadius * sphere.m_fRadius) * a;
    if (discr >= 0.0f) {
        const float t = (mdotd - std::sqrt(discr)) / a;
        if (t >= 0.0f && t <= 1.0f && t < depth) {
            const CVector pt = line.m_vecStart + d * t;
            colPoint.m_vecPoint = pt;
            colPoint.m_vecNormal = (pt - sphere.m_vecCenter).Normalized();

            colPoint.m_nSurfaceTypeB = sphere.m_Surface.m_nMaterial;
            colPoint.m_nPieceTypeB   = sphere.m_Surface.m_nPiece;
            colPoint.m_nLightingB    = sphere.m_Surface.m_nLighting;
            colPoint.m_nSurfaceTypeA = static_cast<eColSurfaceType>(0); // SURFACE_DEFAULT
            colPoint.m_nPieceTypeA   = 0;
            // NOTE: the original does not set m_nLightingA here.

            depth = t;
            ms_iProcessLineNumCrossings += 2;
            return true;
        }
    }
    return false;
}

// 0x412C70 (src/CCollision/TestLineBox_DW_00412c70.c)
bool CCollision::TestLineBox_DW(const CColLine& line, const CBox& box) {
    const auto IsInBox = [&](const CVector& p) {
        // CBoundingBox::IsPointWithin is not converted yet; inclusive test.
        return p.x >= box.m_vecMin.x && p.x <= box.m_vecMax.x &&
               p.y >= box.m_vecMin.y && p.y <= box.m_vecMax.y &&
               p.z >= box.m_vecMin.z && p.z <= box.m_vecMax.z;
    };

    // Quick early exit if either line end is inside the box.
    if (IsInBox(line.m_vecStart) || IsInBox(line.m_vecEnd))
        return true;

    float x, y, z, t;

    // Check if the ends are on opposite sides of the min-x plane.
    if ((box.m_vecMin.x - line.m_vecEnd.x) * (box.m_vecMin.x - line.m_vecStart.x) < 0.0f) {
        t = (box.m_vecMin.x - line.m_vecStart.x) / (line.m_vecEnd.x - line.m_vecStart.x);
        y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
        if (y > box.m_vecMin.y && y < box.m_vecMax.y) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z)
                return true;
        }
    }

    // Max-x plane.
    if ((line.m_vecEnd.x - box.m_vecMax.x) * (line.m_vecStart.x - box.m_vecMax.x) < 0.0f) {
        t = (line.m_vecStart.x - box.m_vecMax.x) / (line.m_vecStart.x - line.m_vecEnd.x);
        y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
        if (y > box.m_vecMin.y && y < box.m_vecMax.y) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z)
                return true;
        }
    }

    // Min-y plane.
    if ((box.m_vecMin.y - line.m_vecStart.y) * (box.m_vecMin.y - line.m_vecEnd.y) < 0.0f) {
        t = (box.m_vecMin.y - line.m_vecStart.y) / (line.m_vecEnd.y - line.m_vecStart.y);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z)
                return true;
        }
    }

    // Max-y plane.
    if ((line.m_vecStart.y - box.m_vecMax.y) * (line.m_vecEnd.y - box.m_vecMax.y) < 0.0f) {
        t = (line.m_vecStart.y - box.m_vecMax.y) / (line.m_vecStart.y - line.m_vecEnd.y);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z)
                return true;
        }
    }

    // Min-z plane.
    if ((box.m_vecMin.z - line.m_vecStart.z) * (box.m_vecMin.z - line.m_vecEnd.z) < 0.0f) {
        t = (box.m_vecMin.z - line.m_vecStart.z) / (line.m_vecEnd.z - line.m_vecStart.z);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
            if (y > box.m_vecMin.y && y < box.m_vecMax.y)
                return true;
        }
    }

    // Max-z plane.
    if ((line.m_vecStart.z - box.m_vecMax.z) * (line.m_vecEnd.z - box.m_vecMax.z) < 0.0f) {
        t = (line.m_vecStart.z - box.m_vecMax.z) / (line.m_vecStart.z - line.m_vecEnd.z);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
            if (y > box.m_vecMin.y && y < box.m_vecMax.y)
                return true;
        }
    }

    return false;
}

// 0x413070 (no named .c - the binary's TestLineBox forwards to TestLineBox_DW)
bool CCollision::TestLineBox(const CColLine& line, const CBox& box) {
    return TestLineBox_DW(line, box);
}

// 0x413080 (src/CCollision/TestVerticalLineBox_00413080.c)
bool CCollision::TestVerticalLineBox(const CColLine& line, const CBox& box) {
    for (uint32_t i = 0; i < 2; i++) { // x and y axes
        if (line.m_vecStart[i] <= box.m_vecMin[i] || line.m_vecStart[i] >= box.m_vecMax[i])
            return false;
    }

    // The line may run top-to-bottom or bottom-to-top.
    const auto [minz, maxz] = std::minmax(line.m_vecStart.z, line.m_vecEnd.z);
    return minz <= box.m_vecMax.z && maxz >= box.m_vecMin.z;
}

// 0x413100 (src/CCollision/ProcessLineBox_00413100.c)
bool CCollision::ProcessLineBox(const CColLine& line, const CColBox& box, CColPoint& colPoint, float& maxTouchDistance) {
    float mint = 1.0f, t, x, y, z;
    CVector normal;
    CVector p;

    // Min-x plane.
    if ((box.m_vecMin.x - line.m_vecEnd.x) * (box.m_vecMin.x - line.m_vecStart.x) < 0.0f) {
        t = (box.m_vecMin.x - line.m_vecStart.x) / (line.m_vecEnd.x - line.m_vecStart.x);
        y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
        if (y > box.m_vecMin.y && y < box.m_vecMax.y) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z && t < mint) {
                mint = t;
                p = CVector{ box.m_vecMin.x, y, z };
                normal = CVector{ -1.0f, 0.0f, 0.0f };
            }
        }
    }

    // Max-x plane.
    if ((line.m_vecEnd.x - box.m_vecMax.x) * (line.m_vecStart.x - box.m_vecMax.x) < 0.0f) {
        t = (line.m_vecStart.x - box.m_vecMax.x) / (line.m_vecStart.x - line.m_vecEnd.x);
        y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
        if (y > box.m_vecMin.y && y < box.m_vecMax.y) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z && t < mint) {
                mint = t;
                p = CVector{ box.m_vecMax.x, y, z };
                normal = CVector{ 1.0f, 0.0f, 0.0f };
            }
        }
    }

    // Min-y plane.
    if ((box.m_vecMin.y - line.m_vecStart.y) * (box.m_vecMin.y - line.m_vecEnd.y) < 0.0f) {
        t = (box.m_vecMin.y - line.m_vecStart.y) / (line.m_vecEnd.y - line.m_vecStart.y);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z && t < mint) {
                mint = t;
                p = CVector{ x, box.m_vecMin.y, z };
                normal = CVector{ 0.0f, -1.0f, 0.0f };
            }
        }
    }

    // Max-y plane.
    if ((line.m_vecStart.y - box.m_vecMax.y) * (line.m_vecEnd.y - box.m_vecMax.y) < 0.0f) {
        t = (line.m_vecStart.y - box.m_vecMax.y) / (line.m_vecStart.y - line.m_vecEnd.y);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            z = line.m_vecStart.z + (line.m_vecEnd.z - line.m_vecStart.z) * t;
            if (z > box.m_vecMin.z && z < box.m_vecMax.z && t < mint) {
                mint = t;
                p = CVector{ x, box.m_vecMax.y, z };
                normal = CVector{ 0.0f, 1.0f, 0.0f };
            }
        }
    }

    // Min-z plane.
    if ((box.m_vecMin.z - line.m_vecStart.z) * (box.m_vecMin.z - line.m_vecEnd.z) < 0.0f) {
        t = (box.m_vecMin.z - line.m_vecStart.z) / (line.m_vecEnd.z - line.m_vecStart.z);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
            if (y > box.m_vecMin.y && y < box.m_vecMax.y && t < mint) {
                mint = t;
                p = CVector{ x, y, box.m_vecMin.z };
                normal = CVector{ 0.0f, 0.0f, -1.0f };
            }
        }
    }

    // Max-z plane.
    if ((line.m_vecStart.z - box.m_vecMax.z) * (line.m_vecEnd.z - box.m_vecMax.z) < 0.0f) {
        t = (line.m_vecStart.z - box.m_vecMax.z) / (line.m_vecStart.z - line.m_vecEnd.z);
        x = line.m_vecStart.x + (line.m_vecEnd.x - line.m_vecStart.x) * t;
        if (x > box.m_vecMin.x && x < box.m_vecMax.x) {
            y = line.m_vecStart.y + (line.m_vecEnd.y - line.m_vecStart.y) * t;
            if (y > box.m_vecMin.y && y < box.m_vecMax.y && t < mint) {
                mint = t;
                p = CVector{ x, y, box.m_vecMax.z };
                normal = CVector{ 0.0f, 0.0f, 1.0f };
            }
        }
    }

    if (mint >= maxTouchDistance)
        return false;

    colPoint.m_vecPoint  = p;
    colPoint.m_vecNormal = normal;

    colPoint.m_nSurfaceTypeA = static_cast<eColSurfaceType>(0); // SURFACE_DEFAULT
    colPoint.m_nLightingA    = tColLighting{ 0 };

    colPoint.m_nSurfaceTypeB = box.m_Surface.m_nMaterial;
    colPoint.m_nLightingB    = box.m_Surface.m_nLighting;

    maxTouchDistance = mint;
    return true;
}

// 0x4138D0 (src/CCollision/Test2DLineAgainst2DLine_004138d0.c)
bool CCollision::Test2DLineAgainst2DLine(float line1StartX, float line1StartY, float line1EndX, float line1EndY, float line2StartX, float line2StartY, float line2EndX, float line2EndY) {
    return ((line2StartX - line1StartX + line2EndX) * line1EndY - (line2StartY - line1StartY + line2EndY) * line1EndX)
         * ((line2StartX - line1StartX) * line1EndY - (line2StartY - line1StartY) * line1EndX) <= 0.0
        &&
           ((line1StartX - line2StartX + line1EndX) * line2EndY - (line1StartY - line2StartY + line1EndY) * line2EndX)
         * ((line1StartX - line2StartX) * line2EndY - (line1StartY - line2StartY) * line2EndX) <= 0.0;
}

// 0x413960 (src/CCollision/ProcessDiscCollision_00413960.c)
bool CCollision::ProcessDiscCollision(CColPoint& tempTriCol, const CMatrix& matBA, const CColDisk& disk, CColPoint& diskColPoint, bool& lineCollision, float& lineRatio, CColPoint& lineColPoint) {
    const CVector pt     = matBA.TransformPoint(tempTriCol.m_vecPoint);
    const CVector normal = matBA.TransformVector(tempTriCol.m_vecNormal);

    if (std::fabs(normal.Dot(disk.m_vThickness)) < 0.77f) {
        const CVector d = pt - disk.m_vecCenter;
        if (std::fabs(d.Dot(disk.m_vThickness)) < disk.m_fThickness) {
            const float z = std::sqrt(disk.m_fRadius * disk.m_fRadius - d.y * d.y - d.x * d.x) + pt.z;
            if (z < lineRatio)
                return false;
            lineCollision = true;
            lineRatio = z;
            // Decomp calls Hoodlum::unk_015637e0 here (struct copy into
            // lineColPoint); the explicit field write below is verbatim.
            lineColPoint = tempTriCol;
            lineColPoint.m_fDepth = tempTriCol.m_fDepth;
            return false;
        }
    }
    if (disk.m_Surface.m_nPiece < 0x11 && diskColPoint.m_fDepth < tempTriCol.m_fDepth) {
        // Decomp calls Hoodlum::unk_015637e0 here (struct copy into
        // diskColPoint); the explicit field writes below are verbatim.
        diskColPoint = tempTriCol;
        diskColPoint.m_fDepth = tempTriCol.m_fDepth;
        diskColPoint.m_nSurfaceTypeB = static_cast<eColSurfaceType>(60); // SURFACE_WHEELBASE
        return true;
    }
    return false;
}

// 0x413AC0 (src/CCollision/TestLineTriangle_00413ac0.c)
bool CCollision::TestLineTriangle(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& triPlane) {
    float t;
    CVector normal;
    return ProcessLineTriangleInternal(line, verts, tri, triPlane, t, normal);
}

// 0x4140F0 (src/CCollision/ProcessLineTriangle_004140f0.c)
bool CCollision::ProcessLineTriangle(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& triPlane, CColPoint& colPoint, float& maxTouchDistance, CStoredCollPoly* collPoly) {
    float t;
    CVector normal;
    if (!ProcessLineTriangleInternal(line, verts, tri, triPlane, t, normal))
        return false;
    if (t >= maxTouchDistance)
        return false;

    colPoint.m_vecPoint  = line.m_vecStart + (line.m_vecEnd - line.m_vecStart) * t;
    colPoint.m_vecNormal = normal;

    colPoint.m_nSurfaceTypeB = tri.m_nMaterial;
    colPoint.m_nPieceTypeB   = 0;
    colPoint.m_nLightingB    = tri.m_nLight;

    colPoint.m_nSurfaceTypeA = static_cast<eColSurfaceType>(0); // SURFACE_DEFAULT
    colPoint.m_nPieceTypeA   = 0;
    // NOTE: the original does not set m_nLightingA here.

    if (collPoly) {
        // Decomp: CColTrianglePlane::unk_00411590 decompresses each vertex.
        collPoly->verts[0] = UncompressVert(verts[tri.vA]);
        collPoly->verts[1] = UncompressVert(verts[tri.vB]);
        collPoly->verts[2] = UncompressVert(verts[tri.vC]);
        collPoly->valid    = true;
        collPoly->ligthing = tri.m_nLight;
    }

    maxTouchDistance = t;
    return true;
}

// 0x4147E0 (src/CCollision/ProcessVerticalLineTriangle_004147e0.c)
// Same as ProcessLineTriangle, plus an early-out on the triangle's x/y bounds
// (the line is vertical, so its x/y are constant).
bool CCollision::ProcessVerticalLineTriangle(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& triPlane, CColPoint& colPoint, float& maxTouchDistance, CStoredCollPoly* collPoly) {
    const CVector a = UncompressVert(verts[tri.vA]);
    const CVector b = UncompressVert(verts[tri.vB]);
    const CVector c = UncompressVert(verts[tri.vC]);

    const float lx = line.m_vecStart.x;
    if (a.x <= lx) {
        if (b.x < lx && c.x < lx)
            return false;
    } else if (lx < b.x && lx < c.x) {
        return false;
    }
    const float ly = line.m_vecStart.y;
    if (a.y <= ly) {
        if (b.y < ly && c.y < ly)
            return false;
    } else if (ly < b.y && ly < c.y) {
        return false;
    }

    return ProcessLineTriangle(line, verts, tri, triPlane, colPoint, maxTouchDistance, collPoly);
}

bool CCollision::IsStoredPolyStillValidVerticalLine(const CVector& lineOrigin, float lineDist, CColPoint& colPoint, CStoredCollPoly* collPoly) {
    // TODO: src/CCollision/IsStoredPolyStillValidVerticalLine_00414d70.c
    (void)lineOrigin; (void)lineDist; (void)colPoint; (void)collPoly;
    return false;
}

// 0x415230 (src/CCollision/GetBoundingBoxFromTwoSpheres_00415230.c)
// NOTE: the original uses spA's radius for both spheres' extents.
CColBox CCollision::GetBoundingBoxFromTwoSpheres(const CColSphere& spA, const CColSphere& spB) {
    CColBox box;
    const float r = spA.m_fRadius;
    box.m_vecMin.x = std::min(spA.m_vecCenter.x, spB.m_vecCenter.x) - r;
    box.m_vecMax.x = std::max(spA.m_vecCenter.x, spB.m_vecCenter.x) + r;
    box.m_vecMin.y = std::min(spA.m_vecCenter.y, spB.m_vecCenter.y) - r;
    box.m_vecMax.y = std::max(spA.m_vecCenter.y, spB.m_vecCenter.y) + r;
    box.m_vecMin.z = std::min(spA.m_vecCenter.z, spB.m_vecCenter.z) - r;
    box.m_vecMax.z = std::max(spA.m_vecCenter.z, spB.m_vecCenter.z) + r;
    return box;
}

bool CCollision::IsThisVehicleSittingOnMe(CVehicle* vehicle, CVehicle* vehicleOnMe) {
    // TODO: src/CCollision/IsThisVehicleSittingOnMe_01566da0.c - needs CVehicle.
    (void)vehicle; (void)vehicleOnMe;
    return false;
}

bool CCollision::CheckCameraCollisionPeds(int32_t sectorX, int32_t sectorY, const CVector& pos, const CVector& dir, float& /*unused*/) {
    // TODO: src/CCollision/CheckCameraCollisionPeds_00415320.c - needs ped pool.
    (void)sectorX; (void)sectorY; (void)pos; (void)dir;
    return false;
}

bool CCollision::CheckPeds(const CVector& src, const CVector& normal, float& nearest) {
    // TODO: src/CCollision/CheckPeds_01567650.c - needs ped pool.
    (void)src; (void)normal; (void)nearest;
    return false;
}

// 0x415590 (src/CCollision/SphereCastVsBBox_00415590.c)
bool CCollision::SphereCastVsBBox(const CColSphere& sphere1, const CColSphere& sphere2, const CColBox& box) {
    // Expand the box by sphere1's radius, then test the center sweep line.
    const float r = sphere1.m_fRadius;
    CBox expanded;
    expanded.m_vecMin = CVector{ box.m_vecMin.x - r, box.m_vecMin.y - r, box.m_vecMin.z - r };
    expanded.m_vecMax = CVector{ box.m_vecMax.x + r, box.m_vecMax.y + r, box.m_vecMax.z + r };
    const CColLine line{ sphere1.m_vecCenter, sphere2.m_vecCenter };
    return TestLineBox_DW(line, expanded);
}

// 0x415620 (src/CCollision/RayPolyPOP_00415620.c)
bool CCollision::RayPolyPOP(CVector* arg0, CVector* arg1, CColTriangle* arg2, CVector* arg3, CVector* arg4) {
    // arg0: ray origin (in/out: intersection point); arg1: ray direction;
    // arg3: plane normal; arg4: plane point. (arg2 is unused.)
    (void)arg2;
    const float d = (*arg4 - *arg0).Dot(*arg3);
    if (d > 0.0f)
        return false;
    const float denom = arg1->Dot(*arg3);
    if (d <= denom)
        return false;
    const float t = d / denom;
    *arg0 = *arg0 + *arg1 * t;
    return true;
}

// 0x4156D0 (src/CCollision/GetPrincipleAxis_004156d0.c)
int32_t CCollision::GetPrincipleAxis(const CVector& normal) {
    const float ax = std::fabs(normal.x);
    const float ay = std::fabs(normal.y);
    const float az = std::fabs(normal.z);
    if (ay < ax && az < ax)
        return 0;
    return ay <= az ? 4 : 2;
}

// 0x415730 (src/CCollision/PointInPoly_00415730.c)
bool CCollision::PointInPoly(const CVector& testPt, const CColTriangle& /*unused*/, const CVector& normal, const CVector* verts) {
    // Project onto the dominant plane and require all three edge tests to
    // agree. NOTE: the Y-dominant case uses inverted (<= 0 / < 0) comparisons.
    const int32_t axis = GetPrincipleAxis(normal);
    bool s1, s2, s3;
    if (axis == 0) {
        // Dominant X: test in (y, z).
        const float e1 = (verts[1].z - verts[0].z) * (testPt.y - verts[0].y) - (testPt.z - verts[0].z) * (verts[1].y - verts[0].y);
        const float e2 = (verts[2].z - verts[1].z) * (testPt.y - verts[1].y) - (verts[2].y - verts[1].y) * (testPt.z - verts[1].z);
        const float e3 = (verts[0].z - verts[2].z) * (testPt.y - verts[2].y) - (verts[0].y - verts[2].y) * (testPt.z - verts[2].z);
        s1 = e1 >= 0.0f; s2 = e2 >= 0.0f; s3 = e3 >= 0.0f;
    } else if (axis == 4) {
        // Dominant Z: test in (x, y).
        const float e1 = (verts[1].y - verts[0].y) * (testPt.x - verts[0].x) - (verts[1].x - verts[0].x) * (testPt.y - verts[0].y);
        const float e2 = (verts[2].y - verts[1].y) * (testPt.x - verts[1].x) - (verts[2].x - verts[1].x) * (testPt.y - verts[1].y);
        const float e3 = (verts[0].y - verts[2].y) * (testPt.x - verts[2].x) - (verts[0].x - verts[2].x) * (testPt.y - verts[2].y);
        s1 = e1 >= 0.0f; s2 = e2 >= 0.0f; s3 = e3 >= 0.0f;
    } else {
        // Dominant Y: test in (x, z) with inverted winding.
        const float e1 = (verts[1].z - verts[0].z) * (testPt.x - verts[0].x) - (verts[1].x - verts[0].x) * (testPt.z - verts[0].z);
        const float e2 = (testPt.x - verts[1].x) * (verts[2].z - verts[1].z) - (verts[2].x - verts[1].x) * (testPt.z - verts[1].z);
        const float e3 = (verts[0].z - verts[2].z) * (testPt.x - verts[2].x) - (verts[0].x - verts[2].x) * (testPt.z - verts[2].z);
        s1 = e1 <= 0.0f; s2 = e2 <= 0.0f; s3 = e3 < 0.0f;
    }
    return s1 == s2 && s1 == s3;
}

// 0x415950 (src/CCollision/Closest3_00415950.c)
// arg0: 3 candidate points; arg1: in/out - replaced by the nearest candidate.
void CCollision::Closest3(CVector* arg0, CVector* arg1) {
    const float d0 = (*arg1 - arg0[0]).SquaredMagnitude();
    const float d1 = (*arg1 - arg0[1]).SquaredMagnitude();
    const float d2 = (*arg1 - arg0[2]).SquaredMagnitude();
    if (d1 <= d0) {
        if (d1 < d2) {
            *arg1 = arg0[1];
            return;
        }
    } else if (d0 < d2) {
        *arg1 = arg0[0];
        return;
    }
    *arg1 = arg0[2];
}

// 0x415CF0 (src/CCollision/SphereCastVersusVsPoly_00415cf0.c)
bool CCollision::SphereCastVersusVsPoly(const CColSphere& sphere1, const CColSphere& sphere2, const CColTriangle& tri, const CColTrianglePlane& triPlane, CompressedVector* verts) {
    const CVector vel   = sphere2.m_vecCenter - sphere1.m_vecCenter;
    const float   velSq = vel.SquaredMagnitude();
    const CVector start = sphere1.m_vecCenter;
    const float   radius = sphere1.m_fRadius;

    CVector normal;
    triPlane.GetNormal(&normal); // normal stored compressed x4096 (src/CColTrianglePlane/GetNormal_00411610.c)
    const float planeDist = start.Dot(normal) - (float)triPlane.m_normalOffset * 0.0078125f;

    const CVector a = UncompressVert(verts[tri.vA]);

    CVector contact;
    if (std::fabs(planeDist) <= radius) {
        contact = start - normal * planeDist;
    } else {
        contact = start - normal * radius;
        const float dd = (a - contact).Dot(normal);
        const float velDotN = vel.Dot(normal);
        if (dd > 0.0f || dd <= velDotN)
            return false;
        contact = contact + vel * (dd / velDotN);
    }

    const CVector b = UncompressVert(verts[tri.vB]);
    const CVector c = UncompressVert(verts[tri.vC]);

    // The original passes stack copies of the uncompressed vertices; the
    // triangle argument is unused by PointInPoly.
    const CVector triVerts[3] = { a, b, c };
    if (PointInPoly(contact, tri, normal, triVerts))
        return true;

    const float radiusSq = radius * radius;
    if (radiusSq <= ClosestSquaredDistanceBetweenFiniteLines(start, a, b, vel, velSq))
        return false;
    if (radiusSq <= ClosestSquaredDistanceBetweenFiniteLines(start, c, b, vel, velSq))
        return false;
    return radiusSq > ClosestSquaredDistanceBetweenFiniteLines(start, a, c, vel, velSq);
}

// 0x416330 (src/CCollision/CalculateTrianglePlanes_00416330.c)
void CCollision::CalculateTrianglePlanes(CCollisionData* colData) {
    if (!colData->m_nNumTriangles) // no triangles => no planes to calculate
        return;

    if (colData->m_pTrianglePlanes) {
        // Planes already calculated: re-insert at the front (most-recently-used).
        ms_colModelCache.Insert(*colData->GetLinkPtr());
    } else {
        CLink<CCollisionData*>* l = ms_colModelCache.Insert(colData);
        if (!l) {
            // No free cache slot: evict the least-recently-used entry.
            CLink<CCollisionData*>* const lru = ms_colModelCache.GetTail();
            lru->data->RemoveTrianglePlanes();
            ms_colModelCache.Remove(lru);

            l = ms_colModelCache.Insert(colData);
            assert(l); // the original VERIFYs this; it must succeed now
        }
        colData->CalculateTrianglePlanes();
        colData->SetLinkPtr(l);
    }
}

// 0x416400 (src/CCollision/RemoveTrianglePlanes_00416400.c)
void CCollision::RemoveTrianglePlanes(CCollisionData* colData) {
    if (!colData->m_pTrianglePlanes)
        return;

    ms_colModelCache.Remove(colData->GetLinkPtr());
    colData->RemoveTrianglePlanes();
}

// 0x416450 (src/CCollision/ProcessSphereSphere_00416450.c)
bool CCollision::ProcessSphereSphere(const CColSphere& spA, const CColSphere& spB, CColPoint& colPoint, float& maxTouchDistance) {
    const CVector spBToA = spA.m_vecCenter - spB.m_vecCenter;
    const float distSq = spBToA.SquaredMagnitude();

    // Early-out without a sqrt (the original computed the sqrt first).
    const float rSum = spA.m_fRadius + spB.m_fRadius;
    if (distSq >= rSum * rSum)
        return false;

    const float touchDistUnclamped = std::sqrt(distSq) - spB.m_fRadius;
    const float touchDist = std::max(touchDistUnclamped, 0.0f);
    const float touchDistSq = touchDist * touchDist;

    if (touchDistSq >= maxTouchDistance)
        return false;
    if (touchDist >= spA.m_fRadius)
        return false;

    maxTouchDistance = touchDistSq;

    colPoint.m_vecNormal = spBToA.Normalized();
    colPoint.m_vecPoint  = spA.m_vecCenter - colPoint.m_vecNormal * touchDist;
    colPoint.m_fDepth    = spA.m_fRadius - touchDistUnclamped;

    colPoint.m_nSurfaceTypeA = spA.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeA   = spA.m_Surface.m_nPiece;
    colPoint.m_nLightingA    = spA.m_Surface.m_nLighting;

    colPoint.m_nSurfaceTypeB = spB.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeB   = spB.m_Surface.m_nPiece;
    colPoint.m_nLightingB    = spB.m_Surface.m_nLighting;

    return true;
}

// 0x4165B0 (src/CCollision/TestSphereTriangle_004165b0.c)
bool CCollision::TestSphereTriangle(const CColSphere& sphere, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& triPlane) {
    CVector normal;
    triPlane.GetNormal(&normal); // normal stored compressed x4096 (src/CColTrianglePlane/GetNormal_00411610.c)
    const float planeDist = sphere.m_vecCenter.Dot(normal) - (float)triPlane.m_normalOffset * 0.0078125f;
    if (std::fabs(planeDist) > sphere.m_fRadius)
        return false;

    float dist;
    CVector closest;
    if (!SphereTriangleClosestPoint(sphere.m_vecCenter,
                                    UncompressVert(verts[tri.vA]),
                                    UncompressVert(verts[tri.vB]),
                                    UncompressVert(verts[tri.vC]),
                                    normal, planeDist, dist, closest)) {
        return false;
    }
    return dist < sphere.m_fRadius;
}

// 0x416BA0 (src/CCollision/ProcessSphereTriangle_00416ba0.c)
bool CCollision::ProcessSphereTriangle(const CColSphere& sphere, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& triPlane, CColPoint& colPoint, float& maxTouchDistance) {
    CVector normal;
    triPlane.GetNormal(&normal); // normal stored compressed x4096 (src/CColTrianglePlane/GetNormal_00411610.c)
    const float planeDist = sphere.m_vecCenter.Dot(normal) - (float)triPlane.m_normalOffset * 0.0078125f;
    if (std::fabs(planeDist) > sphere.m_fRadius)
        return false;
    if (maxTouchDistance < planeDist * planeDist)
        return false;

    float dist;
    CVector closest;
    if (!SphereTriangleClosestPoint(sphere.m_vecCenter,
                                    UncompressVert(verts[tri.vA]),
                                    UncompressVert(verts[tri.vB]),
                                    UncompressVert(verts[tri.vC]),
                                    normal, planeDist, dist, closest)) {
        return false;
    }
    if (dist >= sphere.m_fRadius || dist * dist >= maxTouchDistance)
        return false;

    colPoint.m_vecPoint  = closest;
    colPoint.m_vecNormal = (sphere.m_vecCenter - closest).Normalized();

    colPoint.m_nSurfaceTypeA = sphere.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeA   = sphere.m_Surface.m_nPiece;
    colPoint.m_nLightingA    = sphere.m_Surface.m_nLighting;

    colPoint.m_nSurfaceTypeB = tri.m_nMaterial;
    colPoint.m_nPieceTypeB   = 0;
    // NOTE: the original does not set m_nLightingB here.

    colPoint.m_fDepth = sphere.m_fRadius - dist;
    maxTouchDistance  = dist * dist;
    return true;
}

// 0x417470 (src/CCollision/TestLineSphere_00417470.c)
bool CCollision::TestLineSphere(const CColLine& line, const CColSphere& sphere) {
    const CVector d = line.m_vecEnd - line.m_vecStart;
    const float len = d.Magnitude();
    const CVector m = sphere.m_vecCenter - line.m_vecStart;

    if (len < 1e-6f) // degenerate line: point-in-sphere test
        return m.SquaredMagnitude() <= sphere.m_fRadius * sphere.m_fRadius;

    const float mdotd = m.Dot(d);
    const float b = -2.0f * mdotd;
    const float lenSq = len * len;
    const float discr = b * b - 4.0f * (m.SquaredMagnitude() - sphere.m_fRadius * sphere.m_fRadius) * lenSq;
    if (discr < 0.0f)
        return false;
    const float t = (-b - std::sqrt(discr)) / (2.0f * lenSq);
    return t >= 0.0f && t < 1.0f;
}

// 0x417730 (body relocated to .HOODLUM: src/CCollision/TestLineOfSight_01564c40.c)
// NOTE: unlike the gta-reversed reimplementation, the binary does NOT test
// triangles here - only spheres and boxes. Verified against the decomp.
bool CCollision::TestLineOfSight(const CColLine& line, const CMatrix& transform, CColModel& colModel, bool doSeeThroughCheck, bool doShootThroughCheck) {
    CCollisionData* const colData = colModel.m_pColData;
    if (!colData)
        return false;

    // Transform the line into the model's object space.
    const CMatrix invTransform = Invert(transform);
    const CColLine lineOS{ invTransform.TransformPoint(line.m_vecStart),
                           invTransform.TransformPoint(line.m_vecEnd) };

    if (!TestLineBox_DW(lineOS, colModel.GetBoundingBox()))
        return false;

    for (uint16_t i = 0; i < colData->m_nNumSpheres; i++) {
        if (!ShouldTestSurface(colData->m_pSpheres[i].m_Surface.m_nMaterial, doSeeThroughCheck, doShootThroughCheck))
            continue;
        if (TestLineSphere(lineOS, colData->m_pSpheres[i]))
            return true;
    }

    for (uint16_t i = 0; i < colData->m_nNumBoxes; i++) {
        if (!ShouldTestSurface(colData->m_pBoxes[i].m_Surface.m_nMaterial, doSeeThroughCheck, doShootThroughCheck))
            continue;
        if (TestLineBox_DW(lineOS, colData->m_pBoxes[i]))
            return true;
    }

    return false;
}

// 0x417950 (src/CCollision/ProcessLineOfSight_00417950.c)
bool CCollision::ProcessLineOfSight(const CColLine& line, const CMatrix& transform, CColModel& colModel, CColPoint& colPoint, float& maxTouchDistance, bool doSeeThroughCheck, bool doShootThroughCheck) {
    CCollisionData* const colData = colModel.m_pColData;
    if (!colData)
        return false;

    // Transform the line into the model's object space. (The original keeps
    // the inverted matrix in a static with an atexit dtor hook; a local is
    // equivalent here.)
    const CMatrix invTransform = Invert(transform);
    const CColLine lineOS{ invTransform.TransformPoint(line.m_vecStart),
                           invTransform.TransformPoint(line.m_vecEnd) };

    if (!TestLineBox_DW(lineOS, colModel.GetBoundingBox()))
        return false;

    float touchDist = maxTouchDistance;

    for (uint16_t i = 0; i < colData->m_nNumSpheres; i++) {
        if (!ShouldTestSurface(colData->m_pSpheres[i].m_Surface.m_nMaterial, doSeeThroughCheck, doShootThroughCheck))
            continue;
        ProcessLineSphere(lineOS, colData->m_pSpheres[i], colPoint, touchDist);
    }

    for (uint16_t i = 0; i < colData->m_nNumBoxes; i++) {
        if (!ShouldTestSurface(colData->m_pBoxes[i].m_Surface.m_nMaterial, doSeeThroughCheck, doShootThroughCheck))
            continue;
        ProcessLineBox(lineOS, colData->m_pBoxes[i], colPoint, touchDist);
    }

    CalculateTrianglePlanes(colData);
    for (uint16_t i = 0; i < colData->m_nNumTriangles; i++) {
        if (!ShouldTestSurface(colData->m_pTriangles[i].m_nMaterial, doSeeThroughCheck, doShootThroughCheck))
            continue;
        if (ProcessLineTriangle(lineOS, colData->m_pVertices, colData->m_pTriangles[i],
                                colData->m_pTrianglePlanes[i], colPoint, touchDist, nullptr)) {
            ms_iProcessLineNumCrossings++;
        }
    }

    if (touchDist < maxTouchDistance) {
        // Transform the hit back to world space.
        colPoint.m_vecPoint  = transform.TransformPoint(colPoint.m_vecPoint);
        colPoint.m_vecNormal = transform.TransformVector(colPoint.m_vecNormal);
        maxTouchDistance = touchDist;
        return true;
    }
    return false;
}

// 0x417BF0 (src/CCollision/ProcessVerticalLine_00417bf0.c)
bool CCollision::ProcessVerticalLine(const CColLine& line, const CMatrix& transform, CColModel& colModel, CColPoint& colPoint, float& maxTouchDistance, bool doSeeThroughCheck, bool doShootThroughCheck, CStoredCollPoly* collPoly) {
    CCollisionData* const colData = colModel.m_pColData;
    if (!colData)
        return false;

    // Object-space line via the orthonormal inverse transform (the original
    // inlines the transpose math; InverseTransformPoint is identical).
    const CColLine lineOS{ transform.InverseTransformPoint(line.m_vecStart),
                           transform.InverseTransformPoint(line.m_vecEnd) };

    if (!TestLineBox_DW(lineOS, colModel.GetBoundingBox()))
        return false;

    float touchDist = maxTouchDistance;

    // NOTE: unlike ProcessLineOfSight, there is no shoot-through skip here -
    // only the see-through gate.
    const auto shouldTestVL = [&](eColSurfaceType material) {
        return !doSeeThroughCheck || !g_surfaceInfos.IsSeeThrough(static_cast<uint32_t>(material));
    };

    for (uint16_t i = 0; i < colData->m_nNumSpheres; i++) {
        if (!shouldTestVL(colData->m_pSpheres[i].m_Surface.m_nMaterial))
            continue;
        ProcessLineSphere(lineOS, colData->m_pSpheres[i], colPoint, touchDist);
    }

    for (uint16_t i = 0; i < colData->m_nNumBoxes; i++) {
        if (!shouldTestVL(colData->m_pBoxes[i].m_Surface.m_nMaterial))
            continue;
        ProcessLineBox(lineOS, colData->m_pBoxes[i], colPoint, touchDist);
    }

    CalculateTrianglePlanes(colData);

    // The original keeps the closest triangle's poly in a static
    // (DAT_009659fc); its `valid` field doubles as the DAT_00965a20 flag.
    static CStoredCollPoly s_triPoly{};
    s_triPoly.valid = false;
    for (uint16_t i = 0; i < colData->m_nNumTriangles; i++) {
        if (!shouldTestVL(colData->m_pTriangles[i].m_nMaterial))
            continue;
        ProcessLineTriangle(lineOS, colData->m_pVertices, colData->m_pTriangles[i],
                            colData->m_pTrianglePlanes[i], colPoint, touchDist, &s_triPoly);
    }

    if (touchDist < maxTouchDistance) {
        colPoint.m_vecPoint  = transform.TransformPoint(colPoint.m_vecPoint);
        colPoint.m_vecNormal = transform.TransformVector(colPoint.m_vecNormal);
        if (s_triPoly.valid && collPoly) {
            *collPoly = s_triPoly;
            for (CVector& v : collPoly->verts)
                v = transform.TransformPoint(v);
        }
        maxTouchDistance = touchDist;
        return true;
    }
    return false;
}

// 0x417F20 (src/CCollision/SphereCastVsSphere_00417f20.c)
bool CCollision::SphereCastVsSphere(const CColSphere& spA, const CColSphere& spB, const CColSphere& spS) {
    const CVector d = spA.m_vecCenter - spS.m_vecCenter;
    const float r = spS.m_fRadius + spA.m_fRadius;
    if (r * r <= d.SquaredMagnitude()) {
        if (TestSphereSphere(spB, spS))
            return true;
        // NOTE: the decomp shows an unresolved register for the line's second
        // endpoint; the sweep is from spA's center to spB's center, so the
        // line is (spA.center -> spB.center).
        const CColLine line{ spA.m_vecCenter, spB.m_vecCenter };
        CColSphere grown = spS;
        grown.m_fRadius += spA.m_fRadius;
        return TestLineSphere(line, grown);
    }
    return true;
}

// 0x417FD0 (src/CCollision/ClosestPointOnLine_00417fd0.c)
// NOTE: despite the parameter names, this computes the point on segment
// (l1, point) closest to l0 (see ClosestPointsOnPoly's call pattern).
void CCollision::ClosestPointOnLine(const CVector& l0, const CVector& l1, const CVector& point, CVector& closest) {
    const CVector v = point - l1;
    const float dist = v.Magnitude();
    const float proj = (v * (1.0f / dist)).Dot(l0 - l1);
    if (proj < 0.0f) {
        closest = l1;
    } else if (dist < proj) {
        closest = point;
    } else {
        closest = l1 + v * (1.0f / dist) * proj;
    }
}

// 0x418100 (src/CCollision/ClosestPointsOnPoly_00418100.c)
// arg1: triangle vertices; arg2: query point; arg3: out - closest point on
// each of the three edges.
void CCollision::ClosestPointsOnPoly(CColTriangle* arg0, CVector* arg1, CVector* arg2, CVector* arg3) {
    (void)arg0;
    ClosestPointOnLine(*arg2, arg1[1], arg1[0], arg3[0]);
    ClosestPointOnLine(*arg2, arg1[2], arg1[1], arg3[1]);
    ClosestPointOnLine(*arg2, arg1[0], arg1[2], arg3[2]);
}

// 0x418150 (src/CCollision/ClosestPointOnPoly_00418150.c)
// arg1: triangle vertices; arg2: in/out - replaced by the closest point on
// the triangle's edges.
void CCollision::ClosestPointOnPoly(CColTriangle* arg0, CVector* arg1, CVector* arg2) {
    CVector edgeClosest[3];
    ClosestPointsOnPoly(arg0, arg1, arg2, edgeClosest);
    Closest3(edgeClosest, arg2);
}

bool CCollision::SphereCastVsCaches(const CColSphere& spA, const CVector& velocity, int32_t numTest, CColCacheEntry* pTest, int32_t& pNumWrite, CColCacheEntry* pWrite) {
    // TODO: src/CCollision/SphereCastVsCaches_004181b0.c - needs CColCacheEntry.
    (void)spA; (void)velocity; (void)numTest; (void)pTest; (void)pNumWrite; (void)pWrite;
    return false;
}

// 0x418580 (src/CCollision/CalculateTrianglePlanes_00418580.c)
void CCollision::CalculateTrianglePlanes(CColModel* colModel) {
    if (colModel->m_pColData)
        CalculateTrianglePlanes(colModel->m_pColData);
}

// 0x4185A0 (src/CCollision/RemoveTrianglePlanes_004185a0.c)
void CCollision::RemoveTrianglePlanes(CColModel* colModel) {
    if (colModel->m_pColData)
        RemoveTrianglePlanes(colModel->m_pColData);
}

int32_t CCollision::ProcessColModels(const CMatrix& transformA, CColModel& cmA, const CMatrix& transformB, CColModel& cmB, std::array<CColPoint, 32>& sphereCPs, CColPoint* lineCPs, float* maxTouchDistances, bool arg7) {
    // TODO: src/CCollision/ProcessColModels_004185c0.c (48KB - the full
    // entity-vs-entity collision solver). Out of scope for this pass.
    (void)transformA; (void)cmA; (void)transformB; (void)cmB;
    (void)sphereCPs; (void)lineCPs; (void)maxTouchDistances; (void)arg7;
    return 0;
}

bool CCollision::SphereCastVsEntity(const CColSphere& spAws, const CColSphere& spBws, CEntity* entity) {
    // TODO: src/CCollision/SphereCastVsEntity_00419f00.c - needs CEntity.
    (void)spAws; (void)spBws; (void)entity;
    return false;
}

bool CCollision::SphereVsEntity(CColSphere* sphere, CEntity* entity) {
    // TODO: src/CCollision/SphereVsEntity_0041a5a0.c - needs CEntity.
    (void)sphere; (void)entity;
    return false;
}

bool CCollision::CheckCameraCollisionBuildings(int32_t X, int32_t Y, const CColBox& bbSpAB, const CColSphere& spS, const CColSphere& spA, const CColSphere& spB) {
    // TODO: src/CCollision/CheckCameraCollisionBuildings_0041a820.c - needs world/entity pools.
    (void)X; (void)Y; (void)bbSpAB; (void)spS; (void)spA; (void)spB;
    return false;
}

bool CCollision::CheckCameraCollisionVehicles(int32_t X, int32_t Y, const CColBox& bbSpAB, const CColSphere& spS, const CColSphere& spA, const CColSphere& spB, const CVector* plyrVehVel) {
    // TODO: src/CCollision/CheckCameraCollisionVehicles_0041a990.c - needs vehicle pool.
    (void)X; (void)Y; (void)bbSpAB; (void)spS; (void)spA; (void)spB; (void)plyrVehVel;
    return false;
}

bool CCollision::CheckCameraCollisionObjects(int32_t X, int32_t Y, const CColBox& pBox, const CColSphere& spS, const CColSphere& spA, const CColSphere& spB) {
    // TODO: src/CCollision/CheckCameraCollisionObjects_0041ab20.c - needs object pool.
    (void)X; (void)Y; (void)pBox; (void)spS; (void)spA; (void)spB;
    return false;
}

bool CCollision::BuildCacheOfCameraCollision(const CColSphere& spA, const CColSphere& spB) {
    // TODO: src/CCollision/BuildCacheOfCameraCollision_0041ac40.c
    (void)spA; (void)spB;
    return false;
}

bool CCollision::CameraConeCastVsWorldCollision(const CColSphere& spA, const CColSphere& spB, float& dist, float minDist) {
    // TODO: src/CCollision/CameraConeCastVsWorldCollision_0041b000.c
    (void)spA; (void)spB; (void)dist; (void)minDist;
    return false;
}

// --- NOTSA helpers (no decomp .c files; added by gta-reversed contributors) ---

CVector CCollision::GetClosestPtOnLine(const CVector& l0, const CVector& l1, const CVector& point) {
    const float lnMagSq = (l1 - l0).SquaredMagnitude();
    const float dot = (point - l0).Dot(l1 - l0);
    if (dot <= 0.0f)
        return l0;
    if (dot >= lnMagSq)
        return l1;
    return l0 + (l1 - l0) * (dot / lnMagSq);
}

CVector CCollision::GetBaryCoordsOnTriangle(CVector a, CVector b, CVector c, CVector p) {
    const CVector vab = b - a, vac = c - a, vap = p - a;
    const float bb = vab.Dot(vab);
    const float bc = vab.Dot(vac);
    const float cc = vac.Dot(vac);
    const float pb = vap.Dot(vab);
    const float pc = vap.Dot(vac);
    const float d = bb * cc - bc * bc;
    const float v = (cc * pb - bc * pc) / d;
    const float w = (bb * pc - bc * pb) / d;
    const float u = 1.0f - v - w;
    return CVector{ u, v, w };
}

CVector CCollision::GetClampedBaryCoordsIntoTriangle(CVector a, CVector b, CVector c, CVector p) {
    const CVector bary = GetBaryCoordsOnTriangle(a, b, c, p);
    const float u = bary.x, v = bary.y, w = bary.z;

    const auto projT = [&](CVector v1, CVector v2) {
        const CVector d = v1 - v2;
        return std::clamp((p - v2).Dot(d) / d.Dot(d), 0.0f, 1.0f);
    };

    if (u < 0.0f) {
        const float t = projT(c, b);
        return CVector{ 0.0f, 1.0f - t, t };
    }
    if (v < 0.0f) {
        const float t = projT(a, c);
        return CVector{ t, 0.0f, 1.0f - t };
    }
    if (w < 0.0f) {
        const float t = projT(b, a);
        return CVector{ 1.0f - t, t, 0.0f };
    }
    return CVector{ u, v, w }; // the point was inside the triangle
}

CVector CCollision::GetCoordsClampedIntoTriangle(CVector a, CVector b, CVector c, CVector p) {
    const CVector bary = GetClampedBaryCoordsIntoTriangle(a, b, c, p);
    return a * bary.x + b * bary.y + c * bary.z;
}

float CCollision::ClosestPtSegmentSegment(CVector p1, CVector d1, float a, CVector p2, CVector d2, float e, float& s, float& t, CVector& c1, CVector& c2) {
    // Credit: "Real-Time Collision Detection" by Christer Ericson (via gta-reversed).
    assert(a >= 0.0f);
    assert(e >= 0.0f);

    constexpr float EPSILON = std::numeric_limits<float>::epsilon();

    const CVector r = p1 - p2;
    const float f = d2.Dot(r);

    if (a <= EPSILON && e <= EPSILON) {
        // Both segments degenerate into points.
        s = t = 0.0f;
        c1 = p1;
        c2 = p2;
        return (c1 - c2).SquaredMagnitude();
    }

    if (a <= EPSILON) {
        // First segment degenerates into a point.
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    } else {
        const float c = d1.Dot(r);
        if (e <= EPSILON) {
            // Second segment degenerates into a point.
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        } else {
            const float b = d1.Dot(d2);
            const float denom = a * e - b * b;
            s = (denom != 0.0f) ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
    return (c1 - c2).SquaredMagnitude();
}

// --- free functions declared in CCollision.h ---

bool ProcessDiscCollision(CColPoint& colPoint1, const CMatrix& mat, const CColDisk& disk, CColPoint& colPoint2, bool& arg4, float& arg5, CColPoint& colPoint3) {
    // TODO: no named .c in src/CCollision/ (only the CCollision:: member
    // ProcessDiscCollision_00413960.c) - identify from unk_*.c / binary.
    // Forwards to the member version for now.
    return CCollision::ProcessDiscCollision(colPoint1, mat, disk, colPoint2, arg4, arg5, colPoint3);
}

void ResetMadeInvisibleObjects() {
    // TODO: no named .c in src/CCollision/ - identify from unk_*.c / binary.
}

// 0x415A40 (src/CCollision/ClosestSquaredDistanceBetweenFiniteLines_00415a40.c)
//
// NOTE: despite the names, line2End is a DIRECTION (its squared length is
// arg4), not an endpoint. Measures the squared distance between the segment
// (line1Start + s*line2End) and the segment (line1End + t*(line2Start -
// line1End)).
float ClosestSquaredDistanceBetweenFiniteLines(const CVector& line1Start, const CVector& line1End, const CVector& line2Start, const CVector& line2End, float arg4) {
    const CVector d1 = line1Start - line1End;
    const CVector r  = line2Start - line1End;
    const CVector& d2 = line2End;

    const float b  = d1.Dot(d2);
    const float dd = d1.Dot(r);
    const float ee = d2.Dot(r);
    const float cc = r.SquaredMagnitude();
    const float denomS = cc * arg4 - ee * ee;

    float sN, sD, tN, tD;
    if (denomS >= 1e-5f) {
        sN = dd * ee - b * cc;
        tN = dd * arg4 - b * ee;
        if (sN < 0.0f) {
            // s = 0: minimize over t alone (sD keeps denomS, as in the decomp).
            sN = 0.0f;
            sD = denomS;
            tN = dd;
            tD = cc;
        } else {
            sD = denomS;
            tD = denomS;
            if (denomS < sN) {
                // s = 1: minimize over t alone.
                tN = dd + ee;
                tD = cc;
                sN = denomS;
            }
        }
    } else {
        // Degenerate: s = 0.
        sN = 0.0f;
        sD = 1.0f;
        tN = dd;
        tD = cc;
    }

    // Clamp t to [0, 1], recomputing s for the clamped value:
    // s = clamp(sN2 / arg4, 0, 1).
    bool tClamped = false;
    float sN2 = 0.0f;
    if (tN < 0.0f) {
        tN = 0.0f;
        sN2 = -b;
        tClamped = true;
    } else if (tN > tD) {
        tN = tD;
        sN2 = ee - b;
        tClamped = true;
    }
    if (tClamped) {
        if (sN2 < 0.0f) {
            sN = 0.0f; // sD unchanged: 0/x == 0
        } else if (sN2 > arg4) {
            sN = sD;   // sN/sD == 1
        } else {
            sN = sN2;
            sD = arg4;
        }
    }

    const float s = std::fabs(sN) >= 1e-5f ? sN / sD : 0.0f;
    const float t = std::fabs(tN) >= 1e-5f ? tN / tD : 0.0f;
    const CVector diff = d2 * s + d1 - r * t;
    return diff.SquaredMagnitude();
}
