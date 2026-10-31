#include "Train.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {
// CTrain::TrainModelArray: the model ids of each train type, engine first (read from the retail table).
const int kTrainModels[16][6] = {
    {537, 569, 569, 569, 569, 0}, {538, 570, 570, 0, 0, 0}, {538, 570, 570, 0, 0, 0}, {537, 569, 569, 569, 0, 0},
    {538, 570, 570, 0, 0, 0},     {538, 570, 570, 570, 0, 0}, {537, 569, 569, 0, 0, 0}, {538, 570, 570, 0, 0, 0},
    {449, 449, 0, 0, 0, 0},       {449, 0, 0, 0, 0, 0},       {537, 569, 0, 0, 0, 0},   {538, 570, 570, 0, 0, 0},
    {537, 569, 569, 569, 537, 0}, {537, 569, 569, 569, 569, 569}, {449, 0, 0, 0, 0, 0}, {538, 0, 0, 0, 0, 0}};
const char* kTrackFile[4] = {"tracks.dat", "tracks3.dat", "tracks2.dat", "tracks4.dat"};   // InitTrains order
// CTrain::aStationCoors: Unity Station, Market, Cranberry, Yellow Bell, Linden, Sobell Rail Yards
const float kStationCoors[6][3] = {{1741.0f, -1954.0f, 15.0f}, {1297.0f, -1898.0f, 3.0f}, {-1945.0f, 128.0f, 29.0f},
                                   {1434.0f, 2632.0f, 13.0f},  {2783.0f, 1758.0f, 12.0f}, {2865.0f, 1281.0f, 12.0f}};
constexpr float kDerailSpeed = 1.0f;     // 0x8D4944: per time step (50 m/s)
constexpr float kDerailBend = 0.996f;    // 0x8D4940: the cosine of the bend between the last two segments
}

// CTrain::ReadAndInterpretTrackFile
bool Trains::ReadTrack(const std::string& path, std::vector<TrainNode>& nodes, float* total) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string line;
    std::getline(f, line);
    if (line.compare(0, 9, "processed") == 0) std::getline(f, line);   // a "processed" header precedes the count
    const int count = std::atoi(line.c_str());
    if (count <= 0) return false;
    nodes.assign((size_t)count, TrainNode());
    for (int i = 0; i < count && std::getline(f, line); i++) {
        float x = 0, y = 0, z = 0;
        int flag = 0;
        std::sscanf(line.c_str(), "%f %f %f %d", &x, &y, &z, &flag);
        nodes[(size_t)i].x = (int16_t)(int)(x * 8.0f);
        nodes[(size_t)i].y = (int16_t)(int)(y * 8.0f);
        nodes[(size_t)i].z = (int16_t)(int)(z * 8.0f);
    }
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        nodes[(size_t)i].dist = (uint16_t)(int)(sum * 3.0f);
        const TrainNode& a = nodes[(size_t)i];
        const TrainNode& b = nodes[(size_t)((i + 1) % count)];
        const float dx = (a.x - b.x) * 0.125f, dy = (a.y - b.y) * 0.125f;
        sum += std::sqrt(dx * dx + dy * dy);
    }
    *total = sum;
    return true;
}

bool Trains::Load(const std::string& gameDir) {
    for (int t = 0; t < 4; t++) {
        m_track[t].clear();
        m_total[t] = 0.0f;
        bool ok = false;
        for (const char* dir : {"/data/Paths/", "/data/paths/"}) {
            if (ReadTrack(gameDir + dir + kTrackFile[t], m_track[t], &m_total[t])) { ok = true; break; }
        }
        if (ok) std::fprintf(stderr, "[TRAIN] %s: %zu nodes, %.1f m\n", kTrackFile[t], m_track[t].size(), m_total[t]);
        else if (t == 0) return false;
    }
    for (int s = 0; s < 6; s++) {   // InitTrains: the nearest node's index, read on track 0
        int track = 0;
        const size_t node = (size_t)FindClosestTrackNode(kStationCoors[s], &track);
        m_stationDist[s] = node < m_track[0].size() ? m_track[0][node].dist * 0.33333334f : 0.0f;
    }
    m_genStatus = 0;
    return true;
}

