#include "GameLogic.h"

#include "Stats.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>

void GameLogic::Log(const char* fmt, ...) {
    if (!hooks.log) return;
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    hooks.log(b);
}

// map.zon: "name, type, x1, y1, z1, x2, y2, z2, level, text" between "zone" and "end".
void GameLogic::Init(const std::string& gameDir) {
    m_zones.clear();
    std::ifstream f(gameDir + "/data/map.zon");
    std::string line;
    bool in = false;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("zone", 0) == 0) { in = true; continue; }
        if (line.rfind("end", 0) == 0) { in = false; continue; }
        if (!in || line.empty() || line[0] == '#') continue;
        for (char& c : line) if (c == ',') c = ' ';
        std::istringstream s(line);
        std::string name;
        int type = 0;
        Zone z{};
        if (s >> name >> type >> z.x0 >> z.y0 >> z.z0 >> z.x1 >> z.y1 >> z.z1 >> z.level) m_zones.push_back(z);
    }
}

// CTheZones::GetLevelFromPosition: the level of the map zone the point is in, else the countryside.
int GameLogic::LevelAt(float x, float y, float z) const {
    for (const Zone& q : m_zones)
        if (x >= q.x0 && x <= q.x1 && y >= q.y0 && y <= q.y1 && z >= q.z0 && z <= q.z1) return q.level;
    return 0;
}

// The ADD_HOSPITAL_RESTART / ADD_POLICE_RESTART calls at the start of the retail main script.
void GameLogic::AddRetailRestartPoints() {
    static const Point kHosp[] = {
        {2027.77f, -1420.52f, 15.99f, 137.0f, 0}, {1180.85f, -1325.57f, 12.58f, 271.4f, 0},
        {1244.44f, 331.23f, 18.55f, 7.55f, 1},    {-2199.72f, -2308.07f, 29.62f, 322.89f, 1},
        {-2670.29f, 616.44f, 13.45f, 183.1f, 1},  {-316.38f, 1056.05f, 18.73f, 1.6f, 2},
        {-1514.82f, 2527.12f, 54.74f, 2.35f, 2},  {1578.45f, 1770.68f, 9.84f, 99.76f, 2},
    };
    static const Point kPolice[] = {
        {1550.68f, -1675.49f, 14.51f, 90.0f, 0},   {632.23f, -571.71f, 15.35f, 267.2f, 1},
        {-2163.83f, -2387.82f, 29.62f, 134.21f, 1}, {-1605.79f, 716.86f, 11.02f, 355.3f, 1},
        {-212.19f, 979.42f, 18.32f, 278.05f, 2},   {-1393.07f, 2633.12f, 54.95f, 86.04f, 2},
        {2337.08f, 2453.8f, 13.98f, 90.76f, 2},
    };
    for (const Point& p : kHosp) AddHospitalRestart(p.x, p.y, p.z, p.heading, p.town);
    for (const Point& p : kPolice) AddPoliceRestart(p.x, p.y, p.z, p.heading, p.town);
}

void GameLogic::AddHospitalRestart(float x, float y, float z, float heading, int town) {
    if (m_hospitals.size() < 10) m_hospitals.push_back({x, y, z, heading, town});   // CRestart: room for 10
}

void GameLogic::AddPoliceRestart(float x, float y, float z, float heading, int town) {
    if (m_police.size() < 10) m_police.push_back({x, y, z, heading, town});
}

void GameLogic::OverrideNextRestart(float x, float y, float z, float heading) {
    m_override = true;
    m_overridePos[0] = x; m_overridePos[1] = y; m_overridePos[2] = z;
    m_overrideHeading = heading;
}

void GameLogic::SetExtraHospitalRestart(float x, float y, float z, float radius, float heading) {
    m_extraHosp[0] = x; m_extraHosp[1] = y; m_extraHosp[2] = z;
    m_extraHospRadius = radius; m_extraHospHeading = heading;
}

void GameLogic::SetExtraPoliceRestart(float x, float y, float z, float radius, float heading) {
    m_extraPolice[0] = x; m_extraPolice[1] = y; m_extraPolice[2] = z;
    m_extraPoliceRadius = radius; m_extraPoliceHeading = heading;
}

