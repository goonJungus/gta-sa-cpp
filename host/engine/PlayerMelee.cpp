// CJ's melee: CTaskSimpleFight on data\melee.dat. The swing clips come from the
// weapon's anim group (Melee.cpp); each move strikes once at its hit frame, and
// a press after the hit frame chains the next attack at or after the chain frame.
// See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"

#include <algorithm>
#include <cctype>
#include <cmath>

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

// CStats::GetFatAndMuscleModifier(4): stats 0..1000 (muscle 0x17, fat 0x15).
float PlayerPed::FatMuscleModifier() const {
    const float muscle = m_muscleStat * 1000.0f, fat = m_fatStat * 1000.0f;
    return std::min(2.0f, 1.0f + (muscle - 50.0f) / 950.0f + (fat - 200.0f) * 0.5f / 800.0f);
}

void PlayerPed::FightStart(const Input& in, int move) {
    const Melee::Combo* c = CurrentCombo();
    if (!c) return;
    const std::vector<std::string>& cl = Melee::GroupClips(c->group);
    const size_t idx = move == Melee::kMoving ? 4 : (size_t)move;
    if (idx >= cl.size()) return;
    const AnimClip* clip = Clip(LowerName(cl[idx]).c_str());
    if (!clip) return;
    if (m_fight.active && m_fight.clip != clip)
        if (Assoc* o = Get(m_fight.clip)) o->delta = -8.0f;
    Assoc* a = Add(clip, false, false);
    a->partial = true;
    a->blend = 0.0f;
    a->delta = 8.0f;
    m_fight.active = true;
    m_fight.combo = c;
    m_fight.move = move;
    m_fight.clip = clip;
    m_fight.hitDone = false;
    m_fight.queued = false;
    (void)in;
}

void PlayerPed::FightEnd(bool abrupt) {
    if (m_fight.active && m_fight.clip)
        if (Assoc* a = Get(m_fight.clip)) a->delta = abrupt ? -16.0f : -8.0f;
    m_fight = Fight();
}

void PlayerPed::UpdateMelee(const Input& in) {
    const Melee::Combo* c = CurrentCombo();
    if (m_fight.active && c != m_fight.combo) FightEnd(true);
    if (!c) return;

    if (m_fight.active) {
        Assoc* a = Get(m_fight.clip);
        if (!a || a->dead) { m_fight = Fight(); return; }
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
            s.move = m_fight.move;
            s.sword = c->name == "SWORD";
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
        if (m_fight.hitDone && m_fight.queued && a->time >= mv.chain) {
            int next = -1;
            if (m_fight.move == Melee::kAttack1 && c->Has(1)) next = Melee::kAttack2;
            else if (m_fight.move == Melee::kAttack2 && c->Has(2)) next = Melee::kAttack3;
            if (next >= 0) { FightStart(in, next); return; }
        }
        if (a->finished) FightEnd(false);
        return;
    }

    if (in.fireJust && !m_busy && !m_inAir && !m_swimming && !m_climbing) {
        const bool running = m_ratio > 1.5f;
        FightStart(in, running && c->Has(8) ? Melee::kMoving : Melee::kAttack1);
    }
}