int Trains::FindClosestTrackNode(const float p[3], int* outTrack) const {
    float best = 99999.9f;
    int node = 0;
    for (int t = 0; t < 4; t++)
        for (size_t i = 0; i < m_track[t].size(); i++) {
            const TrainNode& n = m_track[t][i];
            const float dx = p[0] - n.x * 0.125f, dy = p[1] - n.y * 0.125f, dz = p[2] - n.z * 0.125f;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d < best) { best = d; *outTrack = t; node = (int)i; }
        }
    return node;
}

bool Trains::CoorsFromRail(float rail, int track, float out[3]) const {
    const std::vector<TrainNode>& nd = m_track[track];
    const size_t n = nd.size();
    for (size_t i = 0; i < n; i++) {
        const TrainNode& a = nd[i];
        const TrainNode& b = nd[(i + 1) % n];
        const float da = rail - a.dist * 0.33333334f, db = b.dist * 0.33333334f - rail;
        if (da < 0.0f || db < 0.0f) continue;
        const float k = 1.0f / (da + db);
        out[0] = (a.x * 0.125f * db + b.x * 0.125f * da) * k;
        out[1] = (a.y * 0.125f * db + b.y * 0.125f * da) * k;
        out[2] = (a.z * 0.125f * db + b.z * 0.125f * da) * k;
        return true;
    }
    return false;
}

float Trains::ModelLength(Vehicles& v, int model) const {
    float lo = 0, hi = 0, z = 0;
    return v.ModelBounds(model, &lo, &hi, &z) ? hi - lo : 0.0f;
}

// CTrain::FindPositionOnTrackFromCoors: the rail distance of the point on the track within 3 m of pos (xy).
void Trains::FindPositionOnTrack(Vehicles& v, TrainCarriage& c, const float pos[3]) {
    const std::vector<TrainNode>& nd = m_track[c.track];
    const size_t n = nd.size();
    for (size_t i = 0; i < n; i++) {
        const TrainNode& a = nd[i];
        const TrainNode& b = nd[(i + 1) % n];
        const float ax = a.x * 0.125f, ay = a.y * 0.125f, bx = b.x * 0.125f, by = b.y * 0.125f;
        const float seg = std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
        const float k = ((pos[0] - ax) * (bx - ax) + (pos[1] - ay) * (by - ay)) / (seg * seg);
        if (!(k > 0.001f && k < 1.001f)) continue;
        const float dx = (bx - ax) * k + ax - pos[0], dy = (by - ay) * k + ay - pos[1];
        if (std::sqrt(dx * dx + dy * dy) >= 3.0f) continue;
        float d = a.dist * 0.33333334f;
        d = (b.dist * 0.33333334f - d) * k + d;
        d -= ModelLength(v, v.Car((size_t)c.car).model) * 0.5f;
        if (d < 0.0f) d += m_total[c.track];
        c.rail = d;
        c.speed = 0.0f;   // m_vecMoveSpeed is zero for a fresh train
        return;
    }
}

int Trains::CreateMissionTrain(Vehicles& v, D3DRenderer& r, const float pos[3], bool clockwise, int type) {
    if (type < 0 || type > 15 || !Loaded()) return -1;
    int track = 0;
    const int node = FindClosestTrackNode(pos, &track);
    const int t = CreateTrainAtNode(v, r, track, node, clockwise, type, true);
    if (t >= 0) {   // the engine sits where the script asked, the carriages behind it
        FindPositionOnTrack(v, m_cars[(size_t)t], pos);
        float rail = m_cars[(size_t)t].rail;
        for (int i = m_cars[(size_t)t].next; i >= 0; i = m_cars[(size_t)i].next) {
            rail += m_cars[(size_t)i].length;
            m_cars[(size_t)i].rail = rail;
        }
        for (int i = t; i >= 0; i = m_cars[(size_t)i].next) Place(v, m_cars[(size_t)i], 0.0f);
    }
    return t;
}

