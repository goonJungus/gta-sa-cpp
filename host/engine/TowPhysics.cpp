// Tow links: a trailer (or a car on a tow truck's hoist) hangs by its hitch on a tractor's tow bar.
//
// Ported from CVehicle::UpdateTractorLink / UpdateTrailerLink, CAutomobile::GetTowHitchPos / GetTowBarPos /
// SetTowLink / BreakTowLink / TowTruckControl and CTrailer::GetTowHitchPos / GetTowBarPos / SetTowLink /
// BreakTowLink / ScanForTowLink. Each time step the towed car's ProcessControl pulls both ends' points together:
// an impulse cancels their relative speed (the tractor feels its share by mass), then a second pass pulls the
// points' gap closed. A link stretched too far, or a trailer folded back or rolled over against its tractor, breaks.
#define NOMINMAX
#include "Vehicle.h"
#include "PhysicsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace {

using namespace physics;

constexpr int kLinerun = 403, kPetro = 514, kRdTrain = 515, kTowtruck = 525, kTractor = 531, kUtility = 552, kBaggage = 485;
constexpr int kTug = 583, kArtict3 = 591, kBagboxA = 606, kBagboxB = 607, kTugStair = 608, kFarmTr1 = 610, kUtilTr1 = 611;
constexpr int kHoistDown = 20000, kHoistUp = 10000;   // TOWTRUCK_HOIST_DOWN_LIMIT (0x8D313C) / TOWTRUCK_HOIST_UP_LIMIT (0x8D3140)
constexpr float kTowbarOffsetY = -0.05f;              // TRAILER_TOWBAR_OFFSET_Y (0x871C20)
constexpr float kRelinkXY = 0.4f, kRelinkZ = 1.0f;    // RELINK_TRAILER_DIFF_LIMIT_XY / _Z (0x8D3470 / 0x8D3474)

bool Bagbox(int m) { return m == kBagboxA || m == kBagboxB; }
bool Hoist(int m) { return m == kTowtruck || m == kTractor; }

V3 ToWorld(const VehicleInstance& c, const V3& l) {
    V3 r, f, u;
    CarFrame(c, r, f, u);
    return V3(c.x, c.y, c.z) + r * l.x + f * l.y + u * l.z;
}

// CPhysical::ApplyForce of the impulse that brings a point's speed by v, at that point (relative to the car).
void Pull(Body& b, V3 v, const V3& at) {
    const V3 n = Norm(v);
    const float m = 1.0f / (1.0f / b.mass + Len2(Cross(at - b.ComW(), n)) / b.turnMass);
    b.ApplyForce(v * m, at);
}

}  // namespace

bool Vehicles::IsTrailer(int model) const {
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    return t == "trailer";
}

bool Vehicles::IsAircraft(int model) const {
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    return t == "plane" || t == "heli";
}

bool Vehicles::TowHitchPos(size_t i, bool check, int other, float out[3]) {
    const VehicleInstance& c = m_cars[i];
    VehicleModel* vm = nullptr;
    const HandlingData* h = nullptr;
    if (!Lookup(c.model, vm, h)) return false;
    float mn[3], mx[3];
    ColBounds(i, mn, mx);
    V3 l;
    if (vm->isTrailer) {   // CTrailer: the hookup dummy (never on a tow truck's hoist)
        if (other >= 0 && m_cars[(size_t)other].model == kTowtruck) return false;
        if (vm->hookupFound) l = V3(vm->hookup);
        else if (check) l = V3(0.0f, mx[1] + 1.0f, 0.5f - vm->frontHeight);
        else return false;
    } else {   // CAutomobile: under the front, half a metre off the road
        if (!check) return false;
        l = V3(0.0f, mx[1] - 0.5f, 0.5f - vm->frontHeight);
    }
    ToWorld(c, l).To(out);
    return true;
}

