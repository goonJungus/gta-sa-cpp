#pragma once
// CPathFind: the road network of San Andreas (data/Paths/NODES0..63.DAT, an 8 x 8 grid of 750 m areas). Each area
// has its path nodes (vehicle ones first, then ped ones), the car path links ("navi nodes": lane geometry along
// one connection), the node links (up to 12 neighbours per node, in order from the node's base link id), the link
// lengths and the intersection flags. Read as CPathFind::LoadPathFindData does.

#include <cstdint>
#include <string>
#include <vector>

class PathNodes {
public:
    static constexpr int kAreas = 64;

    struct Address {
        uint16_t area = 0xFFFF, node = 0xFFFF;
        bool Valid() const { return area != 0xFFFF && node != 0xFFFF; }
        bool operator==(const Address& o) const { return area == o.area && node == o.node; }
        bool operator!=(const Address& o) const { return !(*this == o); }
    };

    struct Node {
        float x = 0, y = 0, z = 0;           // metres (the file keeps them as int16 / 8)
        float width = 0;                     // m_nPathWidth / 16
        int baseLink = 0;                    // its first entry in the area's node links
        int numLinks = 0;
        uint16_t area = 0, id = 0;
        bool switchedOff = false, water = false, deadEnd = false, roadBlock = false;
        bool dontWander = false, highway = false, notHighway = false;
        int spawnProbability = 0;            // 0..15
        int behaviour = 0;                   // 1 roadblock
        bool vehicle = false;                // false: a ped node
    };

    struct CarLink {   // CCarPathLink: one lane strip from a node towards another
        float x = 0, y = 0;
        Address attachedTo;
        float dx = 0, dy = 0;                // unit direction to the target
        float width = 0;
        int oppositeLanes = 0, sameLanes = 0;
        bool trafficLightDirection = false;
        int trafficLightState = 0;
    };

    bool Load(const std::string& gameDir);
    bool Ready() const { return m_loaded > 0; }
    int AreasLoaded() const { return m_loaded; }
    size_t VehicleNodeCount() const { return m_vehNodes; }

    const Node* Get(Address a) const;
    // The i-th neighbour of a node: its address, the length (metres) of the link, and the lane geometry.
    bool Neighbour(Address a, int i, Address* to, float* length, const CarLink** lane) const;
    // The area's index for a world position (x, y), -1 off the map.
    static int AreaOf(float x, float y);
    // The vehicle nodes within `radius` of (x, y), nearest first (a coarse scan of the nearby areas).
    std::vector<Address> NodesNear(float x, float y, float radius, size_t maxCount = 256) const;
    // The nearest vehicle node to (x, y, z) within maxDist, else an invalid address.
    Address Nearest(float x, float y, float z, float maxDist, bool excludeWater = true) const;

private:
    struct Area {
        std::vector<Node> nodes;
        std::vector<CarLink> links;
        std::vector<Address> nodeLinks;      // CNodeAddress per link slot
        std::vector<uint16_t> naviLinks;     // CCarPathLinkAddress per link slot (10 bit link id, 6 bit area)
        std::vector<uint8_t> linkLengths;
        int numVehicle = 0;
    };
    Area m_areas[kAreas];
    int m_loaded = 0;
    size_t m_vehNodes = 0;
};