int Trains::CreateTrainAtNode(Vehicles& v, D3DRenderer& r, int track, int node, bool clockwise, int type, bool mission) {
    if (type < 0 || type > 15 || !Loaded() || node < 0 || (size_t)node >= m_track[track].size()) return -1;
    float pos[3] = {0, 0, 0};
    CoorsFromRail(m_track[track][(size_t)node].dist * 0.33333334f, track, pos);
    float rail = m_track[track][(size_t)node].dist * 0.33333334f;
    int first = -1, prev = -1;
    for (int k = 0; k < 6 && kTrainModels[type][k] != 0; k++) {
        const int model = kTrainModels[type][k];
        const int car = v.Spawn(r, model, pos[0], pos[1], pos[2], 0.0f);
        if (car < 0) { std::fprintf(stderr, "[TRAIN] model %d did not load\n", model); continue; }
        TrainCarriage c;
        c.car = car; c.track = track; c.node = node; c.rail = rail;
        c.clockwise = clockwise; c.mission = mission; c.stopsAtStations = !mission;
        c.engine = prev < 0; c.last = true;
        if (prev < 0) {
            c.length = 0.0f;
        } else if (clockwise) {
            const float len = ModelLength(v, model);
            c.length = -len;
            rail -= len;
            c.rail = rail;
        } else {
            const float len = ModelLength(v, v.Car((size_t)m_cars[(size_t)prev].car).model);
            c.length = len;
            rail += len;
            c.rail = rail;
        }
        c.prev = prev;
        m_cars.push_back(c);
        const int idx = (int)m_cars.size() - 1;
        if (prev >= 0) { m_cars[(size_t)prev].next = idx; m_cars[(size_t)prev].last = false; }
        if (first < 0) first = idx;
        prev = idx;
    }
    if (first < 0) return -1;
    for (int i = first; i >= 0; i = m_cars[(size_t)i].next) Place(v, m_cars[(size_t)i], 0.0f);
    std::fprintf(stderr, "[TRAIN] type %d on track %d node %d, %s, rail %.1f of %.1f\n", type, track, node, clockwise ? "clockwise" : "anticlockwise", m_cars[(size_t)first].rail, m_total[track]);
    return first;
}

void Trains::SetTrainSpeed(int t, float speed) {
    if (t < 0 || (size_t)t >= m_cars.size()) return;
    TrainCarriage& c = m_cars[(size_t)t];
    c.speed = speed * 0.02f;
    if (!c.clockwise) c.speed = -(speed * 0.02f);
}

void Trains::SetTrainCruiseSpeed(int t, float speed) {
    if (t < 0 || (size_t)t >= m_cars.size()) return;
    m_cars[(size_t)t].cruise = (float)(int)(unsigned char)(int)speed;   // stored as a byte
}

int Trains::FindCaboose(int t) const {
    if (t < 0 || (size_t)t >= m_cars.size()) return -1;
    while (m_cars[(size_t)t].next >= 0) t = m_cars[(size_t)t].next;
    return t;
}

int Trains::FindCarriage(int t, int n) const {
    if (t < 0 || (size_t)t >= m_cars.size()) return -1;
    for (int i = 0; i < n; i++) { t = m_cars[(size_t)t].next; if (t < 0) return -1; }
    return t;
}

int Trains::CarriageOfCar(int car) const {
    for (size_t i = 0; i < m_cars.size(); i++) if (!m_cars[i].removed && m_cars[i].car == car) return (int)i;
    return -1;
}

