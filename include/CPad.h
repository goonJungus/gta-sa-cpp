// CPad - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Pad.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations:
// - stripped InjectHooks()
// - VALIDATE_SIZE(CPad, 0x134) -> guarded static_assert (32-bit only); same treatment
//   for the input-state structs (CControllerState 0x30, CKeyboardState 0x270,
//   CMouseControllerState 0x14)
// - StaticRef<T>(addr) statics -> static member declarations; definitions live in
//   src/CPad.cpp
// - CControllerState / CKeyboardState / CMouseControllerState are defined here in full
//   (the inline accessors below need complete types). TODO: split each into its own
//   header with the input subsystem.
// - RsKeyCodes (RenderWare rw/skeleton.h, rsInputDevice): aliased to int32.
//   TODO: port the real RenderWare input types with the RenderWare layer.
// - DirectInput-only surface dropped: GetMouseState(DIMOUSESTATE2*), DIReleaseMouse(),
//   InitialiseMouse(). TODO: re-add with the input backend (SDL3 path or Win32 backend).

#pragma once

#include "GrTypes.h" // int8..uint32
#include "CVector.h"  // CVector, CVector2D

#include <array>
#include <cstdint>

class CPed;

// ---- input state structs ----
// TODO: each of these belongs in its own header (input subsystem).

// Source: gta-reversed/source/game_sa/ControllerState.h
// Set values to 128 unless otherwise specified
class CControllerState {
public:
    int16 LeftStickX; // move/steer left (-128?)/right (+128)
    int16 LeftStickY; // move back(+128)/forwards(-128?)
    int16 RightStickX; // numpad 6(+128)/numpad 4(-128?)
    int16 RightStickY;

    int16 LeftShoulder1;
    int16 LeftShoulder2;
    int16 RightShoulder1; // target / hand brake
    int16 RightShoulder2;

    int16 DPadUp; // radio change up           Next radio station / Call gang forward/Recruit gang member
    int16 DPadDown; // radio change down       Previous radio station / Gang stay back/Release gang (hold)
    int16 DPadLeft; //                         Skip trip/Action / Negative talk reply
    int16 DPadRight; //                        Next user MP3 track / Positive talk reply

    int16 Start;                             //Pause
    int16 Select;                            //Camera modes

    int16 ButtonSquare; // jump / reverse      Break/Reverse / Jump/Climb
    int16 ButtonTriangle; // get in/out        Exit vehicle / Enter veihcle
    int16 ButtonCross; // sprint / accelerate  Accelerate / Sprint/Swim
    int16 ButtonCircle; // fire                Fire weapon

    int16 ShockButtonL;
    int16 ShockButtonR; // look behind

    int16 m_bChatIndicated;
    int16 m_bPedWalk;
    int16 m_bVehicleMouseLook;
    int16 m_bRadioTrackSkip;

public:
    void Clear();
    bool CheckForInput();
};

// Source: gta-reversed/source/game_sa/KeyboardState.h
class CKeyboardState {
public:
    std::array<int16, 12>  FKeys{};        //!< F1 - F12 (0-11)
    std::array<int16, 256> standardKeys{}; //!< All other keys with keycode < 0xFF
    int16                  esc{};          //!< Escape
    int16                  insert{};       //!< Insert
    int16                  del{};          //!< Delete
    int16                  home{};         //!< Home
    int16                  end{};          //!< End
    int16                  pgup{};         //!< Page up
    int16                  pgdn{};         //!< Page down
    int16                  up{};           //!< Arrow up
    int16                  down{};         //!< Arrow down
    int16                  left{};         //!< Arrow left
    int16                  right{};        //!< Arrow right
    int16                  scroll{};       //!< Scroll lock (?)
    int16                  pause{};        //!< Pause
    int16                  numlock{};      //!< Numlock
    int16                  div{};          //!< `/`
    int16                  mul{};          //!< `*`
    int16                  sub{};          //!< `-`
    int16                  add{};          //!<  `+`
    int16                  enter{};        //!< Enter (Regular)
    int16                  decimal{};      //!<
    int16                  num1{};         //!< Numpad 1
    int16                  num2{};         //!< Numpad 2
    int16                  num3{};         //!< Numpad 3
    int16                  num4{};         //!< Numpad 4
    int16                  num5{};         //!< Numpad 5
    int16                  num6{};         //!< Numpad 6
    int16                  num7{};         //!< Numpad 7
    int16                  num8{};         //!< Numpad 8
    int16                  num9{};         //!< Numpad 9
    int16                  num0{};         //!< Numpad 0
    int16                  back{};         //!< Return?
    int16                  tab{};          //!< Tab
    int16                  capslock{};     //!< Capslock
    int16                  extenter{};     //!< Enter (Numpad?)
    int16                  lshift{};       //!< Left shift
    int16                  rshift{};       //!< Right shift
    int16                  shift{};        //!< Shift keys combined (lshift | rshift)
    int16                  lctrl{};        //!< Left ctrl
    int16                  rctrl{};        //!< Right ctrl
    int16                  lalt{};         //!< L Alt (AKA lmenu)
    int16                  ralt{};         //!< R Alt (AKA rmenu)
    int16                  lwin{};         //!< Left windows key
    int16                  rwin{};         //!< Right windows key
    int16                  apps{};         //!< Apps

public:
    void Clear();
};

