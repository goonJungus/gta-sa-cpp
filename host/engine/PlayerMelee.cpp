// CJ's melee: CTaskSimpleFight on data\melee.dat. The swing clips come from the
// weapon's anim group (Melee.cpp); each move strikes once at its hit frame, and
// a press after the hit frame chains the next attack at or after the chain frame.
// See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"
#include "Stats.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace {

std::string LowerName(const std::string& s) {
    std::string o = s;
    for (char& c : o) c = (char)std::tolower((unsigned char)c);
    return o;
}

}  // namespace

// The combo of whatever CJ holds: fists and the knuckles take the fight style's
// UNARMED_n, the rest their weapon.dat base combo.
const Melee::Combo* PlayerPed::CurrentCombo() const {
    if (!m_meleeOk || m_gun >= 0) return nullptr;
    std::string n = m_melee >= 0 ? m_guns[m_melee].comboName : std::string("UNARMED#");
    if (n.empty()) return nullptr;
    if (n.back() == '#') n = n.substr(0, n.size() - 1) + "_" + (char)('1' + m_fightStyle);
    return m_meleeTable.Find(n);
}

// CTaskSimpleFight: the player's move damage is scaled by CStats::GetFatAndMuscleModifier(4).
float PlayerPed::FatMuscleModifier() const { return Stats::FatMuscleMod(4); }

namespace {
constexpr int kBlockMove = Melee::kMoveCount;   // FightX_Block: the group's clip 8, ABLOCK times
}

void PlayerPed::FightStart(const Input& in, int move, const Melee::Combo* use) {
    const Melee::Combo* c = use ? use : CurrentCombo();
    if (!c) return;
    const std::vector<std::string>& cl = Melee::GroupClips(c->group);
    const size_t idx = move == kBlockMove ? 8 : move == Melee::kMoving ? 4 : (size_t)move;
    if (idx >= cl.size()) return;
    const AnimClip* clip = Clip(LowerName(cl[idx]).c_str());
    if (!clip) return;
    if (m_fight.active && m_fight.clip != clip)
        if (Assoc* o = Get(m_fight.clip)) o->delta = -8.0f;
    Assoc* a = Add(clip, false, false);
    a->partial = true;
    a->blend = 0.0f;
    a->delta = 8.0f;
    a->time = 0.0f;
    a->playing = true;
    a->speed = Stats::FatMuscleMod(3);   // the fight clip runs at GetFatAndMuscleModifier(3)
    m_fight.active = true;
    m_fight.combo = c;
    m_fight.move = move;
    m_fight.clip = clip;
    m_fight.hitDone = false;
    m_fight.queued = false;
    m_fight.special = use != nullptr;
    m_fight.block = move == kBlockMove;
    std::fprintf(stderr, "[MELEE] %s move %d clip %s%s speed %.3f\n", c->name.c_str(), move, clip->name.c_str(), m_fight.special ? " special" : "", a->speed);
    (void)in;
}

void PlayerPed::FightEnd(bool abrupt) {
    if (m_fight.active && m_fight.clip)
        if (Assoc* a = Get(m_fight.clip)) { a->playing = true; a->delta = abrupt ? -16.0f : -8.0f; }
    m_fight = Fight();
}

bool PlayerPed::MeleeHit(float fromX, float fromY, float worldX, float worldY, int strength) {
    if (m_seated) return true;
    m_heading = m_aimHeading = std::atan2(fromY - worldY, fromX - worldX);
    if (Blocking()) return false;
    const Melee::Combo* c = CurrentCombo();
    const std::vector<std::string>& gc = Melee::GroupClips(c ? c->group : "melee_1");
    const size_t idx = 5 + (size_t)std::clamp(strength, 0, 2);
    if (idx < gc.size())
        if (const AnimClip* h = Clip(LowerName(gc[idx]).c_str())) {
            FightEnd(true);
            Assoc* a = Add(h, false, false);
            a->partial = true;
            a->blend = 0.0f;
            a->delta = 8.0f;
            a->time = 0.0f;
            a->playing = true;
            a->autoRemove = true;
        }
    return true;
}