int Trains::FindNumCarriagesPulled(int t) const {
    int n = 0;
    for (t = m_cars[(size_t)t].next; t >= 0; t = m_cars[(size_t)t].next) n++;
    return n;
}

void Trains::RemoveTrain(Vehicles& v, int t) {
    while (t >= 0 && (size_t)t < m_cars.size()) {
        TrainCarriage& c = m_cars[(size_t)t];
        const int nx = c.next;
        c.removed = true;
        v.Car((size_t)c.car).removed = true;
        v.Car((size_t)c.car).hasRot = false;
        t = nx;
    }
}

void Trains::RemoveMissionTrains(Vehicles& v) {
    for (size_t i = 0; i < m_cars.size(); i++)
        if (!m_cars[i].removed && m_cars[i].mission) RemoveTrain(v, (int)i);
}

void Trains::ReleaseMissionTrains() {
    for (TrainCarriage& c : m_cars) c.mission = false;
}

void Trains::ReleaseTrain(int t) {
    while (t >= 0 && (size_t)t < m_cars.size()) { m_cars[(size_t)t].mission = false; t = m_cars[(size_t)t].next; }
}

// CTrain::FindMaximumSpeedToStopAtStations: the speed (script units) that brings the train to rest at the next
// station ahead on track 0 (50 within 500 m, falling to 0 at the platform); true once within 5 m of it.
bool Trains::MaxSpeedToStopAtStations(const TrainCarriage& c, float* speed) const {
    *speed = 50.0f;
    if (c.track != 0) return false;
    const float total = m_total[0];
    float nearest = 10000.0f;
    for (float station : m_stationDist) {
        float d = station - c.rail + (c.clockwise ? 40.0f : -40.0f);
        while (d > total * 0.5f) d -= total;
        while (d < total * -0.5f) d += total;
        if (!c.clockwise && d < 0.0f) nearest = std::min(nearest, -d);
        else if (c.clockwise && d > 0.0f) nearest = std::min(nearest, d);
    }
    *speed = nearest >= 500.0f ? 100000.0f : (1.0f - (500.0f - nearest) * 0.002f) * 50.0f;
    return nearest < 5.0f;
}

// The engine half of CTrain::ProcessControl: the pad (the player's train) or the cruise speed with the station
// stops (10 s with the doors open, 8 s more, then away after 22 s) sets gas and brake, and they change the speed
// shared out over the carriages pulled.
void Trains::DriveEngine(int i, float ts, const Frame& in) {
    TrainCarriage& c = m_cars[(size_t)i];
    float s = c.clockwise ? c.speed : -c.speed;   // forward along the train
    if (c.car == in.playerCar) {
        const float accel = in.accel * 255.0f, brake = in.brake * 255.0f;
        if (std::fabs(s) >= 0.001f) {
            if (s <= 0.0f) { c.gas = -brake; c.brake = accel; }
            else { c.brake = brake; c.gas = accel; }
        } else { c.brake = 0.0f; c.gas = accel - brake; }
    } else {
        float target = c.cruise;
        const uint32_t since = in.nowMs - c.stoppedMs;
        if (c.stoppedMs != 0 && since < 10000) {
            target = 0.0f;
            if (!c.stopped) for (int k = i; k >= 0; k = m_cars[(size_t)k].next) { m_cars[(size_t)k].stopped = m_cars[(size_t)k].doorsOpen = true; }
        } else if (c.stoppedMs != 0 && since < 18000) {
            target = 0.0f;
            if (c.stopped) for (int k = i; k >= 0; k = m_cars[(size_t)k].next) m_cars[(size_t)k].doorsOpen = false;
        } else if (c.stoppedMs != 0 && since < 22000) {
            if (c.stopped) for (int k = i; k >= 0; k = m_cars[(size_t)k].next) m_cars[(size_t)k].stopped = false;
        } else if (c.stopsAtStations) {
            float limit = 0.0f;
            if (MaxSpeedToStopAtStations(c, &limit)) { c.stoppedMs = in.nowMs ? in.nowMs : 1; target = 0.0f; }
            else if (limit <= target) target = limit;
        }
        const float want = target * 0.02f - s;
        if (want <= 0.0f) { c.gas = 0.0f; c.brake = std::min(want * -30.0f, 1.0f) * 255.0f; }
        else { c.brake = 0.0f; c.gas = std::min(want * 30.0f, 1.0f) * 255.0f; }
    }
    const float n = (float)(FindNumCarriagesPulled(i) + 3);
    s += c.gas * 0.00390625f * ts * 0.002f / n;
    if (c.brake != 0.0f) {
        const float dec = c.brake * 0.00390625f * ts * 0.006f / n;
        if (dec <= std::fabs(s)) s = s >= 0.0f ? s - dec : s + dec; else s = 0.0f;
    }
    s *= std::pow(0.9997f, ts);
    c.speed = c.clockwise ? s : -s;
    c.rail += ts * c.speed;
}