bool Vehicles::TowBarPos(size_t i, bool check, int other, float out[3]) {
    const VehicleInstance& c = m_cars[i];
    VehicleModel* vm = nullptr;
    const HandlingData* h = nullptr;
    if (!Lookup(c.model, vm, h)) return false;
    float mn[3], mx[3];
    ColBounds(i, mn, mx);
    const int om = other >= 0 ? m_cars[(size_t)other].model : -1;
    const bool otherTrailer = other >= 0 && IsTrailer(om);
    if (vm->isTrailer) {   // CTrailer: only the baggage carts pull another, from behind
        if (!check && !Bagbox(c.model)) return false;
        ToWorld(c, V3(0.0f, mn[1] - kTowbarOffsetY, 0.5f - vm->frontHeight)).To(out);
        return true;
    }
    if (Hoist(c.model)) {   // the hoist's hook: behind the truck, higher as it is raised
        float baseY = -1.05f;
        if (c.model == kTractor) {
            if (otherTrailer && om != kFarmTr1) return false;
            baseY = -0.6f;
        } else if (otherTrailer) return false;
        const float z = (1.0f - (float)c.hoist / (float)kHoistDown) / 2.0f + 0.5f - vm->frontHeight;
        ToWorld(c, V3(0.0f, baseY + mn[1], z)).To(out);
        return true;
    }
    if (vm->nodes[0].found) {   // misc_a: a semi's fifth wheel, the utility van's and the baggage tug's hitch
        const bool node = c.model == kPetro || c.model == kRdTrain || c.model == kLinerun || c.model == kArtict3 ||
                          (c.model == kUtility && om == kUtilTr1) ||
                          ((c.model == kBaggage || c.model == kTug) && (Bagbox(om) || om == kTugStair));
        if (node) return NodeWorldPos(i, 0, out);
    }
    return TowHitchPos(i, check, other, out);
}

bool Vehicles::SetTowLink(size_t towed, size_t tractor, bool place, float ts) {
    if (towed == tractor || towed >= m_cars.size() || tractor >= m_cars.size()) return false;
    VehicleInstance& c = m_cars[towed];
    VehicleInstance& t = m_cars[tractor];
    if (c.removed || t.removed || c.hasRot || c.phys.player) return false;   // STATUS_PHYSICS / ABANDONED / IS_TOWED only
    const bool trailer = IsTrailer(c.model);
    if (!trailer && c.towedBy >= 0) return false;   // CAutomobile: already on a hoist
    if (c.towedBy >= 0) BreakTowLink(towed);
    if (t.towing >= 0 && (size_t)t.towing < m_cars.size() && m_cars[(size_t)t.towing].towedBy == (int)tractor) BreakTowLink((size_t)t.towing);
    Activate(c);
    Activate(t);
    c.towedBy = (int)tractor;
    t.towing = (int)towed;
    c.phys.fakePhysics = t.phys.fakePhysics = 0;
    if (!place) {   // hooked where it stands: the first link pass matches the speeds
        if (trailer) TractorLink(tractor, true, false, ts);
        else TrailerLink(towed, true, false, ts);
        return true;
    }
    // Put it on the bar: CTrailer keeps its own heading (levelled) and takes the tractor's speed, a car on a hoist
    // takes the tractor's heading with the hoist raised. (PlaceOnRoadProperly is left to the springs.)
    if (!trailer && Hoist(t.model)) t.hoist = kHoistUp;
    float hd = c.heading;
    if (!trailer) { V3 r, f, u; CarFrame(t, r, f, u); hd = std::atan2(-f.x, f.y); }
    const float cs = std::cos(hd), sn = std::sin(hd);
    const float rot[9] = {cs, sn, 0, -sn, cs, 0, 0, 0, 1};
    std::copy(rot, rot + 9, c.rot);
    c.heading = hd;
    std::fill(c.phys.tv, c.phys.tv + 3, 0.0f);
    if (trailer) std::copy(t.phys.mv, t.phys.mv + 3, c.phys.mv);
    float hitch[3], bar[3];
    if (!TowHitchPos(towed, true, (int)towed, hitch) || !TowBarPos(tractor, true, (int)towed, bar)) return false;
    c.x = bar[0] - (hitch[0] - c.x); c.y = bar[1] - (hitch[1] - c.y); c.z = bar[2] - (hitch[2] - c.z);
    return true;
}

bool Vehicles::AttachTrailer(size_t trailer, size_t cab) { return SetTowLink(trailer, cab, true, 1.0f); }