// CRestart::FindClosestHospitalRestartPoint / FindClosestPoliceRestartPoint: the override first, then the
// extra point when within its radius, else the nearest point of an unlocked town (STAT_CITY_UNLOCKED), one in
// another level counting six times as far.
bool GameLogic::FindClosest(const std::vector<Point>& pts, bool extra, const float extraPos[3], float extraRadius,
                            float extraHeading, float x, float y, float z, float out[3], float& heading) {
    if (m_override) {
        out[0] = m_overridePos[0]; out[1] = m_overridePos[1]; out[2] = m_overridePos[2];
        heading = m_overrideHeading;
        m_override = false;
        return true;
    }
    if (extra && extraRadius > 0.0f && std::hypot(x - extraPos[0], y - extraPos[1]) < extraRadius) {
        out[0] = extraPos[0]; out[1] = extraPos[1]; out[2] = extraPos[2];
        heading = extraHeading;
        return true;
    }
    const int level = LevelAt(x, y, z);
    const float unlocked = Stats::Get(Stats::STAT_CITY_UNLOCKED);
    int best = -1;
    float bestD = 1e7f;
    for (size_t i = 0; i < pts.size(); i++) {
        const Point& p = pts[i];
        if ((float)p.town > unlocked) continue;
        float d = std::sqrt((x - p.x) * (x - p.x) + (y - p.y) * (y - p.y) + (z - p.z) * (z - p.z));
        if (level != 0 && LevelAt(p.x, p.y, p.z) != level) d *= 6.0f;
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    if (best < 0) return false;
    out[0] = pts[(size_t)best].x; out[1] = pts[(size_t)best].y; out[2] = pts[(size_t)best].z;
    heading = pts[(size_t)best].heading;
    return true;
}

bool GameLogic::FindClosestHospital(float x, float y, float z, float out[3], float& heading) {
    return FindClosest(m_hospitals, true, m_extraHosp, m_extraHospRadius, m_extraHospHeading, x, y, z, out, heading);
}

bool GameLogic::FindClosestPolice(float x, float y, float z, float out[3], float& heading) {
    return FindClosest(m_police, true, m_extraPolice, m_extraPoliceRadius, m_extraPoliceHeading, x, y, z, out, heading);
}

void GameLogic::ForceDeathRestart() {   // CGameLogic::ForceDeathRestart: straight to the fade
    m_state = State::Wasted;
    m_since = 3.0f;
}

void GameLogic::Update(float dt, bool dead, bool arrested, float px, float py, float pz) {
    // CPlayerInfo::KillPlayer / ArrestPlayer, from CGameLogic::Update
    if (m_state == State::Playing && dead) {
        m_state = State::Wasted;
        m_since = 0.0f;
        m_message = m_wastedText; m_msgTimer = 4.0f; m_msgAlpha = 0.0f;   // AddBigMessage("DEAD", 4000)
        Stats::Increment(Stats::STAT_NUMBER_OF_HOSPITAL_VISITS, 1.0f);
        Log("GAMELOGIC: wasted");
    } else if (m_state == State::Playing && arrested) {
        m_state = State::Busted;
        m_since = 0.0f;
        m_message = m_bustedText; m_msgTimer = 5.0f; m_msgAlpha = 0.0f;   // AddBigMessage("BUSTED", 5000)
        Stats::Increment(Stats::STAT_TIMES_BUSTED, 1.0f);
        Log("GAMELOGIC: busted");
    }

    // CHud::DrawBustedWastedMessage: the alpha climbs 0.4 a millisecond until the message runs out
    if (m_msgTimer > 0.0f) {
        m_msgTimer -= dt;
        m_msgAlpha = std::min(255.0f, m_msgAlpha + dt * 1000.0f * 0.4f);
        if (m_msgTimer <= 0.0f) m_message.clear();
    }

    // CCamera::Fade
    if (m_fadeDir != 0.0f) {
        m_fade = std::clamp(m_fade + m_fadeDir * m_fadeRate * dt, 0.0f, 1.0f);
        if ((m_fadeDir > 0.0f && m_fade >= 1.0f) || (m_fadeDir < 0.0f && m_fade <= 0.0f)) m_fadeDir = 0.0f;
    }

    if (m_state == State::Playing) return;
    const float before = m_since;
    m_since += dt;
    if (m_since > 3.0f && before <= 3.0f) { m_fadeDir = 1.0f; m_fadeRate = 0.5f; }   // Fade(2.0, to black)
    const bool fading = m_fadeDir != 0.0f;
    if (m_state == State::Wasted && (m_since <= 3.0f || fading)) return;
    if (m_state == State::Busted && (m_since < 4.0f || fading)) return;
    Restart(m_state == State::Wasted, px, py, pz);
}

void GameLogic::Restart(bool hospital, float px, float py, float pz) {
    if (hospital) {
        if (penaltyForDeath) {
            if (!freeHealthCare) {   // $100 at the hospital, never below nothing, and the weapons are gone
                if (hooks.money && hooks.setMoney && hooks.money() > 0) hooks.setMoney(std::max(0, hooks.money() - 100));
                if (hooks.clearWeapons) hooks.clearWeapons();
            } else freeHealthCare = false;
        }
    } else if (penaltyForArrest) {
        int fine = 100;   // by the wanted level at the arrest
        switch (hooks.wanted ? hooks.wanted() : 0) {
        case 2: fine = 200; break;
        case 3: fine = 400; break;
        case 4: fine = 600; break;
        case 5: fine = 900; break;
        case 6: fine = 1500; break;
        default: break;
        }
        if (!getOutOfJailFree) {
            if (hooks.money && hooks.setMoney && hooks.money() > 0) hooks.setMoney(std::max(0, hooks.money() - fine));
            if (hooks.clearWeapons) hooks.clearWeapons();
        } else getOutOfJailFree = false;
    }
    m_state = State::Playing;
    m_message.clear(); m_msgTimer = 0.0f;   // CHud::ResetWastedText
    if (hospital) m_hoursDidntEat = 0;      // fed in hospital

    float pos[3] = {px, py, pz}, heading = 0.0f;
    const bool found = hospital ? FindClosestHospital(px, py, pz, pos, heading) : FindClosestPolice(px, py, pz, pos, heading);
    if (hooks.passTime) hooks.passTime(720);   // PassTime(720): twelve hours
    if (hooks.resurrect) hooks.resurrect(pos[0], pos[1], pos[2], heading);
    Log("GAMELOGIC: restart at the %s %.1f %.1f %.1f%s", hospital ? "hospital" : "police station", pos[0], pos[1], pos[2],
        found ? "" : " (no restart point: where he fell)");

    bool& fadeIn = hospital ? fadeInAfterNextDeath : fadeInAfterNextArrest;
    if (fadeIn) { m_fadeDir = -1.0f; m_fadeRate = 1.0f / 3.0f; }   // Fade(3.0, back in)
    else {   // the script fades in when it is ready
        fadeIn = true;
        m_fade = 0.0f; m_fadeDir = 0.0f;
        if (hooks.holdBlack) hooks.holdBlack();
    }
}

void GameLogic::UpdateHunger(int hour, bool neverHungryCheat) {
    if (neverHungryCheat || m_state != State::Playing) return;
    if (m_hungerHour < 0) m_hungerHour = hour;
    const bool newHour = hour != m_hungerHour;
    m_hungerHour = hour;
    if (newHour) {
        if (m_hoursDidntEat == 0) m_lastHungryState = 0;
        m_hoursDidntEat++;
    }
    if (m_hoursDidntEat <= 48) { m_hungryShown = false; return; }
    if (!newHour) return;
    if (!m_hungryShown) {   // the first hungry hour: just the warning
        if (hooks.helpMessage) hooks.helpMessage("NOTEAT");
        m_hungryShown = true;
        Log("GAMELOGIC: hungry after %d hours", m_hoursDidntEat);
        return;
    }
    bool lostFat = false;
    if (Stats::Get(Stats::STAT_FAT) > 0.0f) {
        if (hooks.statDown) hooks.statDown(Stats::STAT_FAT, 25.0f);
        lostFat = true;
        if (m_lastHungryState == 0) m_lastHungryState = m_hoursDidntEat + 24;   // a day of fat before the muscle goes
    }
    if (Stats::Get(Stats::STAT_MUSCLE) <= 0.0f || (m_hoursDidntEat <= m_lastHungryState && m_lastHungryState != 0)) {
        if (!lostFat && hooks.loseHealth) hooks.loseHealth(2.0f);
    } else if (hooks.statDown) hooks.statDown(Stats::STAT_MUSCLE, 25.0f);
}
