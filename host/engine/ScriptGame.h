#pragma once
// ScriptGame: the game commands of the mission script (CRunningScript::ProcessCommands*): screen
// fade, widescreen, the text and print commands, the script camera, peds and cars made by the
// script. The machine (Script.h) runs the language and calls Handle() for every game command.

#include <cstdint>
#include <functional>
#include <map>
#include <array>
#include <string>
#include <vector>

#include <memory>

#include "Cutscene.h"
#include "ImgLoader.h"
#include "Vehicle.h"
#include "Hud.h"
#include "Script.h"

class D3DRenderer;
class Peds;
class Pickups;
class EntryExit;
class Vehicles;

// The camera the script placed (CCamera in fixed mode). main applies it over the player camera.
struct ScriptCamera {
    bool active = false;               // a fixed position is set
    float pos[3] = {};
    float look[3] = {};
    bool moveOn = false, trackOn = false;
    float moveFrom[3] = {}, moveTo[3] = {}, trackFrom[3] = {}, trackTo[3] = {};
    float moveT = 0, moveDur = 0, trackT = 0, trackDur = 0;   // seconds
    bool persistPos = false, persistTrack = false, persistFov = false;
    float nearClip = 0.0f;             // 0 = the game default
    float shakeT = 0, shakeDur = 0, shakeAmp = 0;
    bool cutscene = false;             // the cutscene player owns the camera
    float up[3] = {0, 0, 1};           // cutscene roll
    float fov = 0.0f;                  // cutscene FOV in degrees, 0 = the game default
};

class ScriptGame {
public:
    ScmMachine* scm = nullptr;
    Peds* peds = nullptr;
    Vehicles* cars = nullptr;
    D3DRenderer* r = nullptr;
    Hud* hud = nullptr;
    Pickups* pickups = nullptr;
    EntryExit* enex = nullptr;
    std::function<int()> getArea;                                // CGame::currArea
    std::function<void(int)> setArea;
    std::function<uint32_t()> nowMs;                              // the pickup clock
    // main supplies these
    std::function<bool()> playerInCar;
    std::function<int()> playerCar;                              // Vehicles index or -1
    std::function<void(int)> putPlayerInCar;
    std::function<void()> playerLeaveCar;
    std::function<float(float, float)> groundAt;                 // world height or < -1e5
    std::function<int()> wantedLevel;
    std::function<void(int)> setWantedLevel;
    std::function<void(float)> setPlayerHeading;
    std::function<float()> getPlayerHeading;
    std::function<void(int, int)> giveWeapon;                    // weapon type, ammo
    std::function<void(int)> setWeapon;                          // weapon type in hand
    std::string gameDir;
    Cutscene::DrawCj drawCj;                                     // CJ in a cutscene
    std::function<void(bool)> showPlayer;                        // hide CJ while a cutscene plays
    std::function<void(const std::string&)> cutsceneAudio;       // START_CUTSCENE: play the scene's audio track (CAudioEngine::PlayPreloadedCutsceneTrack)
    std::function<void()> cutsceneAudioStop;                     // the scene ended, was skipped or cleared
    std::function<int()> menuInput;                              // edges this frame: 1 up, 2 down, 4 accept
    std::function<int(int, int)> playSound;                      // bank, sound -> voice (0 = nothing played)
    std::function<bool(int)> voicePlaying;
    std::function<void(int)> stopSound;
    std::function<bool()> skipPressed;                           // CCutsceneMgr::IsCutsceneSkipButtonBeingPressed (edge)

    ScriptCamera cam;
    float fade = 0.0f;                 // 0 clear .. 1 solid (script fade layer)
    bool playerControl = true;
    struct PlayerGoal { bool on = false; float x = 0, y = 0; int mode = 3; } playerGoal;   // TASK_GO_STRAIGHT_TO_COORD on the player; main walks CJ there
    bool hudOn = true, radarOn = true; // DISPLAY_HUD / DISPLAY_RADAR
    float wide = 0.0f;                 // 0 .. 1, eases to the target
    bool wideTarget = false;

    bool Handle(ScmMachine::Call& c);
    bool HandleMore(ScmMachine::Call& c);       // language extensions, queries, objects, areas
    void Update(float dt);
    // CVehicleRecording: the recorded car paths of carrec.img.
    bool RecordingLoaded(int number);
    bool IsPlaybackGoingOn(int car) const;
    int PlaybackSlot(int car) const;
    void StartPlayback(int car, int number, bool looped, bool useAi);
    void StopPlayback(int car, bool skipToEnd);
    void PlaybackStep(float dt);
    // The overlay for this frame; call after Update. Clears the per frame text queue.
    ScriptOverlay Overlay();