void Vehicles::BreakTowLink(size_t towed) {
    VehicleInstance& c = m_cars[towed];
    const int t = c.towedBy;
    if (t >= 0 && (size_t)t < m_cars.size() && m_cars[(size_t)t].towing == (int)towed) m_cars[(size_t)t].towing = -1;
    c.towedBy = -1;
}

// CVehicle::UpdateTractorLink: the tractor's end. settle: the link was just made (no mass share, no gap pull);
// fix: the second pass, which pulls the gap shut instead of matching speeds.
void Vehicles::TractorLink(size_t tractor, bool settle, bool fix, float ts) {
    VehicleInstance& tc = m_cars[tractor];
    const int r = tc.towing;
    if (r < 0 || (size_t)r >= m_cars.size()) return;
    VehicleInstance& rc = m_cars[(size_t)r];
    float hitch[3], bar[3];
    if (!TowHitchPos((size_t)r, true, (int)tractor, hitch) || !TowBarPos(tractor, true, r, bar)) return;
    if (Hoist(tc.model) && tc.hoist > kHoistDown - 100) return;   // the hoist all the way down holds nothing
    VehicleModel* vt = nullptr; VehicleModel* vr = nullptr;
    const HandlingData* ht = nullptr; const HandlingData* hr = nullptr;
    if (!Lookup(tc.model, vt, ht) || !Lookup(rc.model, vr, hr)) return;
    Body T = Load(tc, *ht);
    const Body R = Load(rc, *hr);
    const V3 gap = V3(hitch) - V3(bar);
    V3 v = R.Speed(V3(hitch) - R.pos) - T.Speed(V3(bar) - T.pos);
    if (!settle) {
        v *= (1.0f - T.mass / (R.mass + T.mass)) * 0.5f;
        if (fix) v = gap * 0.1f / std::max(1.0f, ts);
    }
    if (Bagbox(rc.model)) v -= R.up * Dot(v, R.up);   // the baggage carts ride on their own wheels
    Pull(T, v, V3(bar) - T.pos);
    Store(T, tc);
    tc.phys.fakePhysics = 0;
}

// CVehicle::UpdateTrailerLink: the towed end, whole (no mass share). Breaks when the points have come apart
// further than a time step's slack, or the trailer is folded back on, or upside down against, its tractor.
void Vehicles::TrailerLink(size_t towed, bool settle, bool fix, float ts) {
    VehicleInstance& rc = m_cars[towed];
    const int t = rc.towedBy;
    float hitch[3], bar[3];
    if (t < 0 || (size_t)t >= m_cars.size() || !TowHitchPos(towed, true, t, hitch) || !TowBarPos((size_t)t, true, (int)towed, bar)) {
        BreakTowLink(towed);
        return;
    }
    VehicleInstance& tc = m_cars[(size_t)t];
    VehicleModel* vt = nullptr; VehicleModel* vr = nullptr;
    const HandlingData* ht = nullptr; const HandlingData* hr = nullptr;
    if (!Lookup(tc.model, vt, ht) || !Lookup(rc.model, vr, hr)) return;
    const Body T = Load(tc, *ht);
    Body R = Load(rc, *hr);
    const V3 gap = V3(bar) - V3(hitch);
    if (Len(gap) > std::max(ts, 0.7f) || Dot(T.fwd, R.fwd) < -0.3f || Dot(T.up, R.up) < 0.0f) {
        BreakTowLink(towed);
        return;
    }
    if (Hoist(tc.model) && tc.hoist > kHoistDown - 100) return;
    V3 v = T.Speed(V3(bar) - T.pos) - R.Speed(V3(hitch) - R.pos);
    if (!settle && fix) v = gap * 0.3f / std::max(1.0f, ts);
    if (Bagbox(rc.model)) v -= R.up * Dot(v, R.up);
    Pull(R, v, V3(hitch) - R.pos);
    Store(R, rc);
}

