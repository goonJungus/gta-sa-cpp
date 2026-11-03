#include "Vending.h"

#include "PlayerPed.h"
#include "Stats.h"
#include "World.h"

#include <cmath>

namespace {
// ALLOCATE_STREAMED_SCRIPT_TO_OBJECT in main.scm: the vending script's models; the candy ones sell snacks
// (DOES_OBJECT_HAVE_THIS_MODEL VENDIN3 / CJ_CANDYVENDOR / CJ_EXT_CANDY), the rest drinks.
const struct { const char* model; bool snack; } kModels[] = {
    {"vendmachfd", false}, {"vendmach", false}, {"vendin3", true}, {"cj_sprunk1", false},
    {"cj_candyvendor", true}, {"cj_ext_candy", true}, {"cj_ext_sprunk", false},
};
constexpr float kActivation = 6.0f;   // the brain's activation radius

// GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS
void Offset(const float pos[3], const float r[3], const float f[3], const float u[3], float ox, float oy, float oz, float out[3]) {
    for (int k = 0; k < 3; k++) out[k] = pos[k] + r[k] * ox + f[k] * oy + u[k] * oz;
}
}

void Vending::Init(const GameWorld& w) {
    m_machines.clear();
    for (const auto& m : kModels)
        for (size_t i : w.InstsOfModels({m.model})) m_machines.push_back({i, m.snack});
    Reset();
}

void Vending::Reset() {
    if (m_state >= 2 && hooks.help) hooks.help("");
    m_active = -1;
    m_state = 0;
}

void Vending::Update(const GameWorld& w, PlayerPed& cj, float px, float py, float pz, int area, bool canUse,
                     bool usePressed, uint32_t nowMs) {
    if (m_machines.empty()) return;
    auto frame = [&](int idx, float pos[3], float r[3], float f[3], float u[3], int& a) { w.InstFrame(m_machines[(size_t)idx].inst, pos, r, f, u, a); };

    if (m_state <= 2) {   // the nearest machine in its activation range owns the player
        int best = -1;
        float bestD = kActivation * kActivation;
        for (size_t i = 0; i < m_machines.size(); i++) {
            float pos[3], r[3], f[3], u[3];
            int a = 0;
            frame((int)i, pos, r, f, u, a);
            if ((a != area && a != 13) || w.InstHidden(m_machines[i].inst)) continue;
            const float d = (pos[0] - px) * (pos[0] - px) + (pos[1] - py) * (pos[1] - py);
            if (d < bestD) { bestD = d; best = (int)i; }
        }
        if (best != m_active) { Reset(); m_active = best; }
        if (m_active < 0) return;
    }
    if (m_state == 3 && cj.Dead()) { Reset(); return; }   // the script loses its player

    float pos[3], r[3], f[3], u[3];
    int a = 0;
    frame(m_active, pos, r, f, u, a);
    const bool snack = m_machines[(size_t)m_active].snack;
    float spot[3];
    Offset(pos, r, f, u, 0.0f, -1.0f, 0.5f, spot);
    // LOCATE_CHAR_ON_FOOT_3D 0.6 0.6 1.0
    const bool there = canUse && std::fabs(px - spot[0]) < 0.6f && std::fabs(py - spot[1]) < 0.6f && std::fabs(pz - spot[2]) < 1.0f;

    switch (m_state) {
    case 0: m_state = 1; break;   // REQUEST_ANIMATION VENDING (loaded with CJ)
    case 1:
        if (there) {
            if (hooks.help) hooks.help(snack ? "SLOT_04" : "SLOT_05");
            m_state = 2;
        }
        break;
    case 2:
        if (!there) { if (hooks.help) hooks.help(""); m_state = 1; break; }
        if (!usePressed) break;
        if (hooks.money && hooks.money() > 0) {   // IS_SCORE_GREATER 0
            float at[3];
            Offset(pos, r, f, u, 0.2f, -1.0f, 0.0f, at);
            if (!cj.StartVending(at[0], at[1], std::atan2(f[1], f[0]), snack)) break;   // GET_OBJECT_HEADING: he faces the machine
            if (hooks.addMoney) hooks.addMoney(-1);
            if (hooks.help) hooks.help("");
            if (hooks.sound) hooks.sound(350, snack ? 1 : 0);   // mission audio 42601 / 42600
            m_state = 3;
        }
        break;
    case 3:
        if (cj.TakeVendingDone()) {   // GET_SCRIPT_TASK_STATUS PERFORM_SEQUENCE_TASK == 7
            Stats::Increment(Stats::STAT_CALORIES, 5.0f);
            if (hooks.addHealth) hooks.addHealth(30.0f);
            m_until = nowMs + 1500;
            m_state = 4;
        }
        break;
    case 4:
        if (nowMs > m_until) m_state = 0;   // CLEAR_CHAR_TASKS, REMOVE_ANIMATION
        break;
    default: break;
    }
}