// Source: gta-reversed/source/game_sa/MouseControllerState.h
class CMouseControllerState {
public:
    bool      isMouseLeftButtonPressed{};   // LMB
    bool      isMouseRightButtonPressed{};  // RMB
    bool      isMouseMiddleButtonPressed{}; // MMB
    bool      isMouseWheelMovedUp{};        // Wheel up
    bool      isMouseWheelMovedDown{};      // Wheel down
    bool      isMouseFirstXPressed{};       // BMX1
    bool      isMouseSecondXPressed{};      // BMX2
    float     m_fWheelMoved{};              // Wheel movement
    CVector2D m_AmountMoved{};              // Mouse movement

public:
    CMouseControllerState();
    CMouseControllerState* Constructor();

    void Clear();
    [[nodiscard]] bool CheckForInput() const;
    [[nodiscard]] auto GetAmountMouseMoved() const { return m_AmountMoved; }
};

// TODO: port RenderWare rw/skeleton.h (rsInputDevice) with the RenderWare layer.
using RsKeyCodes = int32;

// Taken from GTA3 Script Compiler (miss2.exe)
enum ePadID {
    PAD1 = 0,
    PAD2 = 1,

    MAX_PADS
};

enum eFKeyID : uint8 {
    FKEY1,
    FKEY2,
    FKEY3,
    FKEY4,
    FKEY5,
    FKEY6,
    FKEY7,
    FKEY8,
    FKEY9,
    FKEY10,
    FKEY11,
    FKEY12,
};

#define KEY_IS_DOWN(btn)       (NewKeyState.btn)
#define KEY_IS_PRESSED(btn)    (NewKeyState.btn && !OldKeyState.btn)

#define BUTTON_IS_PRESSED(btn) (NewState.btn && !OldState.btn)
#define BUTTON_IS_DOWN(btn)    (NewState.btn)
#define BUTTON_JUST_UP(btn)    (!NewState.btn && OldState.btn)

#define MOUSE_IS_PRESSED(btn)  (NewMouseControllerState.btn && !OldMouseControllerState.btn)
#define MOUSE_IS_DOWN(btn)     (NewMouseControllerState.btn)

#ifdef NOTSA_USE_SDL3
union SDL_Event;
#endif

class CPad {
public:
    CControllerState NewState;
    CControllerState OldState;
    std::array<int16, 10> SteeringLeftRightBuffer;
    int32            DrunkDrivingBufferUsed;
    CControllerState PCTempKeyState;
    CControllerState PCTempJoyState;
    CControllerState PCTempMouseState;
    char             Phase;
    int16            Mode;
    int16            ShakeDur;

    union {
        struct {
            uint16 bCamera : 1;
            uint16 unk2 : 1;
            uint16 bPlayerAwaitsInGarage : 1;
            uint16 bPlayerOnInteriorTransition : 1;
            uint16 unk3 : 1; // 0x10 unused
            uint16 bPlayerSafe : 1;
            uint16 bPlayerTalksOnPhone : 1; // bPlayerSafeForPhoneCall?
            uint16 bPlayerSafeForCutscene : 1;