// CTrain::DoTrainGenerationAndRemoval: a random train that is all over 220 m from the camera (and not the
// player's) goes; with none about, one is set up at a track node 170..220 m from the camera once the camera is
// within 70 m of the line (100 m near a station, checked every frame there, else every 0.95 s), the train types
// cycling; in San Fierro the trams (track 1) come the same way.
void Trains::GenerateAndRemove(Vehicles& v, D3DRenderer& r, const Frame& in) {
    if (in.nowMs / 3000 != in.prevMs / 3000) {
        m_nearStation = false;
        for (const auto& s : kStationCoors)
            if (std::hypot(s[0] - in.player[0], s[1] - in.player[1]) < 60.0f) m_nearStation = true;
    }
    const uint32_t period = m_nearStation ? 1 : 950;
    const float genDist = m_nearStation ? 100.0f : 70.0f;
    if (in.nowMs / period == in.prevMs / period) return;
    bool anyTrain = false;
    for (size_t i = 0; i < m_cars.size(); i++) {
        if (m_cars[i].removed || !m_cars[i].engine || m_cars[i].mission) continue;
        anyTrain = true;
        bool away = true;
        for (int k = (int)i; k >= 0; k = m_cars[(size_t)k].next) {
            const VehicleInstance& car = v.Car((size_t)m_cars[(size_t)k].car);
            if (m_cars[(size_t)k].car == in.playerCar || std::hypot(in.cam[0] - car.x, in.cam[1] - car.y) < 220.0f) away = false;
        }
        if (away) RemoveTrain(v, (int)i);
    }
    if (!in.randomTrains) return;
    auto nodeDist = [&](int track, int node) {
        const TrainNode& n = m_track[track][(size_t)node];
        return std::hypot(n.x * 0.125f - in.cam[0], n.y * 0.125f - in.cam[1]);
    };
    auto walkAway = [&](float d) {   // along the track, away from the camera
        const int count = (int)m_track[m_genTrack].size();
        while (d < 170.0f) {
            m_genNode = m_genClockwise ? (m_genNode - 1 + count) % count : (m_genNode + 1) % count;
            d = nodeDist(m_genTrack, m_genNode);
        }
        return d;
    };
    if (m_genStatus == 1) {
        if (nodeDist(m_genTrack, m_genNode) > 60.0f) {
            const int t = CreateTrainAtNode(v, r, m_genTrack, m_genNode, m_genClockwise, m_genConfig, false);
            if (t >= 0) {
                int cruise;
                if (m_genTrack == 0) {
                    cruise = (int)(std::rand() % 30) + 15;
                    const TrainNode& n = m_track[0][(size_t)m_genNode];
                    if (std::hypot(n.x * 0.125f - 2222.0f, n.y * 0.125f + 1750.0f) < 300.0f) cruise /= 2;   // slow through Los Santos
                } else cruise = (int)(std::rand() % 7) + 7;
                float limit = 0.0f;
                MaxSpeedToStopAtStations(m_cars[(size_t)t], &limit);
                SetTrainSpeed(t, std::min((float)cruise, limit));
                m_cars[(size_t)t].cruise = (float)cruise;
            }
        }
        m_genStatus = 0;
        return;
    }
    if (m_genStatus != 0 || anyTrain || m_track[0].empty()) return;
    m_genTrack = 0;
    m_genNode = (int)(std::rand() % (int)m_track[0].size());
    float d = nodeDist(0, m_genNode);
    if (d < genDist) {
        m_genClockwise = (std::rand() & 1) != 0;
        const int before = m_genNode;
        d = walkAway(d);
        m_genConfig = m_lastConfig = (m_lastConfig + 1) & 7;
        m_genStatus = 1;
        if (d > 220.0f) m_genNode = before;
        return;
    }
    const bool sf = in.cam[0] < -1430.0f && in.cam[1] > -580.0f && in.cam[1] < 1430.0f;   // CWeather::FindWeatherRegion
    if (sf && !m_track[1].empty()) {
        m_genTrack = 1;
        m_genNode = (int)(std::rand() % (int)m_track[1].size());
        d = nodeDist(1, m_genNode);
        if (d < genDist) {
            m_genClockwise = (std::rand() & 1) != 0;
            d = walkAway(d);
            if (d < 220.0f) { m_genStatus = 1; m_genConfig = 8 + (std::rand() & 1); }
        }
    }
}

