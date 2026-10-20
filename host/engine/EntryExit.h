#pragma once
// CEntryExit / CEntryExitManager: the doors between the outside world (area 0) and the interiors.
// Every "enex" line of the map IPLs is one door side; an outdoor door with the "create linked
// pair" flag (4) pairs with the same named door of the interior. Walking into a door's yellow
// marker fades out, moves the player to the linked door's exit point and switches the area.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class D3DRenderer;

struct EntryExitDoor {
    char name[8] = {};
    float cx = 0, cy = 0, cz = 0;        // entrance centre (cz already +1, like CEntryExit::m_fEntranceZ)
    float halfX = 0, halfY = 0;          // entrance rect half extents
    float angle = 0;                     // radians
    float ex = 0, ey = 0, ez = 0;        // exit position (ez already +1)
    float exitAngle = 0;                 // degrees
    int area = 0;                        // area the exit is in
    int flags = 0;
    int sky = 0, peds = 0, timeOn = 0, timeOff = 24;
    int link = -1;
    bool enabled = true;
    bool IsIn(float x, float y, float z) const;
};

class EntryExit {
public:
    // Parse the enex sections of the map IPLs listed in gta.dat.
    int Load(const std::string& gameDir);
    size_t Count() const { return m_d.size(); }
    const EntryExitDoor& At(size_t i) const { return m_d[i]; }

    // Per frame. Returns true while a transition owns the player (input off).
    // hour is the clock for the open/close times; inCar from the player.
    bool Update(float dt, float px, float py, float pz, bool inCar, int hour);
    // Set by the owner: the teleport at the fade peak (x, y, z, headingDegrees, area).
    std::function<void(float, float, float, float, int)> warp;
    std::function<void(int)> setArea;

    int  Index(const char* name, int notFlags = 0) const;       // GetEntryExitIndex
    void SetEnabledByName(const char* name, bool on);            // SWITCH_ENTRY_EXIT
    void SetAllDisabled(bool off) { m_disabled = off; }
    void SetBurglaryHouses(bool on);
    // The last door the player went through (GET_NAME_OF_ENTRY_EXIT_CHAR_USED).
    const char* LastName() const { return m_lastName; }
    int  LastIndex() const { return m_last; }
    int  CurrentArea() const { return m_curArea; }
    void ForceArea(int a) { m_curArea = a; if (setArea) setArea(a); }
    float Fade() const { return m_fade; }
    bool Busy() const { return m_state != 0; }

    void DrawMarkers(D3DRenderer& r, const float cam[3], uint32_t nowMs, int hour);

    int  Trigger(int idx, bool inCar);       // start the transition through door idx (dev/test, script)

private:
    std::vector<EntryExitDoor> m_d;
    int m_state = 0;          // 0 idle, 1 fading out, 2 fading in
    float m_t = 0, m_fade = 0;
    int m_active = -1;        // the door being used
    bool m_armed = true;      // false until the player leaves every entrance after a transition
    bool m_disabled = false;
    int m_curArea = 0;
    int m_last = -1;
    char m_lastName[8] = {};
    static bool InTime(const EntryExitDoor& d, int hour);
};
