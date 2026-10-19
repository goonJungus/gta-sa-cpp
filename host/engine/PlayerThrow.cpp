// CJ's thrown weapons: CTaskSimpleThrowProjectile. WEAPON_start_throw plays first; when it ends the
// throw clip is picked (overhand WEAPON_throw, or underhand WEAPON_throwu for a quick tap and the
// satchel). The projectile leaves the right hand at the weapon.dat fire frame of that clip, its
// force set by how long the button was held. See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr int kBoneRHand = 24;                 // BONE_R_HAND
constexpr float kMaxCounterMs = 533.0f;        // 0x215: the longest hold that still adds force
constexpr int kSatchel = 39;
constexpr int kMolotov = 18;
}  // namespace

// The right hand's frame plus a weapon-space offset, in the world.
void PlayerPed::HandWorld(const Input& in, const float off[3], float out[3]) const {
    const int f = m_frameOfBone[kBoneRHand];
    out[0] = in.worldX; out[1] = in.worldY; out[2] = in.feetZ;
    if (f < 0 || f >= (int)m_globals.size()) return;
    const RwMat& h = m_globals[f];
    float p[3];
    for (int j = 0; j < 3; j++) p[j] = off[0] * h.m[0][j] + off[1] * h.m[1][j] + off[2] * h.m[2][j] + h.m[3][j];
    const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
    out[0] = in.worldX + p[0] * c - p[1] * s;
    out[1] = in.worldY + p[0] * s + p[1] * c;
    out[2] = in.feetZ - m_bindMinZ + p[2];
}

void PlayerPed::UpdateThrow(const Input& in) {
    if (!HoldingThrown()) { m_throw = Throw(); return; }
    Gun& g = m_guns[m_melee];

    if (g.detonator) {   // WEAPON_FIRE_USE: CWorld::UseDetonator, no animation
        if (in.fireJust && g.total > 0 && !m_busy) {   // CWeapon::Fire: ammo 1, then spent, so the detonator goes
            m_detonatePending = true;
            g.total = 0;
            SwitchAfterEmpty();
        }
        return;
    }

    if (g.type >= 44) {   // CTaskComplexUseGoggles: goggles_put_on, then PutOn/TakeOffGoggles
        if (!m_gogglesClip) {
            if (!in.fireJust || m_busy || m_inAir || m_swimming || m_climbing) return;
            const AnimClip* c = Clip("goggles_put_on");
            if (!c) return;
            Assoc* a = Add(c, false, false);
            a->partial = true;
            a->blend = 0.0f;
            a->delta = 4.0f;
            m_gogglesClip = c;
            return;
        }
        Assoc* a = Get(m_gogglesClip);
        if (!a || a->dead || a->finished) {
            if (a) a->delta = -4.0f;
            m_goggles = m_goggles == g.type ? 0 : g.type;
            m_gogglesClip = nullptr;
        }
        return;
    }

    Throw& t = m_throw;
    if (t.state == 0) {
        if (!in.fireJust || g.total <= 0 || m_busy || m_inAir || m_swimming || m_climbing || m_ratio >= 1.9f) return;
        const AnimClip* c = Clip("weapon_start_throw");
        if (!c) return;
        t = Throw();
        Assoc* a = Add(c, false, false);
        a->partial = true;
        a->blend = 0.0f;
        a->delta = 16.0f;
        t.state = 1;
        t.clip = c;
        return;
    }

    if (!t.released) {
        if (!in.fireHeld) t.released = true;
        else t.counterMs += in.dt * 1000.0f;
    }

    Assoc* a = Get(t.clip);
    if (!a || a->dead) { t = Throw(); return; }

    if (t.state == 1) {
        if (!a->finished) return;
        a->delta = -4.0f;
        // StartAnim: a farTarget lock-on target, or a held button, throws overhand; a tap or the satchel underhand
        bool farTarget = false;
        if (in.lockOn) {
            const float dx = in.lockPos[0] - in.worldX, dy = in.lockPos[1] - in.worldY, dz = in.lockPos[2] - (in.feetZ + 1.0f);
            farTarget = dx * dx + dy * dy + dz * dz > 100.0f;
        }
        t.underhand = !farTarget && (t.released || g.type == kSatchel);
        const AnimClip* c = Clip(t.underhand ? "weapon_throwu" : "weapon_throw");
        if (!c) { t = Throw(); return; }
        Assoc* n = Add(c, false, false);
        n->partial = true;
        n->blend = 0.0f;
        n->delta = 1000.0f;
        t.state = 2;
        t.clip = c;
        t.fired = false;
        return;
    }

    const float fireAt = t.underhand ? g.fire2Time : g.fireTime;
    if (!t.fired && a->time >= fireAt) ThrowFire(in);
    if (a->finished) {
        a->delta = -4.0f;
        const bool out = g.total <= 0 && g.type != kSatchel;
        const int det = g.type == kSatchel && m_switchToDetonator ? DetonatorIndex() : -1;
        t = Throw();
        m_switchToDetonator = false;
        if (det >= 0) SelectGun(det);
        else if (out) SwitchAfterEmpty();
    }
}