    // Dev log of everything the script did, for the headless tests.
    int cutsceneState() const { return m_cut.state; }
    // The cutscene objects, drawn with the world.
    void DrawCutscene(D3DRenderer& r);
    bool CutsceneActive() const { return m_cut.state == 2; }
    float CutsceneMs() const { return m_cutscene.TimeMs(); }
    const Cutscene& CutscenePlayer() const { return m_cutscene; }

private:
    struct Frame { uint32_t time; int16_t vel[3]; int8_t right[3], top[3]; int8_t steer, gas, brake, handbrake; float pos[3]; };
    struct Playback { bool on = false, looped = false, paused = false, ai = false; int car = 0, number = 0; size_t index = 0; float time = 0, speed = 1.0f; };
    ImgLoader m_carrecImg;
    bool m_carrecOpen = false;
    std::map<int, std::vector<Frame>> m_recs;                    // loaded recordings by number
    Playback m_play[16];
    struct MissionAudio { int bank = -1, sound = -1, voice = 0, event = 0; } m_mission[4];   // CAEScriptAudioEntity wavLinks
    void RestoreFrame(VehicleInstance& v, const Frame& f, bool pause);
    struct CutState { std::string name; int state = 0; float t = 0; float dur = 0; bool skipped = false; };   // 0 none, 1 loaded, 2 running, 3 finished
    Cutscene m_cutscene;
    CutState m_cut;
    int m_cutObjects = 0, m_cutSec = -1;
    std::string m_cutKey;
    float m_fadeFrom = 0, m_fadeTo = 0, m_fadeT = 0, m_fadeDur = 0;
    bool m_fading = false;
    float m_fadeRgb[3] = {0, 0, 0};
    ScriptTextItem m_style;             // SET_TEXT_* waiting for the next DISPLAY_TEXT
    std::vector<ScriptTextItem> m_queue;
    std::vector<ScriptSprite> m_spriteQueue;                     // DRAW_RECT / DRAW_SPRITE this frame
    std::map<int, std::string> m_spriteTex;                      // LOAD_SPRITE slot -> texture name
    bool m_spritesBefore = false;                                // SET_SPRITES_DRAW_BEFORE_FADE
    std::string m_subtitle;
    float m_subTimer = 0;
    std::vector<std::string> m_special;                          // LOAD_SPECIAL_CHARACTER slots
    std::map<int, int> m_charCar;                                // char uid -> car index it sits in
    std::map<int, int> m_blipCar;
    int m_playerBlipCar = -1;
    // a ped waiting for TASK_CAR_DRIVE_TO_COORD
    struct Obj { int model = 0; float x = 0, y = 0, z = 0, heading = 0; bool removed = false; };
    std::vector<Obj> m_objs;
    bool EntityPos(int kind, int h, float& x, float& y, float& z, bool& inCar, float* speed = nullptr);   // kind 0 char, 1 car, 2 object
    bool Locate(ScmMachine::Call& c, const std::string& name);
    bool InArea(ScmMachine::Call& c, const std::string& name);
    // CREATE_PICKUP and friends. A pickup of a model the pickup drawer knows lives in a Pickups slot;
    // the rest (property signs, collectibles) are tracked and collected by walking into them.
    struct ScriptPickup { int slot = -1; float x = 0, y = 0, z = 0; bool removed = false, collected = false, locked = false; };
    std::vector<ScriptPickup> m_pickups;
    int m_markers = 0;
    // CMenuSystem standard menus (CREATE_MENU and friends): two slots, up to 4 columns of 12 rows.
    struct Menu {
        bool used = false, interactive = false, background = false;
        std::string title;
        int columns = 0, rows = 0, active = 0, accepted = 0x9d;
        float x = 0, y = 0, width[4] = {};
        int align[4] = {};
        std::string head[4];
        std::string cell[4][12];
        int n1[4][12], n2[4][12];
        bool on[12];
    };
    Menu m_menus[2];
    int m_curMenu = 0;
    void MenuRows(Menu& m);
    void MenuFix(Menu& m);                                                              // CMenuSystem::ActivateItems: the active row lands on a live row
    void MenuUpdate();
    void MenuDraw(const Menu& m, bool current);
    int AddPickup(int model, int type, int ammo, float x, float y, float z);
    struct Drive { int car; float x, y, speed; };
    std::map<int, Drive> m_drives;
    std::map<int, std::array<float, 3>> m_frozenCars;                                 // FREEZE_CAR_POSITION

    ScriptTextItem DefaultStyle() const;
    int CharIndex(int uid) const;
    bool CharPos(int uid, float& x, float& y, float& z) const;
    std::string Text(const std::string& key);
    void AddText(float x, float y, const std::string& text);
};