            uint16 bPlayerSkipsToDestination : 1; // bPlayerSafeForDestination?
        };
        uint16 DisablePlayerControls;
    };

    char     ShakeFreq;
    std::array<char, 5> bHornHistory;
    char     iCurrHornHistory;
    char     JustOutOfFrontEnd;
    bool     bApplyBrakes;
    bool     bDisablePlayerEnterCar;
    bool     bDisablePlayerDuck;
    bool     bDisablePlayerFireWeapon;
    bool     bDisablePlayerFireWeaponWithL1;
    bool     bDisablePlayerCycleWeapon;
    bool     bDisablePlayerJump;
    bool     bDisablePlayerDisplayVitalStats;
    uint32   LastTimeTouched;
    int32    AverageWeapon;
    int32    AverageEntries;
    float    NoShakeBeforeThis;
    char     NoShakeFreq;
    char    _pad131[3];

public:
    static CMouseControllerState TempMouseControllerState;
    static CMouseControllerState NewMouseControllerState;
    static CMouseControllerState OldMouseControllerState;

    static CKeyboardState TempKeyState;
    static CKeyboardState OldKeyState;
    static CKeyboardState NewKeyState;

    static CPad Pads[MAX_PADS];

    static bool bInvertLook4Pad;
    static char padNumber;

public:
    CPad();
    ~CPad() = default; // 0x53ED60

    static void Initialise();
    static void ClearKeyBoardHistory();
    static void ClearMouseHistory();

    /* SDL Support, see `Pad_SDL.cpp` */
#ifdef NOTSA_USE_SDL3
    bool ProcessMouseEvent(const SDL_Event& e, CMouseControllerState& ms);
    bool ProcessKeyboardEvent(const SDL_Event& e, CKeyboardState& ks);
    bool ProcessGamepadEvent(const SDL_Event & e, CControllerState& cs);
    bool ProcessJoyStickEvent(const SDL_Event& e, CControllerState& cs);

    static bool ProcessEvent(const SDL_Event& e, bool ignoreMouseEvents, bool ignoreKeyboardEvents);
#endif

    void Clear(bool enablePlayerControls, bool resetPhase);

    void Update(int32 pad);
    static void UpdatePads();
    void UpdateMouse();
    static void ProcessPad(ePadID padID);
    void ProcessPCSpecificStuff();
    CControllerState& ReconcileTwoControllersInput(CControllerState& out, const CControllerState& controllerA, const CControllerState& controllerB);

    void SetTouched();
    [[nodiscard]] uint32 GetTouchedTimeDelta() const;

    void SetDrunkInputDelay(int32 delay) { DrunkDrivingBufferUsed = delay; } // 0x53F910

    void StartShake(int16 time, uint8 frequency, uint32 arg2);
    void StartShake_Distance(int16 time, uint8 frequency, CVector pos);
    void StartShake_Train(const CVector2D& point);
    static void StopPadsShaking();
    void StopShaking(int16 pad);

    [[nodiscard]] int16 GetCarGunLeftRight() const;
    [[nodiscard]] int16 GetCarGunUpDown() const;
    [[nodiscard]] int16 GetCarGunFired() const;
    [[nodiscard]] int16 CarGunJustDown() const;

    int16 GetSteeringLeftRight();
    [[nodiscard]] int16 GetSteeringUpDown() const;
    //int32 GetSteeringMode(); // Android

    int16 GetPedWalkLeftRight(CPed* ped) const;
    [[nodiscard]] int16 GetPedWalkLeftRight() const;
    int16 GetPedWalkUpDown(CPed* ped) const;
    [[nodiscard]] int16 GetPedWalkUpDown() const;

    [[nodiscard]] bool GetLookLeft() const;
    [[nodiscard]] bool GetLookRight() const;

    [[nodiscard]] bool GetHorn() const;
    [[nodiscard]] bool HornJustDown() const;

    [[nodiscard]] int16 GetBrake() const;
    [[nodiscard]] int16 GetHandBrake() const;

    [[nodiscard]] bool GetExitVehicle() const;
    [[nodiscard]] bool ExitVehicleJustDown() const;

