#include "PathNodes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

struct Reader {
    std::vector<uint8_t> d;
    size_t p = 0;
    bool Ok(size_t n) const { return p + n <= d.size(); }
    template <class T> T Get() { T v{}; if (Ok(sizeof(T))) std::memcpy(&v, &d[p], sizeof(T)); p += sizeof(T); return v; }
};

std::vector<uint8_t> ReadAll(const std::string& path) {
    std::vector<uint8_t> out;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { out.resize((size_t)n); if (std::fread(out.data(), 1, (size_t)n, f) != (size_t)n) out.clear(); }
    std::fclose(f);
    return out;
}

}  // namespace

int PathNodes::AreaOf(float x, float y) {
    const int ax = (int)std::floor(x / 750.0f) + 4, ay = (int)std::floor(y / 750.0f) + 4;
    if (ax < 0 || ax > 7 || ay < 0 || ay > 7) return -1;
    return ax + ay * 8;
}

bool PathNodes::Load(const std::string& gameDir) {
    m_loaded = 0;
    m_vehNodes = 0;
    for (int a = 0; a < kAreas; a++) {
        Reader r;
        r.d = ReadAll(gameDir + "/data/Paths/NODES" + std::to_string(a) + ".DAT");
        if (r.d.size() < 20) continue;
        Area& ar = m_areas[a];
        const uint32_t numNodes = r.Get<uint32_t>(), numVeh = r.Get<uint32_t>(), numPed = r.Get<uint32_t>(), numLinks = r.Get<uint32_t>(),
                       numAddr = r.Get<uint32_t>();
        if (numNodes != numVeh + numPed || !r.Ok((size_t)numNodes * 28 + (size_t)numLinks * 14)) continue;
        ar.numVehicle = (int)numVeh;
        ar.nodes.resize(numNodes);
        for (uint32_t i = 0; i < numNodes; i++) {   // CPathNode, 28 bytes
            r.p += 8;                                // the two list pointers
            Node& n = ar.nodes[i];
            n.x = (float)r.Get<int16_t>() / 8.0f; n.y = (float)r.Get<int16_t>() / 8.0f; n.z = (float)r.Get<int16_t>() / 8.0f;
            r.Get<int16_t>();                        // m_totalDistFromOrigin
            n.baseLink = r.Get<int16_t>();
            n.area = r.Get<uint16_t>(); n.id = r.Get<uint16_t>();
            n.width = (float)r.Get<uint8_t>() / 16.0f;
            r.Get<uint8_t>();                        // flood fill
            const uint32_t f = r.Get<uint32_t>();
            n.numLinks = (int)(f & 0xF);
            n.deadEnd = (f >> 4) & 1; n.switchedOff = (f >> 5) & 1; n.roadBlock = (f >> 6) & 1; n.water = (f >> 7) & 1;
            n.dontWander = (f >> 10) & 1; n.notHighway = (f >> 12) & 1; n.highway = (f >> 13) & 1;
            n.spawnProbability = (int)((f >> 16) & 0xF); n.behaviour = (int)((f >> 20) & 0xF);
            n.vehicle = i < numVeh;
        }
        ar.links.resize(numLinks);
        for (uint32_t i = 0; i < numLinks; i++) {    // CCarPathLink, 14 bytes
            CarLink& l = ar.links[i];
            l.x = (float)r.Get<int16_t>() / 8.0f; l.y = (float)r.Get<int16_t>() / 8.0f;
            l.attachedTo.area = r.Get<uint16_t>(); l.attachedTo.node = r.Get<uint16_t>();
            l.dx = (float)r.Get<int8_t>() / 100.0f; l.dy = (float)r.Get<int8_t>() / 100.0f;
            l.width = (float)r.Get<int8_t>() / 16.0f;
            const uint8_t lanes = r.Get<uint8_t>();
            l.oppositeLanes = lanes & 7; l.sameLanes = (lanes >> 3) & 7; l.trafficLightDirection = (lanes >> 6) & 1;
            l.trafficLightState = r.Get<uint16_t>() & 3;
        }
        if (numAddr) {
            const size_t total = (size_t)numAddr + 16 * 12;   // NUM_DYNAMIC_LINKS_PER_AREA slots of spare room
            if (!r.Ok(total * 4 + (size_t)numAddr * 2 + total * 2)) continue;
            ar.nodeLinks.resize(total);
            for (size_t i = 0; i < total; i++) { ar.nodeLinks[i].area = r.Get<uint16_t>(); ar.nodeLinks[i].node = r.Get<uint16_t>(); }
            ar.naviLinks.resize(numAddr);
            for (size_t i = 0; i < numAddr; i++) ar.naviLinks[i] = r.Get<uint16_t>();
            ar.linkLengths.resize(total);
            for (size_t i = 0; i < total; i++) ar.linkLengths[i] = r.Get<uint8_t>();
        }
        m_loaded++;
        m_vehNodes += numVeh;
    }
    std::fprintf(stderr, "[PATHS] %d areas, %zu vehicle nodes\n", m_loaded, m_vehNodes);
    return m_loaded > 0;
}