void PlayerPed::UpdateMelee(const Input& in) {
    const Melee::Combo* c = m_fight.active && m_fight.special ? m_fight.combo : CurrentCombo();
    if (m_fight.active && c != m_fight.combo) FightEnd(true);
    if (m_fight.active && !m_fight.special && m_gun >= 0) FightEnd(true);

    if (m_fight.active) {
        Assoc* a = Get(m_fight.clip);
        if (!a || a->dead) { m_fight = Fight(); return; }
        if (m_fight.block) {   // held guard: the clip waits at ABLOCK hit while the button stays down
            if (in.aim && in.duckHeld && a->time >= c->blockHit) { a->time = c->blockHit; a->playing = false; }
            else a->playing = true;
            if (a->finished) FightEnd(false);
            return;
        }
        const Melee::Move& mv = c->moves[m_fight.move];
        if (!m_fight.hitDone && a->time >= mv.hit) {
            // the strike sphere: ped position plus the heading-rotated level offset
            m_fight.hitDone = true;
            m_fight.queued = false;   // presses before the hit frame do not chain
            const float pos[3] = {in.worldX, in.worldY, in.feetZ + 1.0f};
            Strike s;
            Melee::StrikeCentre(m_meleeTable, mv.level, pos, m_heading, s.pos);
            s.radius = mv.radius;
            s.damage = mv.damage * FatMuscleModifier();
            std::fprintf(stderr, "[MELEE] strike damage %.3f (move %.1f x %.3f)\n", s.damage, mv.damage, FatMuscleModifier());
            s.move = m_fight.move;
            s.sword = c->name == "SWORD";
            if (m_melee < 0) s.force = 0.5f;   // WEAPON_UNARMED
            else {
                const std::string& wn = m_guns[m_melee].name;
                if (wn == "Brass knuckles" || wn == "Nightstick" || wn == "Chainsaw") s.force = 0.5f;
                else if (wn == "Golf club" || wn == "Baseball bat" || wn == "Shovel" || wn == "Pool cue" || wn == "Katana" || wn == "Purple dildo") s.force = 1.5f;
            }
            s.hitAudio = mv.hitAudio;
            s.altHitAudio = mv.altHitAudio;
            s.heading = m_heading;
            s.ground = m_fight.move == Melee::kGround;
            const std::vector<std::string>& gc = Melee::GroupClips(c->group);
            for (size_t i = 0; i < 3 && 5 + i < gc.size(); i++) s.hitClips[i] = gc[5 + i];
            m_strike = s;
            m_strikePending = true;
        } else if (m_fight.hitDone && in.fireJust) {
            m_fight.queued = true;
        }
        // AGROUND: the stomp loops back to the ground-loop frame while the button stays down
        if (m_fight.hitDone && m_fight.move == Melee::kGround && in.fireHeld && in.groundTarget && a->time >= mv.chain) {
            a->time = mv.groundLoop;
            m_fight.hitDone = false;
            return;
        }
        if (m_fight.hitDone && m_fight.queued && a->time >= mv.chain) {
            int next = -1;
            if (m_fight.special) next = -1;
            else if (m_fight.move == Melee::kAttack1 && c->Has(1)) next = Melee::kAttack2;
            else if (m_fight.move == Melee::kAttack2 && c->Has(2)) next = Melee::kAttack3;
            if (next >= 0) { FightStart(in, next); return; }
        }
        if (a->finished) FightEnd(false);
        return;
    }
    if (m_busy || m_inAir || m_swimming || m_climbing || m_seated) return;

    // CTaskSimpleUseGun PISTOLWHIP: a gun in the aim, a standing ped within a metre in front, the butt comes down
    if (m_gun >= 0 && m_aiming && in.fireJust && in.whipTarget) {
        if (const Melee::Combo* w = m_meleeTable.Find("PISTOL_WHIP"))
            FightStart(in, m_ducking ? Melee::kAttack2 : Melee::kAttack1, w);
        return;
    }
    // CTaskSimpleStealthKill: the knife in hand, locked on, an unaware ped's back in reach
    if (m_melee >= 0 && m_guns[m_melee].comboName == "KNIFE" && m_gun < 0 && in.aim && in.fireJust && in.stealthTarget) {
        if (const AnimClip* k = Clip("kill_knife_player")) {
            m_heading = m_aimHeading = std::atan2(in.stealthPos[1] - in.worldY, in.stealthPos[0] - in.worldX);
            Blend(k, false, false, 8.0f);
            m_busy = true;
            m_busySoft = false;
            m_busyClip = k;
            m_busyNext = nullptr;
            m_stealthPending = true;
            std::fprintf(stderr, "[MELEE] stealth kill\n");
            return;
        }
    }
    if (!c) return;
    // the kick of a locked-on fist fight (KICK_STD): jump while the guard is up
    if (m_gun < 0 && m_melee < 0 && in.aim && in.jumpDown) {
        if (const Melee::Combo* k = m_meleeTable.Find("KICK_STD")) { FightStart(in, Melee::kAttack1, k); return; }
    }
    // the guard: the duck button while locked on
    if (m_gun < 0 && in.aim && in.duckHeld) { FightStart(in, kBlockMove); return; }

    if (in.fireJust) {
        if (in.groundTarget && c->Has(4)) { FightStart(in, Melee::kGround); return; }
        const bool running = m_ratio > 1.5f;
        FightStart(in, running && c->Has(8) ? Melee::kMoving : Melee::kAttack1);
    }
}