// CAutomobile::ProcessControl, STATUS_IS_TOWED: both ends match speeds, then both pull the gap shut. The tractor
// only joins in while one of them moves.
void Vehicles::TowLinks(size_t towed, float ts) {
    VehicleInstance& rc = m_cars[towed];
    const int t = rc.towedBy;
    if (t < 0 || (size_t)t >= m_cars.size() || m_cars[(size_t)t].removed || m_cars[(size_t)t].towing != (int)towed) {
        BreakTowLink(towed);
        return;
    }
    const VehicleInstance& tc = m_cars[(size_t)t];
    const bool both = Len2(V3(tc.phys.mv)) != 0.0f || Len2(V3(rc.phys.mv)) > 0.01f;
    if (both) TractorLink((size_t)t, false, false, ts);
    TrailerLink(towed, false, false, ts);
    if (rc.towedBy >= 0) {
        if (both) TractorLink((size_t)t, false, true, ts);
        TrailerLink(towed, false, true, ts);
    }
}

// CTrailer::ScanForTowLink: a trailer standing on its legs hooks onto any tow bar backed up under its hitch.
void Vehicles::ScanForTowLink(size_t trailer, float ts) {
    const VehicleInstance& c = m_cars[trailer];
    if (c.towedBy >= 0 || c.hasRot || c.model == kFarmTr1) return;
    float hitch[3];
    if (!TowHitchPos(trailer, false, -1, hitch)) return;
    for (size_t k = 0; k < m_cars.size(); k++) {   // CWorld::FindObjectsInRange, 10 m
        const VehicleInstance& o = m_cars[k];
        if (k == trailer || o.removed) continue;
        const float dx = o.x - hitch[0], dy = o.y - hitch[1];
        if (dx * dx + dy * dy > 100.0f) continue;
        float bar[3];
        if (!TowBarPos(k, false, (int)trailer, bar)) continue;
        const float ex = bar[0] - hitch[0], ey = bar[1] - hitch[1];
        if (std::sqrt(ex * ex + ey * ey) < kRelinkXY && std::fabs(hitch[2] - bar[2]) < kRelinkZ) {
            SetTowLink(trailer, k, false, ts);
            return;
        }
    }
}

// CTrailer::ProcessAI for every trailer: one left standing (legs more than half down, or a baggage cart) looks for
// a tractor.
void Vehicles::ScanTrailers(float ts) {
    for (size_t i = 0; i < m_cars.size(); i++) {
        const VehicleInstance& c = m_cars[i];
        if (c.removed || c.wrecked || c.towedBy >= 0 || !IsTrailer(c.model)) continue;
        if (c.phys.supportExt > 0.5f || Bagbox(c.model)) ScanForTowLink(i, ts);
    }
}

// CAutomobile::TowTruckControl: the special control raises (up, 2 per step at full stick) or lowers (6) the hoist;
// lowered all the way under a car's front it picks the car up.
void Vehicles::TowTruckControl(size_t i, float ts) {
    VehicleInstance& c = m_cars[i];
    const float upDown = c.phys.inGunUpDown * 128.0f;   // CPad::GetCarGunUpDown
    if (std::fabs(upDown) > 10.0f) {
        if (upDown > 0.0f) c.hoist = std::max(c.towing >= 0 ? kHoistUp : 0, c.hoist - (int)(upDown * 2.0f * ts));
        else if (c.hoist < kHoistDown) c.hoist = std::min(kHoistDown, c.hoist - (int)(upDown * 6.0f * ts));
    }
    if (c.hoist != kHoistDown || c.towing >= 0) return;
    float bar[3];
    if (!TowBarPos(i, false, (int)i, bar)) return;
    for (size_t k = 0; k < m_cars.size(); k++) {   // CWorld::FindObjectsInRange, 10 m
        const VehicleInstance& o = m_cars[k];
        if (k == i || o.removed) continue;
        const float dx = o.x - bar[0], dy = o.y - bar[1];
        if (dx * dx + dy * dy > 100.0f) continue;
        float hitch[3];
        if (!TowHitchPos(k, true, (int)i, hitch) || o.locked) continue;
        const float ex = bar[0] - hitch[0], ey = bar[1] - hitch[1];
        if (std::fabs(hitch[2] - bar[2]) < 1.0f && ex * ex + ey * ey < 0.5f * 0.5f) {
            SetTowLink(k, i, false, ts);
            c.hoist -= 100;   // just off the bottom, so the link holds
            break;
        }
    }
}
