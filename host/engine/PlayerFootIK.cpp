#include "PlayerPed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// Rewrite foot IK. Each leg is solved on its own: contact is inferred from the
// animated foot's height and its vertical speed, the ground under the ankle is
// probed with the toe, heel and centre, and while the foot is planted the ankle
// is moved to the ground with an analytic two-bone solve (the animated knee is
// the pole). The foot is then pitched to the ground slope. Every weight and
// offset is eased; the corrections are clamped per joint.

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kDeg = kPi / 180.0f;

// FootIK settings
constexpr float kProbeStartHeight = 0.50f, kProbeDepth = 0.70f;
constexpr float kProbeForward = 0.16f, kProbeBackward = 0.13f, kProbeSide = 0.075f;
constexpr float kMaxStepUp = 0.45f, kMaxStepDown = 0.28f, kMaxOffset = 0.50f;
constexpr float kDeadZone = 0.006f;
constexpr float kContactHeight = 0.11f, kContactVelocity = 0.75f;
constexpr float kPlantResponse = 22.0f, kReleaseResponse = 16.0f;
constexpr float kPositionResponse = 18.0f, kNormalResponse = 10.0f;
constexpr float kMinKneePlane = 0.025f;
constexpr float kMaxHipDeg = 35.0f, kMaxKneeDeg = 90.0f, kMaxFootDeg = 28.0f;
constexpr float kMinNormalZ = 0.68f, kFullNormalZ = 0.82f;
constexpr float kGroundSearchUp = 1.5f;   // GetGroundHeight accepts surfaces up to this far above its reference

float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
void Cross(const float a[3], const float b[3], float o[3]) {
    o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
float Len(const float a[3]) { return std::sqrt(Dot(a, a)); }

// Rotation about a model-space axis through a pivot, as an RW (row vector) matrix.
RwMat AxisRotation(const float axis[3], float radians, const float pivot[3]) {
    const float c = std::cos(radians), s = std::sin(radians), t = 1.0f - c;
    const float x = axis[0], y = axis[1], z = axis[2];
    const float m[3][3] = {{t * x * x + c, t * x * y - s * z, t * x * z + s * y},
                           {t * x * y + s * z, t * y * y + c, t * y * z - s * x},
                           {t * x * z - s * y, t * y * z + s * x, t * z * z + c}};
    RwMat r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r.m[i][j] = m[j][i];
    float rp[3];
    r.ApplyDir(pivot, rp);
    for (int k = 0; k < 3; k++) r.m[3][k] = pivot[k] - rp[k];
    return r;
}

// Smallest rotation about pivot taking direction a onto b, capped at maxDeg.
RwMat Between(const float a[3], const float b[3], const float pivot[3], float maxDeg) {
    float axis[3];
    Cross(a, b, axis);
    const float s = Len(axis);
    const float c = Dot(a, b);
    if (s < 1e-6f) return RwMat();
    for (float& v : axis) v /= s;
    float ang = std::atan2(s, c);
    ang = std::min(ang, maxDeg * kDeg);
    return AxisRotation(axis, ang, pivot);
}
}  // namespace

void PlayerPed::FootRestHeights() {
    // Bind pose ankle height above the sole.
    static const int kAnkle[2] = {43, 53};
    for (int l = 0; l < 2; l++) {
        const int f = m_frameOfBone[kAnkle[l]];
        m_footRest[l] = 0.08f;
        if (f < 0 || f >= (int)m_skeleton.size()) continue;
        RwMat g = m_skeleton[f].local;
        for (int p = m_skeleton[f].parent; p >= 0; p = m_skeleton[p].parent) g = g * m_skeleton[p].local;
        m_footRest[l] = std::max(0.0f, g.m[3][2] - m_bindMinZ);
    }
    m_footInit = true;
}