// CTrain::ProcessControl for every train, then each carriage is placed on its rail. The player's engine shakes
// near its top speed, and past it in a bend (out of the tunnels) the whole train leaves the rails.
int Trains::Update(Vehicles& v, D3DRenderer& r, float dt, const Frame& in) {
    if (dt <= 0.0f || !Loaded()) return 0;
    GenerateAndRemove(v, r, in);
    const float ts = dt * 50.0f;   // CTimer::ms_fTimeStep
    int events = 0;
    for (size_t i = 0; i < m_cars.size(); i++) {
        TrainCarriage& c = m_cars[i];
        if (c.removed || c.derailed) continue;
        if (c.engine) DriveEngine((int)i, ts, in);
        else if (c.prev >= 0) {
            const TrainCarriage& p = m_cars[(size_t)c.prev];
            c.speed = p.speed;
            c.rail = p.rail + c.length;
        }
        Place(v, c, dt);
        if (!c.engine || c.car != in.playerCar) continue;
        const float speed = std::fabs(c.speed);
        if (speed > kDerailSpeed * 0.95f) events |= kShake;
        if (speed <= kDerailSpeed) continue;
        const std::vector<TrainNode>& nd = m_track[c.track];
        const int n = (int)nd.size();
        const int a = (c.node - 1 + n) % n, b = (c.node - 2 + n) % n;
        auto seg = [&](int from, int to, float out[3]) {
            out[0] = (nd[(size_t)to].x - nd[(size_t)from].x) * 0.125f;
            out[1] = (nd[(size_t)to].y - nd[(size_t)from].y) * 0.125f;
            out[2] = (nd[(size_t)to].z - nd[(size_t)from].z) * 0.125f;
            const float l = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
            if (l > 0.0f) for (int k = 0; k < 3; k++) out[k] /= l;
        };
        float s1[3], s2[3];
        seg(a, c.node, s1);
        seg(b, a, s2);
        if (s1[0] * s2[0] + s1[1] * s2[1] + s1[2] * s2[2] >= kDerailBend) continue;
        for (int k = (int)i; k >= 0; k = m_cars[(size_t)k].next) {   // off the rails: physics takes every carriage
            TrainCarriage& tc = m_cars[(size_t)k];
            tc.derailed = true;
            VehicleInstance& car = v.Car((size_t)tc.car);
            const float* fwd = car.rot + 3;
            const float step = tc.clockwise ? tc.speed : -tc.speed;
            car.hasRot = false;
            car.derailed = true;
            car.phys = CarPhys{};
            car.phys.active = true;
            for (int q = 0; q < 3; q++) car.phys.mv[q] = fwd[q] * step;
            for (uint8_t& w : car.wheelStatus) w = 2;   // no wheels on the ground any more
        }
        events |= kDerailed;
    }
    return events;
}

