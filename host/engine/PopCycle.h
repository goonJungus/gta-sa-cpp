#pragma once
// The info zones (CTheZones) and CPopCycle: which kind of area a position is in (a zone population type, "residential
// average" unless the script says otherwise), how many cars the time of day allows there (popcycle.dat) and which cars
// the people there drive (cargrp.dat, by the group percentages of that zone type, hour and weekday / weekend).

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class PopCycle {
public:
    static constexpr int kTypes = 20, kGroups = 18;
    enum { kResidentialAverage = 5 };

    // modelOfDff: a vehicles.ide model name (as cargrp.dat spells it) to its id, or -1.
    bool Load(const std::string& gameDir, const std::function<int(const std::string&)>& modelOfDff);
    bool Ready() const { return m_ready; }

    // CTheZones::GetZoneInfo: the smallest info zone round (x, y, z), its population type (the script's, else the default).
    int TypeAt(float x, float y, float z) const;
    // SET_ZONE_POPULATION_TYPE (0x0767)
    void SetZonePopulationType(const std::string& label, int type);
    // The popcycle.dat "#Cars" for the type at this hour (12 two-hour slots), weekday or weekend.
    int MaxCars(int type, float hour, bool weekend) const;
    // A car for the people of that zone type and time: a group by the percentages, then one of its models that `ok`
    // allows. r1 and r2 are 0..1 random numbers. -1 when nothing fits.
    int PickCar(int type, float hour, bool weekend, float r1, float r2, const std::function<bool(int)>& ok) const;

private:
    struct Zone { std::string label; float x1, y1, z1, x2, y2, z2; };
    struct Row { int cars = 0;  float group[kGroups] = {}; };
    std::vector<Zone> m_zones;
    std::unordered_map<std::string, int> m_typeByLabel;
    Row m_rows[kTypes][2][12];
    std::vector<int> m_groupCars[kGroups];
    bool m_ready = false;
};