    [[nodiscard]] uint8 GetMeleeAttack(bool bCheckButtonCircleStateOnly) const;
    [[nodiscard]] uint8 MeleeAttackJustDown(bool bCheckButtonCircleStateOnly) const;
    [[nodiscard]] int16 GetAccelerate() const;
    [[nodiscard]] bool GetAccelerateJustDown() const;
    [[nodiscard]] bool CycleWeaponLeftJustDown() const;
    [[nodiscard]] bool CycleWeaponRightJustDown() const;
    [[nodiscard]] bool GetTarget() const;
    [[nodiscard]] bool GetSprint() const;
    [[nodiscard]] bool SprintJustDown() const;
    int16 GetDisplayVitalStats(CPed* ped) const;
    [[nodiscard]] bool CollectPickupJustDown() const;
    [[nodiscard]] bool GetForceCameraBehindPlayer() const;
    [[nodiscard]] bool SniperZoomIn() const;
    [[nodiscard]] bool SniperZoomOut() const;

    bool WeaponJustDown(CPed* ped) const;
    [[nodiscard]] bool GetEnterTargeting() const;
    int32 GetWeapon(CPed* ped) const;
    int16 AimWeaponLeftRight(CPed* ped) const;
    int16 AimWeaponUpDown(CPed* ped) const;

    static CPad* GetPad(int32 nPadNumber = 0)                               { return &Pads[nPadNumber]; }                                                                    // 0x53FB70

    [[nodiscard]] bool NextStationJustUp() const noexcept                   { return !DisablePlayerControls && IsDPadUpJustUp(); }                                           // 0x5405B0
    [[nodiscard]] bool LastStationJustUp() const noexcept                   { return !DisablePlayerControls && IsDPadDownJustUp(); }                                         // 0x5405E0
    [[nodiscard]] bool GetLookBehindForCar() const noexcept;
    [[nodiscard]] bool GetLookBehindForPed() const noexcept                 { return !DisablePlayerControls && BUTTON_IS_DOWN(ShockButtonR); }                               // 0x53FEC0
    [[nodiscard]] bool GetHydraulicJump() const noexcept                    { return !DisablePlayerControls && BUTTON_IS_DOWN(ShockButtonR); }                               // 0x53FF70
    [[nodiscard]] bool GetDuck() const noexcept                             { return !DisablePlayerControls && !bDisablePlayerDuck && BUTTON_IS_DOWN(ShockButtonL); }        // 0x540700
    [[nodiscard]] bool DuckJustDown() const noexcept                        { return !DisablePlayerControls && !bDisablePlayerDuck && IsLeftShockPressed(); }                // 0x540720
    [[nodiscard]] bool GetJump() const noexcept                             { return !DisablePlayerControls && !bDisablePlayerDuck && BUTTON_IS_DOWN(ButtonSquare); }        // 0x540750
    [[nodiscard]] bool JumpJustDown() const noexcept                        { return !DisablePlayerControls && !bDisablePlayerDuck && IsSquarePressed(); }                   // 0x540770
    [[nodiscard]] bool ShiftTargetLeftJustDown() const noexcept             { return !DisablePlayerControls && IsLeftShoulder2Pressed(); }                                   // 0x540850
    [[nodiscard]] bool ShiftTargetRightJustDown() const noexcept            { return !DisablePlayerControls && IsRightShoulder2Pressed(); }                                  // 0x540880
    [[nodiscard]] bool GetGroupControlForward() const noexcept              { return !DisablePlayerControls && BUTTON_IS_DOWN(DPadUp); }                                     // 0x541190
    [[nodiscard]] bool GetGroupControlBack() const noexcept                 { return !DisablePlayerControls && BUTTON_IS_DOWN(DPadDown); }                                    // 0x5411B0
    [[nodiscard]] bool ConversationYesJustDown() const noexcept             { return !DisablePlayerControls && IsDPadRightPressed(); }                                       // 0x5411D0
    [[nodiscard]] bool ConversationNoJustDown() const noexcept              { return !DisablePlayerControls && IsDPadLeftPressed(); }                                        // 0x541200
    [[nodiscard]] bool GroupControlForwardJustDown() const noexcept         { return !DisablePlayerControls && IsDPadUpPressed(); }                                          // 0x541230
    [[nodiscard]] bool GroupControlBackJustDown() const noexcept            { return !DisablePlayerControls && IsDPadDownPressed(); }                                        // 0x541260

