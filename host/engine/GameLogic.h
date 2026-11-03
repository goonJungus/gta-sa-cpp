// CGameLogic (Wasted / Busted and the resurrection) and CRestart (hospital and police restart points).
//
// The player dies or is arrested: the big WASTED / BUSTED text, three seconds, a two second fade to black,
// the penalties ($100 at the hospital, a fine by wanted level at the police, weapons gone), twelve hours
// pass, and he is put back at the closest restart point of a city he has unlocked, faded in over three.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class GameLogic {
public:
    // map.zon: the level boxes (CTheZones::GetLevelFromPosition, 0 the countryside, 1 LS, 2 SF, 3 LV).
    void Init(const std::string& gameDir);
    int LevelAt(float x, float y, float z) const;

    // ---- CRestart -------------------------------------------------------------------------------
    // The retail main script's points, for a game that runs without the script.
    void AddRetailRestartPoints();
    void ClearRestartPoints() { m_hospitals.clear(); m_police.clear(); }
    void AddHospitalRestart(float x, float y, float z, float heading, int town);   // ADD_HOSPITAL_RESTART
    void AddPoliceRestart(float x, float y, float z, float heading, int town);     // ADD_POLICE_RESTART
    void OverrideNextRestart(float x, float y, float z, float heading);            // OVERRIDE_NEXT_RESTART
    void CancelOverrideRestart() { m_override = false; }
    // SET_EXTRA_HOSPITAL_RESTART_POINT / SET_EXTRA_POLICE_STATION_RESTART_POINT: within the radius, this point.
    void SetExtraHospitalRestart(float x, float y, float z, float radius, float heading);
    void SetExtraPoliceRestart(float x, float y, float z, float radius, float heading);
    bool FindClosestHospital(float x, float y, float z, float out[3], float& heading);
    bool FindClosestPolice(float x, float y, float z, float out[3], float& heading);
    size_t HospitalCount() const { return m_hospitals.size(); }
    size_t PoliceCount() const { return m_police.size(); }

    // ---- CGameLogic -----------------------------------------------------------------------------
    struct Hooks {
        std::function<int()> money;
        std::function<void(int)> setMoney;
        std::function<int()> wanted;
        std::function<void()> clearWeapons;                          // CPed::ClearWeapons
        std::function<void(int)> passTime;                           // game minutes
        // RestorePlayerStuffDuringResurrection at this point (z is the ground, heading in degrees)
        std::function<void(float, float, float, float)> resurrect;
        std::function<void()> holdBlack;                             // SET_FADE_IN_AFTER_NEXT_*: the script fades in
        std::function<void(const char*)> log;
        // hunger
        std::function<void(float)> loseHealth;                       // m_fHealth -= n, past the armour
        std::function<void(const std::string&)> helpMessage;         // CHud::SetHelpMessage (GXT key)
        std::function<void(int, float)> statDown;                    // CStats::DecrementStat + the stat box
    };
    Hooks hooks;

    // Once a frame: dead / arrested this frame, where the player is.
    void Update(float dt, bool dead, bool arrested, float px, float py, float pz);

    bool Playing() const { return m_state == State::Playing; }       // IS_PLAYER_PLAYING
    bool Wasted() const { return m_state == State::Wasted; }
    bool Busted() const { return m_state == State::Busted; }
    float Fade() const { return m_fade; }                            // 0 clear .. 1 black
    // CHud::DrawBustedWastedMessage: the text (already looked up) and its alpha 0..1.
    const std::string& BigMessage() const { return m_message; }
    float BigMessageAlpha() const { return m_msgAlpha / 255.0f; }
    void SetMessageText(const std::string& wasted, const std::string& busted) { m_wastedText = wasted; m_bustedText = busted; }

    // Script switches
    bool penaltyForDeath = true, penaltyForArrest = true;            // SWITCH_DEATH_PENALTIES / SWITCH_ARREST_PENALTIES
    bool freeHealthCare = false, getOutOfJailFree = false;           // SET_FREE_HEALTH_CARE / SET_GET_OUT_OF_JAIL_FREE
    bool fadeInAfterNextDeath = true, fadeInAfterNextArrest = true;  // SET_FADE_IN_AFTER_NEXT_DEATH / _ARREST
    void ForceDeathRestart();                                        // FORCE_DEATH_RESTART

    // CPlayerInfo::WorkOutEnergyFromHunger, once a frame with the game clock hour: past 48 hours without
    // food the stomach rumbles (help NOTEAT once), then every hour 25 fat goes, then 25 muscle, then 2 health.
    void UpdateHunger(int hour, bool neverHungryCheat);
    void Ate() { m_hoursDidntEat = 0; }                              // food filled him up / a save / a resurrection
    int HoursDidntEat() const { return m_hoursDidntEat; }
    void SetHoursDidntEat(int h) { m_hoursDidntEat = h; }

private:
    struct Point { float x, y, z, heading; int town; };
    struct Zone { float x0, y0, z0, x1, y1, z1; int level; };
    enum class State { Playing, Wasted, Busted };

    bool FindClosest(const std::vector<Point>& pts, bool extra, const float extraPos[3], float extraRadius, float extraHeading,
                     float x, float y, float z, float out[3], float& heading);
    void Restart(bool hospital, float px, float py, float pz);
    void Log(const char* fmt, ...);

    std::vector<Zone> m_zones;
    std::vector<Point> m_hospitals, m_police;
    bool m_override = false;
    float m_overridePos[3] = {}, m_overrideHeading = 0.0f;
    float m_extraHosp[3] = {}, m_extraHospRadius = 0.0f, m_extraHospHeading = 0.0f;
    float m_extraPolice[3] = {}, m_extraPoliceRadius = 0.0f, m_extraPoliceHeading = 0.0f;

    State m_state = State::Playing;
    float m_since = 0.0f;          // seconds since the death or the arrest (TimeOfLastEvent)
    float m_fade = 0.0f;
    float m_fadeDir = 0.0f;        // +1 to black, -1 back in, 0 still
    float m_fadeRate = 0.0f;       // per second
    std::string m_message, m_wastedText = "WASTED", m_bustedText = "BUSTED";
    float m_msgTimer = 0.0f, m_msgAlpha = 0.0f;
    int m_hoursDidntEat = 0;       // CPlayerInfo::m_nNumHoursDidntEat
    int m_hungerHour = -1;         // s_lastTimeHungryStateProcessed
    int m_lastHungryState = 0;     // s_LastHungryState: muscle goes once this many hours have passed
    bool m_hungryShown = false;    // s_bHungryMessageShown
};
