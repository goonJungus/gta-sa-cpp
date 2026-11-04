#include "InterestingEvents.h"

#include <cmath>

InterestingEvents::InterestingEvents() {
    // The constructor: every type waits 2 s at priority 5, then its own rows.
    for (int i = 0; i < kTypes; i++) { m_priority[i] = 5; m_delay[i] = 2000; m_endOfTime[i] = 0; }
    static const int kPrio[][2] = {{28, 10}, {27, 10}, {26, 9}, {25, 9}, {24, 9}, {23, 9}, {22, 9}, {16, 9}, {15, 9},
                                   {3, 1}, {2, 1}, {1, 1}, {20, 7}, {18, 7}, {19, 6}, {12, 6}, {13, 6}, {11, 6},
                                   {7, 2}, {6, 2}, {5, 2}, {4, 2}, {21, 8}, {17, 8}, {14, 5}, {10, 5}, {9, 4}, {8, 4}};
    for (const auto& p : kPrio) m_priority[p[0]] = (uint8_t)p[1];
    static const int kDelay[][2] = {{28, 4000}, {27, 4000}, {26, 8000}, {25, 6000}, {24, 6000}, {23, 6000}, {21, 8000},
                                    {20, 6000}, {18, 6000}, {17, 6000}, {16, 6000}, {15, 6000}, {14, 6000}, {12, 8000},
                                    {11, 6000}, {10, 6000}, {22, 5000}, {19, 5000}, {13, 5000}, {9, 3000}, {8, 3000},
                                    {7, 3000}, {6, 3000}, {5, 3000}, {4, 3000}, {3, 5000}, {2, 5000}, {1, 5000}};
    for (const auto& d : kDelay) m_delay[d[0]] = (uint16_t)d[1];
}

void InterestingEvents::Add(int type, const Ref& ref, const Frame& f) {
    if (ref.Empty() || type <= 0 || type >= kTypes || !f.resolve || !f.los) return;
    // The search circle: m_fRadius ahead of the player, along the line from the camera through him.
    float dx = f.player[0] - f.cam[0], dy = f.player[1] - f.cam[1];
    const float l = std::sqrt(dx * dx + dy * dy);
    if (l == 0.0f) { dx = f.playerFwd[0]; dy = f.playerFwd[1]; }
    else { dx /= l; dy /= l; }
    m_dir[0] = dx; m_dir[1] = dy; m_dir[2] = 0.0f;
    m_centre[0] = f.player[0] + m_radius * dx;
    m_centre[1] = f.player[1] + m_radius * dy;
    m_centre[2] = f.player[2];
    float ep[3];
    bool ped = false; float up = 0.0f;
    if (!f.resolve(ref, ep, &ped, &up)) return;
    const float cx = m_centre[0] - ep[0], cy = m_centre[1] - ep[1];
    if (cx * cx + cy * cy > m_radius * m_radius) return;
    // flag 2: nothing behind the camera
    if (dx * ep[0] + dy * ep[1] - (f.cam[0] * dx + f.cam[1] * dy) < 0.0f) return;
    if (!f.los(f.cam, ep)) return;
    for (int i = 0; i < 8; i++) {
        Event& e = m_events[i];
        const bool empty = e.ref.Empty();
        if (!empty) {
            const bool outranked = m_priority[e.type] <= m_priority[type] || (uint32_t)m_delay[e.type] + e.time < f.now;
            if (!(e.type == 0 || (outranked && m_endOfTime[type] < f.now && m_current != i))) continue;
        }
        e.type = type; e.ref = ref; e.time = f.now;
        m_endOfTime[type] = f.now + (m_delay[type] >> 1);   // flag 8: the same kind is not re-added for half its delay
        return;
    }
}

const InterestingEvents::Event* InterestingEvents::Get(uint32_t now) {
    if (m_current >= 0 && !m_events[m_current].ref.Empty() && (uint32_t)m_delay[m_events[m_current].type] + m_events[m_current].time > now)
        return &m_events[m_current];   // flag 4: keep the one being watched until its delay is up
    int best = -1, bestPrio = 0;
    for (int i = 0; i < 8; i++) {
        const Event& e = m_events[i];
        if (e.ref.Empty() || now >= (uint32_t)m_delay[e.type] + e.time) continue;
        m_rng = m_rng * 1664525u + 1013904223u;
        if (bestPrio < m_priority[e.type] || ((m_rng >> 8) & 0xffff) < 0x80) { bestPrio = m_priority[e.type]; best = i; }
    }
    m_current = best;
    return best < 0 ? nullptr : &m_events[best];
}

void InterestingEvents::Invalidate(const Event* ev) {
    for (int i = 0; i < 8; i++) {
        if (&m_events[i] != ev) continue;
        m_events[i].time = 0;
        m_events[i].ref = Ref();
        if (m_current == i) m_current = -1;
        return;
    }
}

void InterestingEvents::InvalidateNonVisible(const Frame& f) {
    if (!f.los || !f.resolve) return;
    for (int i = 0; i < 8; i++) {
        Event& e = m_events[i];
        if (e.ref.Empty()) continue;
        float ep[3]; bool ped = false; float up = 0.0f;
        if (f.resolve(e.ref, ep, &ped, &up) && f.los(f.cam, ep)) continue;
        e.time = 0;
        e.ref = Ref();
        if (m_current == i) m_current = -1;
    }
}

// ScanForNearbyEntities: every half second the peds and cars near the centre are offered as events: a cop,
// a criminal, a prostitute or a gang member (priority 2) and a car (priority 5).
void InterestingEvents::Scan(const Frame& f, const std::vector<Candidate>& around) {
    if (!ScanDue(f.now)) return;
    m_lastScan = f.now;
    for (const Candidate& c : around) {
        int type;
        if (c.pedType < 0) type = Vehicle;
        else if (c.pedType == 6) type = Cop;
        else if (c.pedType == 20) type = Criminal;
        else if (c.pedType == 22) type = Prostitute;
        else if (c.pedType >= 7 && c.pedType <= 16) type = Gang;
        else continue;
        Add(type, c.ref, f);
    }
}