const PathNodes::Node* PathNodes::Get(Address a) const {
    if (!a.Valid() || a.area >= kAreas || a.node >= m_areas[a.area].nodes.size()) return nullptr;
    return &m_areas[a.area].nodes[a.node];
}

bool PathNodes::Neighbour(Address a, int i, Address* to, float* length, const CarLink** lane) const {
    const Node* n = Get(a);
    if (!n || i < 0 || i >= n->numLinks) return false;
    const Area& ar = m_areas[a.area];
    const size_t slot = (size_t)n->baseLink + (size_t)i;
    if (slot >= ar.nodeLinks.size()) return false;
    if (to) *to = ar.nodeLinks[slot];
    if (length) *length = slot < ar.linkLengths.size() ? (float)ar.linkLengths[slot] : 0.0f;
    if (lane) {
        *lane = nullptr;
        if (slot < ar.naviLinks.size() && ar.naviLinks[slot] != 0xFFFF) {
            const int linkId = ar.naviLinks[slot] & 0x3FF, linkArea = ar.naviLinks[slot] >> 10;
            if (linkArea < kAreas && (size_t)linkId < m_areas[linkArea].links.size()) *lane = &m_areas[linkArea].links[(size_t)linkId];
        }
    }
    return true;
}

std::vector<PathNodes::Address> PathNodes::NodesNear(float x, float y, float radius, size_t maxCount) const {
    std::vector<std::pair<float, Address>> found;
    const int a0x = (int)std::floor((x - radius) / 750.0f) + 4, a1x = (int)std::floor((x + radius) / 750.0f) + 4;
    const int a0y = (int)std::floor((y - radius) / 750.0f) + 4, a1y = (int)std::floor((y + radius) / 750.0f) + 4;
    for (int ay = std::max(0, a0y); ay <= std::min(7, a1y); ay++)
        for (int ax = std::max(0, a0x); ax <= std::min(7, a1x); ax++) {
            const Area& ar = m_areas[ax + ay * 8];
            for (int i = 0; i < ar.numVehicle; i++) {
                const Node& n = ar.nodes[(size_t)i];
                const float dx = n.x - x, dy = n.y - y, d2 = dx * dx + dy * dy;
                if (d2 <= radius * radius) found.push_back({d2, Address{n.area, n.id}});
            }
        }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<Address> out;
    for (size_t i = 0; i < found.size() && i < maxCount; i++) out.push_back(found[i].second);
    return out;
}

PathNodes::Address PathNodes::Nearest(float x, float y, float z, float maxDist, bool excludeWater) const {
    Address best;
    float bd = maxDist * maxDist;
    for (const Address& a : NodesNear(x, y, maxDist, 4096)) {
        const Node* n = Get(a);
        if (!n || (excludeWater && n->water)) continue;
        const float dz = n->z - z, d2 = (n->x - x) * (n->x - x) + (n->y - y) * (n->y - y) + dz * dz;
        if (d2 < bd) { bd = d2; best = a; }
    }
    return best;
}