    // KEYBOARD
    [[nodiscard]] bool IsFKeyJustDown(eFKeyID key) const noexcept           { return NewKeyState.FKeys[key] && OldKeyState.FKeys[key]; }
    [[nodiscard]] bool IsFKeyJustPressed(eFKeyID key) const noexcept        { return NewKeyState.FKeys[key] && !OldKeyState.FKeys[key]; }
    [[nodiscard]] bool IsF1JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY1); }
    [[nodiscard]] bool IsF2JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY2); }
    [[nodiscard]] bool IsF3JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY3); }
    [[nodiscard]] bool IsF4JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY4); }
    [[nodiscard]] bool IsF5JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY5); }
    [[nodiscard]] bool IsF6JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY6); }
    [[nodiscard]] bool IsF7JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY7); }
    [[nodiscard]] bool IsF8JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY8); }
    [[nodiscard]] bool IsF9JustPressed() const noexcept                     { return IsFKeyJustPressed(FKEY9); }
    [[nodiscard]] bool IsF10JustPressed() const noexcept                    { return IsFKeyJustPressed(FKEY10); }
    [[nodiscard]] bool IsF11JustPressed() const noexcept                    { return IsFKeyJustPressed(FKEY11); }
    [[nodiscard]] bool IsF12JustPressed() const noexcept                    { return IsFKeyJustPressed(FKEY12); }

    [[nodiscard]] bool IsStandardKeyJustDown(uint8 key) const noexcept      { return NewKeyState.standardKeys[key] && OldKeyState.standardKeys[key]; }                       //
    [[nodiscard]] bool IsStandardKeyJustPressed(uint8 key) const noexcept   { return NewKeyState.standardKeys[key] && !OldKeyState.standardKeys[key]; }                      // 0x4D59B0
    [[nodiscard]] bool IsStandardKeyUp(uint8 key) const noexcept            { return !NewKeyState.standardKeys[key] && !OldKeyState.standardKeys[key]; }                     //
    [[nodiscard]] bool IsLeftCtrlJustPressed() const noexcept               { return KEY_IS_PRESSED(lctrl); }                                                                //
    [[nodiscard]] bool IsRightCtrlJustPressed() const noexcept              { return KEY_IS_PRESSED(rctrl); }                                                                //
    [[nodiscard]] bool IsCtrlJustDown() const noexcept                      { return IsLeftCtrlJustPressed() || IsRightCtrlJustPressed(); }                                  //
    [[nodiscard]] bool IsLeftCtrlJustDown() const noexcept                  { return NewKeyState.lctrl && OldKeyState.lctrl; }                                               //
    [[nodiscard]] bool IsRightCtrlJustDown() const noexcept                 { return NewKeyState.rctrl && OldKeyState.rctrl; }                                               //
    [[nodiscard]] bool IsCtrlPressed() const noexcept                       { return IsLeftCtrlJustDown() || IsRightCtrlJustDown(); }                                        //
    [[nodiscard]] static bool IsRightDown() noexcept                        { return KEY_IS_DOWN(right); }                                                                   //
    [[nodiscard]] static bool IsLeftDown() noexcept                         { return KEY_IS_DOWN(left); }                                                                    //
    [[nodiscard]] static bool IsUpDown() noexcept                           { return KEY_IS_DOWN(up); }                                                                      //
    [[nodiscard]] static bool IsDownDown() noexcept                         { return KEY_IS_DOWN(down); }                                                                    //
    [[nodiscard]] static bool IsPgUpDown() noexcept                         { return KEY_IS_DOWN(pgup); }                                                                    //
    [[nodiscard]] static bool IsPgDnDown() noexcept                         { return KEY_IS_DOWN(pgdn); }                                                                    //
    [[nodiscard]] static bool IsUpPressed() noexcept                        { return KEY_IS_PRESSED(up); }                                                                   //
    [[nodiscard]] static bool IsDownPressed() noexcept                      { return KEY_IS_PRESSED(down); }                                                                 //
    [[nodiscard]] static bool IsLeftPressed() noexcept                      { return KEY_IS_PRESSED(left); }                                                                 //
    [[nodiscard]] static bool IsRightPressed() noexcept                     { return KEY_IS_PRESSED(right); }                                                                //
    static bool IsPadEnterJustPressed() noexcept                            { return KEY_IS_PRESSED(enter); }                                                                //
    static bool IsReturnJustPressed() noexcept                              { return KEY_IS_PRESSED(extenter); }                                                             //
    static bool IsEnterJustPressed() noexcept                               { return IsPadEnterJustPressed() || IsReturnJustPressed(); }                                     // 0x4D5980
    static bool f0x57C330() { return (!NewKeyState.enter && OldKeyState.enter) || (!NewKeyState.extenter && OldKeyState.extenter); }                                         // 0x57C330

    static bool IsMenuKeyJustPressed() noexcept                             { return KEY_IS_PRESSED(lalt); }                                                                 // 0x744D50
    static bool IsTabJustPressed() noexcept                                 { return KEY_IS_PRESSED(tab); }                                                                  // 0x744D90
    static bool IsEscJustPressed() noexcept                                 { return KEY_IS_PRESSED(esc); }                                                                  // 0x572DB0
    static bool IsBackspacePressed() noexcept                               { return KEY_IS_PRESSED(back); }                                                                 // 0x57C360

    // KEYBOARD END

    // PAD
    [[nodiscard]] bool f0x57C3A0() const noexcept                           { return !NewState.ButtonCross && OldState.ButtonCross; }                                        // 0x57C3A0
    [[nodiscard]] bool IsCrossPressed() const noexcept                      { return BUTTON_IS_PRESSED(ButtonCross); }                                                       // 0x4D59E0
    [[nodiscard]] bool IsCirclePressed() const noexcept                     { return BUTTON_IS_PRESSED(ButtonCircle); }                                                      // 0x53EF60
    [[nodiscard]] bool IsTrianglePressed() const noexcept                   { return BUTTON_IS_PRESSED(ButtonTriangle); }                                                    // 0x53EF40
    [[nodiscard]] bool IsSquarePressed() const noexcept                     { return BUTTON_IS_PRESSED(ButtonSquare); }                                                      // 0x53EF20

    [[nodiscard]] bool IsCrossDown() const noexcept                         { return BUTTON_IS_DOWN(ButtonCross); }                                                       // 0x4D59E0
    [[nodiscard]] bool IsCircleDown() const noexcept                        { return BUTTON_IS_DOWN(ButtonCircle); }                                                      // 0x53EF60
    [[nodiscard]] bool IsTriangleDown() const noexcept                      { return BUTTON_IS_DOWN(ButtonTriangle); }                                                    // 0x53EF40
    [[nodiscard]] bool IsSquareDown() const noexcept                        { return BUTTON_IS_DOWN(ButtonSquare); }                                                      // 0x53EF20

    [[nodiscard]] bool IsLeftShockPressed() const noexcept                  { return BUTTON_IS_PRESSED(ShockButtonL); }                                                      // 0x509840
    [[nodiscard]] bool IsRightShockPressed() const noexcept                 { return BUTTON_IS_PRESSED(ShockButtonR); }                                                      //

    [[nodiscard]] bool IsRightStickYPressed() const noexcept                { return BUTTON_IS_PRESSED(RightStickY); }                                                       // 0x53ED90

    [[nodiscard]] bool IsStartPressed() const noexcept                      { return BUTTON_IS_PRESSED(Start); }                                                             //
    [[nodiscard]] bool IsSelectPressed() const noexcept                     { return BUTTON_IS_PRESSED(Select); }                                                            // 0x53EF00

    [[nodiscard]] bool IsDPadLeftPressed() const noexcept                   { return BUTTON_IS_PRESSED(DPadLeft); }                                                          // 0x53EEE0
    [[nodiscard]] bool IsDPadRightPressed() const noexcept                  { return BUTTON_IS_PRESSED(DPadRight); }                                                         //
    [[nodiscard]] bool IsDPadUpPressed() const noexcept                     { return BUTTON_IS_PRESSED(DPadUp); }                                                            // 0x53EE60
    [[nodiscard]] bool IsDPadDownPressed() const noexcept                   { return BUTTON_IS_PRESSED(DPadDown); }                                                          // 0x53EEA0

    [[nodiscard]] bool IsDPadLeftJustUp() const noexcept                    { return BUTTON_JUST_UP(DPadLeft); }                                                             //
    [[nodiscard]] bool f0x541170() const noexcept                           { return !DisablePlayerControls && NewState.DPadLeft != 0; }                                     // 0x541170
    [[nodiscard]] bool f0x57C380() const noexcept                           { return BUTTON_IS_DOWN(DPadLeft); }                                                    // 0x57C380

    [[nodiscard]] bool IsDPadRightJustUp() const noexcept                   { return BUTTON_JUST_UP(DPadRight); }                                                            //
    [[nodiscard]] bool f0x541150() const noexcept                           { return !DisablePlayerControls && NewState.DPadRight != 0; }                                    // 0x541150
    [[nodiscard]] bool f0x57C390() const noexcept                           { return BUTTON_IS_DOWN(DPadRight); }                                                              // 0x57C390

    [[nodiscard]] bool IsDPadUpJustUp() const noexcept                      { return BUTTON_JUST_UP(DPadUp); }                                                               // 0x53EE80
    [[nodiscard]] bool IsDPadDownJustUp() const noexcept                    { return BUTTON_JUST_UP(DPadDown); }                                                             // 0x53EEC0

    [[nodiscard]] bool IsLeftShoulder1Pressed() const noexcept              { return BUTTON_IS_PRESSED(LeftShoulder1); }                                                     // 0x53EDC0
    [[nodiscard]] bool IsLeftShoulder1() const noexcept                     { return BUTTON_IS_DOWN(LeftShoulder1); }                                                        // 0x53EDB0
    bool f0x4D5970()                                                        { return NewState.LeftShoulder1 == 0; }                                                          // 0x4D5970

    [[nodiscard]] bool IsLeftShoulder2JustUp() const noexcept               { return BUTTON_JUST_UP(LeftShoulder2); }                                                        // 0x53EE00
    [[nodiscard]] bool IsLeftShoulder2Pressed() const noexcept              { return BUTTON_IS_PRESSED(LeftShoulder2); }                                                     // 0x53EDE0
    [[nodiscard]] bool IsLeftShoulder2() const noexcept                     { return BUTTON_IS_DOWN(LeftShoulder2); }                                                        //

    [[nodiscard]] bool IsRightShoulder1Pressed() const noexcept             { return BUTTON_IS_PRESSED(RightShoulder1); }                                                    // 0x53EE20
    [[nodiscard]] bool IsRightShoulder1Up() const noexcept                  { return BUTTON_JUST_UP(RightShoulder1); }                                                       //

    [[nodiscard]] bool IsRightShoulder2Pressed() const noexcept             { return BUTTON_IS_PRESSED(RightShoulder2); }                                                    //
    [[nodiscard]] bool IsRightShoulder2JustUp() const noexcept              { return BUTTON_JUST_UP(RightShoulder2); }                                                       // 0x53EE40
    [[nodiscard]] bool IsRightShoulder2() const noexcept                    { return BUTTON_IS_DOWN(RightShoulder2); }                                                       //

    [[nodiscard]] bool IsRadioTrackSkipJustUp() const noexcept              { return BUTTON_JUST_UP(m_bRadioTrackSkip); }                                                    //
    [[nodiscard]] bool IsRadioTrackSkipPressed() const noexcept             { return BUTTON_IS_PRESSED(m_bRadioTrackSkip); }                                                 // 0x4E7F20

    // returns angle in degrees
    [[nodiscard]] int16 GetLeftStickX() const noexcept                      { return BUTTON_IS_DOWN(LeftStickX); }
    [[nodiscard]] int16 GetLeftStickY() const noexcept                      { return BUTTON_IS_DOWN(LeftStickY); }
    [[nodiscard]] int16 GetRightStickX() const noexcept                     { return BUTTON_IS_DOWN(RightStickX); }
    [[nodiscard]] int16 GetRightStickY() const noexcept                     { return BUTTON_IS_DOWN(RightStickY); }

    bool IsSteeringInAnyDirection() { return GetSteeringLeftRight() || GetSteeringUpDown(); }

    // PAD END

    // MOUSE
    static bool f0x57C3C0() noexcept               { return !NewMouseControllerState.isMouseLeftButtonPressed && OldMouseControllerState.isMouseLeftButtonPressed; }         // 0x57C3C0
    static bool IsMouseLButtonPressed() noexcept   { return MOUSE_IS_PRESSED(isMouseLeftButtonPressed); }                                                                    // 0x4D5A00
    static bool IsMouseRButtonPressed() noexcept   { return MOUSE_IS_PRESSED(isMouseRightButtonPressed); }                                                                   // 0x572E70
    static bool IsMouseMButtonPressed() noexcept   { return MOUSE_IS_PRESSED(isMouseMiddleButtonPressed); }                                                                  // 0x57C3E0
    static bool IsMouseWheelUpPressed() noexcept   { return MOUSE_IS_PRESSED(isMouseWheelMovedUp); }                                                                         // 0x57C400
    static bool IsMouseWheelDownPressed() noexcept { return MOUSE_IS_PRESSED(isMouseWheelMovedDown); }                                                                       // 0x57C420
    static bool IsMouseBmx1Pressed() noexcept      { return MOUSE_IS_PRESSED(isMouseFirstXPressed); }                                                                        // 0x57C440
    static bool IsMouseBmx2Pressed() noexcept      { return MOUSE_IS_PRESSED(isMouseSecondXPressed); }                                                                       // 0x57C460
    static bool IsMouseLButton() noexcept          { return MOUSE_IS_DOWN(isMouseLeftButtonPressed); }                                                                       // 0x45AF70
    static bool IsMouseRButton() noexcept          { return MOUSE_IS_DOWN(isMouseRightButtonPressed); }                                                                      // 0x45AF80
    static bool IsMouseMButton() noexcept          { return MOUSE_IS_DOWN(isMouseMiddleButtonPressed); }                                                                     //
    static bool IsMouseWheelUp() noexcept          { return MOUSE_IS_DOWN(isMouseWheelMovedUp); }                                                                            // 0x572E60
    static bool IsMouseWheelDown() noexcept        { return MOUSE_IS_DOWN(isMouseWheelMovedDown); }                                                                          // 0x572E50
    static bool IsMouseBmx1() noexcept             { return MOUSE_IS_DOWN(isMouseFirstXPressed); }                                                                           //
    static bool IsMouseBmx2() noexcept             { return MOUSE_IS_DOWN(isMouseSecondXPressed); }                                                                          //
    // MOUSE END

    int16 LookAroundLeftRight(CPed* entity) noexcept;
    int16 LookAroundUpDown(CPed* ped) noexcept;

    int32 sub_541320() { return AverageWeapon / AverageEntries; } // 0x541320
    int32 sub_541290();
    static bool sub_540A40();
    static bool sub_540A10();
    static bool GetAnaloguePadLeft();
    static bool GetAnaloguePadUp();
    static bool GetAnaloguePadRight();
    static bool GetAnaloguePadDown();
    bool sub_540530() const noexcept;
    bool sub_5404F0() const noexcept { return Mode != 1 ? 0 : IsDPadDownPressed(); } // 0x5404F0
    bool IsPhaseEqual11() const noexcept { return Phase == 11; } // 0x53FB60

    static void SetCurrentPad(int8 pad) { padNumber = pad; } // 0x53ED70

    // 0x541A60
    static bool UpdatePadsTillStable() { return true; }
    bool ArePlayerControlsDisabled() { return DisablePlayerControls != 0; }
    bool DebugMenuJustPressed();
};

// Layout checks: gta-reversed VALIDATE_SIZEs, enforced only on 32-bit targets
// (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CControllerState) == 0x30, "CControllerState layout drift");
static_assert(sizeof(CKeyboardState) == 0x270, "CKeyboardState layout drift");
static_assert(sizeof(CMouseControllerState) == 0x14, "CMouseControllerState layout drift");
static_assert(sizeof(CPad) == 0x134, "CPad layout drift");
#endif

// return pressed key, in order of CKeyboardState
int GetCurrentKeyPressed(RsKeyCodes& keys);

// TODO: input subsystem - the original also declares DirectInput mouse helpers
// (IDirectInputDevice8* DIReleaseMouse(); void InitialiseMouse(bool exclusive);)
// under `#ifndef NOTSA_USE_SDL3`. Dropped here: DirectInput-only. Re-add with the
// input backend. The Android build exposes ~99 pad funcs; the Win32 surface above
// is what's ported.
