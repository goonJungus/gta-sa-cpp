// The vending machines, a port of vending_machine.scm (script.img), the object script the main script gives
// the machine models (ALLOCATE_STREAMED_SCRIPT_TO_OBJECT, activation radius 6 m). Standing in front of one shows
// SLOT_04 (snacks) or SLOT_05 (drinks) and takes the enter-car button; pressing it with money pays $1 and runs
// CJ's sequence (PlayerPed::StartVending); when it ends the food counts (STAT_CALORIES 5) and health rises 30.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class GameWorld;
class PlayerPed;

class Vending {
public:
    struct Hooks {
        std::function<int()> money;
        std::function<void(int)> addMoney;
        std::function<void(const std::string&)> help;   // PRINT_HELP_FOREVER (GXT key); empty: CLEAR_HELP
        std::function<void(int, int)> sound;            // bank, sound
        std::function<void(float)> addHealth;           // hit points
    };
    Hooks hooks;

    void Init(const GameWorld& w);
    size_t Count() const { return m_machines.size(); }
    // Once a frame. px, py, pz: the player's centre (a metre above the feet); canUse: on foot and playing
    // (CAN_PLAYER_START_MISSION); usePressed: the enter-car button is down.
    void Update(const GameWorld& w, PlayerPed& cj, float px, float py, float pz, int area, bool canUse,
                bool usePressed, uint32_t nowMs);
    // SET_PLAYER_ENTER_CAR_BUTTON off: the button belongs to the machine while CJ stands at it.
    bool ClaimsEnterButton() const { return m_state >= 2 && m_state <= 3; }

private:
    struct Machine { size_t inst; bool snack; };
    void Reset();

    std::vector<Machine> m_machines;
    int m_active = -1;        // index into m_machines (l0)
    int m_state = 0;          // the script's l1: 0 request the anims, 1 wait for CJ, 2 at the machine, 3 using, 4 cool down
    uint32_t m_until = 0;     // l2
};