// CWeapon::Fire / CWeapon::FireProjectile for the thrown types.
void PlayerPed::ThrowFire(const Input& in) {
    Throw& t = m_throw;
    Gun& g = m_guns[m_melee];
    t.fired = true;
    const float counter = std::min(t.counterMs, kMaxCounterMs);
    float force = counter * 0.05f * 0.0375f;
    const int type = g.type;

    const float fx = std::cos(m_heading), fy = std::sin(m_heading);
    float pos[3];
    HandWorld(in, g.fireOffset, pos);
    const float ped[3] = {in.worldX, in.worldY, in.feetZ + 1.0f};
    float along = (pos[0] - ped[0]) * fx + (pos[1] - ped[1]) * fy;
    if (along < 0.3f) { pos[0] += fx * (0.3f - along); pos[1] += fy * (0.3f - along); along = 0.3f; }

    // line of sight from the origin on the ped's plane to just ahead of it
    bool blocked = false;
    if (m_wallProbe) {
        const float a0[3] = {pos[0] - fx * along, pos[1] - fy * along, pos[2]};
        const float reach = pos[2] > ped[2] ? 0.6f : 0.0f;
        const float a1[3] = {pos[0] + fx * reach, pos[1] + fy * reach, pos[2]};
        blocked = m_wallProbe(a0, a1);
    }

    Launch l;
    l.gun = m_melee;
    l.type = type;
    if (blocked) {
        l.fell = true;
        if (type == 16 || type == kSatchel) {   // drops behind CJ's feet when a 0.3 sphere there is free
            force = 0.0f;
            l.pos[0] = ped[0] - fx; l.pos[1] = ped[1] - fy; l.pos[2] = pos[2] - 0.4f;
        } else {
            l.pos[0] = pos[0]; l.pos[1] = pos[1]; l.pos[2] = pos[2];
        }
    } else {
        l.pos[0] = pos[0]; l.pos[1] = pos[1]; l.pos[2] = pos[2];
    }

    // CProjectileInfo::AddProjectile: velocity per timestep, x50 for m/s
    float h, vz;
    if (type == kMolotov) {
        h = std::max(0.2f, force * 0.22f + 0.15f);
        vz = (force * 0.2f + 0.4f) * h;
    } else {
        h = force == 0.0f ? 0.0f : force * 0.22f + 0.15f;
        if (type == kSatchel) h *= 0.5f;
        vz = (force + 1.0f) * 0.4f * h;
    }
    l.vel[0] = fx * h * 50.0f; l.vel[1] = fy * h * 50.0f; l.vel[2] = vz * 50.0f;
    m_launch = l;
    m_launchPending = true;

    if (g.total > 0) g.total--;
    if (type == kSatchel) {   // GiveWeapon(DETONATOR, 1); it becomes current when this was the last satchel
        if (const int d = DetonatorIndex(); d >= 0) {
            m_guns[d].total = 1;
            m_switchToDetonator = g.total < 1;
        }
    }
}

// The spent detonator starts the search at slot 8 when a satchel is held there; anything else one slot
// down from its own. Each slot down takes the first weapon in it with ammo; slot 0 is the fists.
// Slots 9-11 are passed over: retail went from the detonator straight to the minigun past the camera
// and goggles.
void PlayerPed::SwitchAfterEmpty() {
    const int cur = WeaponIndex();
    if (cur < 0) return;
    const Gun& c = m_guns[cur];
    int s = c.slot - 1;
    if (c.detonator)
        for (const Gun& g : m_guns) if (g.slot == 8 && g.type == kSatchel && g.total > 0) s = 8;
    for (; s > 0; s--)
        for (size_t i = 0; i < m_guns.size(); i++) {
            const Gun& g = m_guns[i];
            if (g.slot != s || (int)i == cur || (s >= 9 && s <= 11)) continue;   // retail skips camera, gifts and goggles (seen: detonator -> minigun)
            if (g.ammo + g.total > 0) { SelectGun((int)i); return; }
        }
    SelectGun(-1);
}

int PlayerPed::DetonatorIndex() const {
    for (size_t i = 0; i < m_guns.size(); i++)
        if (m_guns[i].detonator) return (int)i;
    return -1;
}

// A thrown weapon's model at a world matrix (the projectile object).
void PlayerPed::DrawGunModel(D3DRenderer& r, int gunIndex, const float w[4][4]) {
    if (gunIndex < 0 || gunIndex >= (int)m_guns.size()) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    D3DMATRIX out;
    float* o = &out._11;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) o[i * 4 + j] = w[i][j];
    dev->SetTransform(D3DTS_WORLD, &out);
    for (D3DRenderMesh* m : m_guns[gunIndex].meshes) r.DrawMesh(m);
}