void PlayerPed::ApplyFootIK(const Input& in) {
    static const int kBones[2][3] = {{41, 42, 43}, {51, 52, 53}};   // thigh, calf, foot (right, left)
    if (!m_footInit) FootRestHeights();
    const float dt = std::max(in.dt, 1e-4f);
    const bool active = in.footIk && in.onGround && !in.noclip && !m_climbing && !m_busy && in.groundAt;
    const float ch = std::cos(m_heading - kPi * 0.5f), sh = std::sin(m_heading - kPi * 0.5f);   // model -> world
    const float fwdX = std::cos(m_heading), fwdY = std::sin(m_heading);
    const float rgtX = std::sin(m_heading), rgtY = -std::cos(m_heading);

    for (int l = 0; l < 2; l++) {
        FootLeg& leg = m_foot[l];
        const int fh = m_frameOfBone[kBones[l][0]], fk = m_frameOfBone[kBones[l][1]], fa = m_frameOfBone[kBones[l][2]];
        if (fh < 0 || fk < 0 || fa < 0) continue;
        float H[3], K[3], A[3];
        for (int i = 0; i < 3; i++) { H[i] = m_globals[fh].m[3][i]; K[i] = m_globals[fk].m[3][i]; A[i] = m_globals[fa].m[3][i]; }

        // animated sole height above the ped's feet plane and its vertical speed
        const float sole = A[2] - m_bindMinZ - m_footRest[l];
        const float vz = leg.valid ? (sole - leg.prevSole) / dt : 0.0f;
        leg.prevSole = sole;
        leg.valid = true;

        bool contact = false;
        if (active) {
            contact = leg.contact ? (sole < kContactHeight * 1.25f && vz < kContactVelocity)
                                  : (sole < kContactHeight && std::fabs(vz) < kContactVelocity);
        }
        leg.contact = contact;
        const float resp = contact ? kPlantResponse : kReleaseResponse;
        leg.weight += ((contact ? 1.0f : 0.0f) - leg.weight) * (1.0f - std::exp(-resp * dt));
        if (!active && leg.weight < 1e-3f) { leg.weight = 0.0f; leg.offset = 0.0f; leg.pitch = 0.0f; continue; }

        // ground under the ankle: centre, toe and heel probes (sides only when the centre misses)
        const float ax = in.worldX + A[0] * ch - A[1] * sh, ay = in.worldY + A[0] * sh + A[1] * ch;
        const float start = in.feetZ + std::max(sole, 0.0f) + kProbeStartHeight;
        const float floorZ = start - kProbeStartHeight - kProbeDepth;
        auto probe = [&](float f, float s, float* out) {
            const float g = in.groundAt(ax + fwdX * f + rgtX * s, ay + fwdY * f + rgtY * s, start - kGroundSearchUp);
            if (g < -1e5f || g > start || g < floorZ) return false;
            *out = g;
            return true;
        };
        float gc = 0, gt = 0, gh = 0;
        bool hit = probe(0.0f, 0.0f, &gc);
        if (!hit) {   // two agreeing side hits stand in for the centre
            float a = 0, b = 0;
            if (probe(0.0f, kProbeSide, &a) && probe(0.0f, -kProbeSide, &b) && std::fabs(a - b) < 0.03f) { gc = (a + b) * 0.5f; hit = true; }
        }
        float slope = 0.0f;
        if (hit && probe(kProbeForward, 0.0f, &gt) && probe(-kProbeBackward, 0.0f, &gh))
            if (std::fabs(gc - (gt * kProbeBackward + gh * kProbeForward) / (kProbeForward + kProbeBackward)) < 0.02f)   // one plane under the foot; a step edge is not a slope
                slope = (gt - gh) / (kProbeForward + kProbeBackward);
        const float nz = 1.0f / std::sqrt(1.0f + slope * slope);
        const float fade = std::clamp((nz - kMinNormalZ) / (kFullNormalZ - kMinNormalZ), 0.0f, 1.0f);

        float want = 0.0f;
        if (hit && active) {
            want = std::clamp(gc - in.feetZ, -kMaxStepDown, kMaxStepUp);
            want = std::clamp(want, -kMaxOffset, kMaxOffset);
            if (std::fabs(want) < kDeadZone) want = 0.0f;
        }
        if (want > leg.offset) leg.offset = want;   // penetration: no lag going up
        else leg.offset += (want - leg.offset) * (1.0f - std::exp(-kPositionResponse * dt));
        const float wantPitch = std::clamp(std::atan(slope), -kMaxFootDeg * kDeg, kMaxFootDeg * kDeg);
        leg.pitch += ((hit && active ? wantPitch : 0.0f) - leg.pitch) * (1.0f - std::exp(-kNormalResponse * dt));

        const float k = leg.weight * fade;
        const float dz = leg.offset * k;
        if (std::fabs(dz) < 1e-4f && std::fabs(leg.pitch * k) < 1e-4f) continue;

        auto subtree = [&](int root, const RwMat& rot) {
            for (size_t f = 0; f < m_skeleton.size(); f++)
                for (int a = (int)f; a >= 0; a = m_skeleton[a].parent)
                    if (a == root) { m_globals[f] = m_globals[f] * rot; break; }
        };

        // two-bone solve towards the lowered / raised ankle
        const float T[3] = {A[0], A[1], A[2] + dz};
        float HK[3] = {K[0] - H[0], K[1] - H[1], K[2] - H[2]};
        float KA[3] = {A[0] - K[0], A[1] - K[1], A[2] - K[2]};
        const float l1 = Len(HK), l2 = Len(KA);
        float HT[3] = {T[0] - H[0], T[1] - H[1], T[2] - H[2]};
        float d = Len(HT);
        if (d > 1e-4f && l1 > 1e-4f && l2 > 1e-4f) {
            const float u[3] = {HT[0] / d, HT[1] / d, HT[2] / d};
            d = std::clamp(d, std::fabs(l1 - l2) + 1e-3f, l1 + l2 - 1e-3f);
            const float a = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
            const float h = std::sqrt(std::max(l1 * l1 - a * a, 0.0f));
            const float kd = Dot(HK, u);
            float p[3] = {HK[0] - u[0] * kd, HK[1] - u[1] * kd, HK[2] - u[2] * kd};
            if (Len(p) < kMinKneePlane) {   // knee in line with the leg: keep it pointing forward
                const float fw[3] = {0, 1, 0};
                const float fd = Dot(fw, u);
                for (int i = 0; i < 3; i++) p[i] = fw[i] - u[i] * fd;
            }
            const float pl = Len(p);
            if (pl > 1e-6f) for (float& v : p) v /= pl;
            const float K2[3] = {H[0] + u[0] * a + p[0] * h, H[1] + u[1] * a + p[1] * h, H[2] + u[2] * a + p[2] * h};
            const float T2[3] = {H[0] + u[0] * d, H[1] + u[1] * d, H[2] + u[2] * d};

            const float HK2[3] = {K2[0] - H[0], K2[1] - H[1], K2[2] - H[2]};
            subtree(fh, Between(HK, HK2, H, kMaxHipDeg));
            float K1[3], A1[3];
            for (int i = 0; i < 3; i++) { K1[i] = m_globals[fk].m[3][i]; A1[i] = m_globals[fa].m[3][i]; }
            const float KA1[3] = {A1[0] - K1[0], A1[1] - K1[1], A1[2] - K1[2]};
            const float KT[3] = {T2[0] - K1[0], T2[1] - K1[1], T2[2] - K1[2]};
            subtree(fk, Between(KA1, KT, K1, kMaxKneeDeg));
        }
        // foot pitched to the slope (toes up when the ground rises ahead)
        if (std::fabs(leg.pitch * k) > 1e-4f) {
            const float right[3] = {1, 0, 0};
            float P[3];
            for (int i = 0; i < 3; i++) P[i] = m_globals[fa].m[3][i];
            subtree(fa, AxisRotation(right, leg.pitch * k, P));
        }
    }
}

void PlayerPed::FootDebug(char* out, size_t n) const {
    std::snprintf(out, n, "fik R=%.2f/%.3f/%.1f L=%.2f/%.3f/%.1f", m_foot[0].weight, m_foot[0].offset, m_foot[0].pitch / kDeg,
                  m_foot[1].weight, m_foot[1].offset, m_foot[1].pitch / kDeg);
}