// The second half of ProcessControl: wrap the distance, find the segment and put the car there. The body sits
// midway between the rail points under its front and rear, facing from one to the other.
void Trains::Place(Vehicles& v, TrainCarriage& c, float dt) {
    const std::vector<TrainNode>& nd = m_track[c.track];
    const int n = (int)nd.size();
    const float total = m_total[c.track];
    if (n < 2 || total <= 0.0f) return;
    while (c.rail < 0.0f) c.rail += total;
    while (c.rail >= total) c.rail -= total;
    auto locate = [&](float d, int idx, float out[3]) {
        auto distAt = [&](int i) { return nd[(size_t)i].dist * 0.33333334f; };
        for (int guard = 0; guard < n + 2; guard++) {
            const float lo = distAt(idx);
            const int nx = idx + 1 < n ? idx + 1 : 0;
            const float hi = idx + 1 < n ? distAt(nx) : total;
            if (d >= lo && d <= hi) {
                float seg = distAt(nx) - lo;
                if (seg < 0.0f) seg += total;
                const float k = seg > 0.0f ? (d - lo) / seg : 0.0f;
                out[0] = nd[(size_t)nx].x * 0.125f * k + nd[(size_t)idx].x * 0.125f * (1.0f - k);
                out[1] = nd[(size_t)nx].y * 0.125f * k + nd[(size_t)idx].y * 0.125f * (1.0f - k);
                out[2] = nd[(size_t)nx].z * 0.125f * k + nd[(size_t)idx].z * 0.125f * (1.0f - k);
                return idx;
            }
            idx = (d < lo ? idx - 1 + n : idx + 1) % n;
        }
        return idx;
    };
    VehicleInstance& car = v.Car((size_t)c.car);
    float a[3], b[3];
    c.node = locate(c.rail, c.node, a);
    float rear = c.rail + ModelLength(v, car.model);
    if (rear > total) rear -= total;
    locate(rear, c.node, b);
    float lo = 0, hi = 0, minZ = 0;
    v.ModelBounds(car.model, &lo, &hi, &minZ);
    const float ox = car.x, oy = car.y, oz = car.z;
    car.x = (a[0] + b[0]) * 0.5f;
    car.y = (a[1] + b[1]) * 0.5f;
    car.z = (a[2] + b[2]) * 0.5f - minZ;
    float f[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float len = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (len > 1e-6f) for (float& q : f) q /= len;
    if (!c.clockwise) for (float& q : f) q = -q;
    // right = forward x up(0,0,1), up = right x forward
    float rt[3] = {f[1], -f[0], 0.0f};
    const float rl = std::sqrt(rt[0] * rt[0] + rt[1] * rt[1]);
    if (rl > 1e-6f) { rt[0] /= rl; rt[1] /= rl; } else { rt[0] = 1.0f; rt[1] = 0.0f; }
    const float up[3] = {rt[1] * f[2] - rt[2] * f[1], rt[2] * f[0] - rt[0] * f[2], rt[0] * f[1] - rt[1] * f[0]};
    std::memcpy(car.rot, rt, sizeof(rt));
    std::memcpy(car.rot + 3, f, sizeof(f));
    std::memcpy(car.rot + 6, up, sizeof(up));
    car.hasRot = true;
    car.heading = std::atan2(-f[0], f[1]);
    if (dt > 0.0f) {
        car.vx = (car.x - ox) / dt; car.vy = (car.y - oy) / dt;
        car.speed = std::sqrt(car.vx * car.vx + car.vy * car.vy);
        (void)oz;
    }
}
