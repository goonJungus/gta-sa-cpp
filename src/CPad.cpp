// CPad - adapted from gta-reversed for clean-room C++ build
// Method bodies ported from decompiled: src/CPad/*.c, src/_global/GetCurrentKeyPressed_00541490.c
//
// Conversion notes:
// - Ghidra types mapped: undefined4->uint32, undefined2->uint16,
//   undefined1->uint8, short->int16, uchar->uint8, char->char/int8 per use.
//   __thiscall/__cdecl stripped; MSVC SEH frame boilerplate removed.
// - The x87 float<->int conversion thunk (CGeneral::unk_00821b40 @ 0x821B40)
//   is resolved inline: (int16)/(uint32) casts at the call sites.
// - UpdateMouse/ProcessPad are DirectInput-only: the converted logic is kept
//   in `#if 0` blocks with TODO(port) naming the missing platform pieces
//   (DirectInput device, RsGlobal, CControllerConfigManager, FrontEndMenuManager
//   fields); the active bodies are empty so the tree keeps building.
// - CControllerConfigManager (ControlsManager) calls in UpdatePads: same
//   `#if 0` + TODO(port) treatment.
// - File-static originals: byte_B73401/B73403 (0xB73401/0xB73403),
//   byte_8CD782 (0x8CD782), the 0xB736C0 reconcile scratch, and the six
//   oldfStickX/Y edge-detector values.
// - CCamera::m_bUseMouse3rdPerson lives as a file-static in CCamera.cpp; this
//   TU uses the new CCamera::GetUseMouse3rdPerson() accessor.
// - FrontEndMenuManager::m_PrefsUseVibration and CCutsceneMgr::ms_running are
//   not ported: TU-local shims with TODO(port), defaults match a fresh game.
// - CPed::m_pAttachedTo is not in the ported CPed.h: PedIsAttachedTo() shim
//   (TODO(port)) used by LookAround*/AimWeapon*.
// - GetMeleeAttack's inverted-looking bCheckButtonCircleStateOnly polarity is
//   original (it checks MORE buttons when true); ported literally.

#include "CPad.h"

#include "CCamera.h"         // GetUseMouse3rdPerson, TheCamera
#include "CPed.h"            // bIsInTheAir, GetIntelligence, m_pVehicle
#include "CPedIntelligence.h"// GetTaskUseGun, IsUsingGun
#include "CTimer.h"          // SetTouched / StartShake timestamps
#include "CVehicle.h"        // IsTrain, m_pDriver
#include "CWorld.h"          // FindPlayerVehicle

#include <algorithm> // std::max/std::min
#include <cmath>     // std::abs
#include <cstdlib>   // std::rand
#include <cstring>   // std::memset

// ---------------------------------------------------------------------------
// Original binary globals -> file-statics.
// ---------------------------------------------------------------------------

static bool s_bUnkB73401{};                // 0xB73401 - unused, unknown (zeroed by Initialise)
static bool s_bPad3rdPersonMouseLook{};     // 0xB73403 - TODO: find what modifies this
static bool s_bPedWalkUseLeftStick{true};  // 0x8CD782 - true by default
static CControllerState s_reconcileTemp{}; // 0xB736C0 - ReconcileTwoControllersInput scratch

static int16 s_oldfStickY_Up{};            // 0xB736F0 - GetAnaloguePadUp
static int16 s_oldfStickY_Down{};          // 0xB736F4 - GetAnaloguePadDown
static int16 s_oldfStickX_Left{};          // 0xB736F8 - GetAnaloguePadLeft
static int16 s_oldfStickX_Right{};         // 0xB736FC - GetAnaloguePadRight
static int16 s_oldfStickX_540A10{};        // 0xB73700 - sub_540A10
static int16 s_oldfStickX_540A40{};        // 0xB73704 - sub_540A40

// ---------------------------------------------------------------------------
// Shims for not-yet-ported pieces. Declared here only so this TU
// syntax-checks; the real declarations belong in their subsystem headers.
// TODO(port): delete this block as the subsystems land (see BUILD_NOTES.md).
// ---------------------------------------------------------------------------

// FrontEndMenuManager::m_PrefsUseVibration (0xBA6768) not ported.
static bool s_bPrefsUseVibration{true};
// CCutsceneMgr::ms_running not ported.
static bool s_bCutsceneRunning{};

// CPed::m_pAttachedTo not in the ported CPed.h yet.
static bool PedIsAttachedTo(const CPed* ped) {
    // TODO(port): return ped->m_pAttachedTo != nullptr once CPed.h has it.
    (void)ped;
    return false;
}

// "Can this ped fire a gun right now" test shared by WeaponJustDown,
// GetWeapon and GetDisplayVitalStats. Decompiled: GetTaskUseGun() != null,
// else simplest active task type == 0x3FE (TASK_SIMPLE_USE_GUN)
// [WeaponJustDown/GetWeapon also accept ped->m_pAttachedTo].
// CTaskManager is unported, so those arms are TODO(port);
// CPedIntelligence::IsUsingGun() folds them in when it lands.
static bool CanPedFireGun(CPed* ped) {
    if (!ped)
        return false;
    CPedIntelligence* const intel = ped->GetIntelligence();
    if (!intel)
        return false;
    if (intel->GetTaskUseGun())
        return true;
    // TODO(port): || CTaskManager::GetSimplestActiveTask(&intel->m_TaskMgr)->GetTaskType() == 0x3FE
    //             || ped->m_pAttachedTo   (WeaponJustDown/GetWeapon only)
    return intel->IsUsingGun();
}

// ---------------------------------------------------------------------------
// Static member definitions.
// ---------------------------------------------------------------------------

CMouseControllerState CPad::TempMouseControllerState{}; // 0xB73424 (via StaticRef)
CMouseControllerState CPad::NewMouseControllerState{};
CMouseControllerState CPad::OldMouseControllerState{};

CKeyboardState CPad::TempKeyState{};
CKeyboardState CPad::OldKeyState{};
CKeyboardState CPad::NewKeyState{};

CPad CPad::Pads[MAX_PADS]{};

bool CPad::bInvertLook4Pad{};
char CPad::padNumber{};

// ---------------------------------------------------------------------------
// CControllerState
// ---------------------------------------------------------------------------

void CControllerState::Clear() {
    std::memset(this, 0, sizeof(*this));
}

bool CControllerState::CheckForInput() {
    const int16* const fields = &LeftStickX;
    for (size_t i = 0; i < sizeof(*this) / sizeof(int16); ++i) {
        if (fields[i] != 0)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// CKeyboardState
// ---------------------------------------------------------------------------

void CKeyboardState::Clear() {
    std::memset(this, 0, sizeof(*this));
}

// ---------------------------------------------------------------------------
// CMouseControllerState
// ---------------------------------------------------------------------------

CMouseControllerState::CMouseControllerState() {
    Clear();
}

CMouseControllerState* CMouseControllerState::Constructor() {
    new (this) CMouseControllerState();
    return this;
}

void CMouseControllerState::Clear() {
    std::memset(this, 0, sizeof(*this));
}

bool CMouseControllerState::CheckForInput() const {
    return isMouseLeftButtonPressed || isMouseRightButtonPressed ||
           isMouseMiddleButtonPressed || isMouseWheelMovedUp ||
           isMouseWheelMovedDown || isMouseFirstXPressed ||
           isMouseSecondXPressed || m_fWheelMoved != 0.0f ||
           m_AmountMoved.x != 0.0f || m_AmountMoved.y != 0.0f;
}

// ---------------------------------------------------------------------------
// CPad
// ---------------------------------------------------------------------------

// 0x541D80
CPad::CPad() {
    Clear(true, true);
}

// 0x541D90
void CPad::Initialise() {
    for (int32 i = 0; i < MAX_PADS; ++i) {
        Pads[i].Clear(true, true);
        Pads[i].Mode = 0;
    }
    padNumber = 0;
    s_bUnkB73401 = false; // byte_B73401 = 0
}

// 0x53F1E0
void CPad::ClearKeyBoardHistory() {
    NewKeyState.Clear();
    OldKeyState.Clear();
    TempKeyState.Clear();
}

// 0x541BD0
void CPad::ClearMouseHistory() {
    TempMouseControllerState.Clear();
    NewMouseControllerState.Clear();
    OldMouseControllerState.Clear();
}

// 0x541A70
void CPad::Clear(bool enablePlayerControls, bool resetPhase) {
    NewState.Clear();
    OldState.Clear();
    PCTempKeyState.Clear();
    PCTempJoyState.Clear();
    PCTempMouseState.Clear();
    ClearKeyBoardHistory();
    // NOTE: the original clears only the 7 button bools here, leaving
    // m_fWheelMoved/m_AmountMoved alone (no CMouseControllerState::Clear()).
    auto clearMouseButtons = [](CMouseControllerState& ms) {
        ms.isMouseLeftButtonPressed = false;
        ms.isMouseRightButtonPressed = false;
        ms.isMouseMiddleButtonPressed = false;
        ms.isMouseWheelMovedUp = false;
        ms.isMouseWheelMovedDown = false;
        ms.isMouseFirstXPressed = false;
        ms.isMouseSecondXPressed = false;
    };
    clearMouseButtons(NewMouseControllerState);
    clearMouseButtons(OldMouseControllerState);
    clearMouseButtons(TempMouseControllerState);

    if (resetPhase)
        Phase = 0;
    ShakeFreq = 0;
    ShakeDur = 0;
    SteeringLeftRightBuffer.fill(0);
    DrunkDrivingBufferUsed = 0;

    if (enablePlayerControls) {
        DisablePlayerControls = 0;
        bDisablePlayerEnterCar = false;
        bDisablePlayerDuck = false;
        bDisablePlayerFireWeapon = false;
        bDisablePlayerFireWeaponWithL1 = false;
        bDisablePlayerCycleWeapon = false;
        bDisablePlayerJump = false;
        bDisablePlayerDisplayVitalStats = false;
    }
    JustOutOfFrontEnd = 0;
    bApplyBrakes = false;
    bHornHistory.fill(0);
    iCurrHornHistory = 0;
    AverageWeapon = 0;
    AverageEntries = 0;
    LastTimeTouched = 0;
    NoShakeBeforeThis = 0.0f;
    NoShakeFreq = 0;
}

// 0x541C40
void CPad::Update(int32 pad) {
    (void)pad; // unused in the original
    OldState = NewState;

    // Writes directly into NewState (via a stack temp in the original).
    CControllerState reconciled;
    ReconcileTwoControllersInput(reconciled, PCTempKeyState, PCTempJoyState);
    NewState = reconciled;
    ReconcileTwoControllersInput(reconciled, PCTempMouseState, NewState);
    NewState = reconciled;

    PCTempJoyState.Clear();
    PCTempKeyState.Clear();
    PCTempMouseState.Clear();

    if (NewState.CheckForInput())
        SetTouched();

    iCurrHornHistory = (iCurrHornHistory + 1) % 5;

    bool horn = false;
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 3:
            horn = NewState.ShockButtonL != 0;
            break;
        case 1:
            horn = NewState.LeftShoulder1 != 0;
            break;
        case 2:
            horn = NewState.RightShoulder1 != 0;
            break;
        default:
            break;
        }
    }
    bHornHistory[(uint8)iCurrHornHistory] = horn;

    // Shift the steering delay line right by 1 (index 0 is written by
    // GetSteeringLeftRight).
    for (size_t i = SteeringLeftRightBuffer.size() - 1; i > 0; --i)
        SteeringLeftRightBuffer[i] = SteeringLeftRightBuffer[i - 1];

    if (JustOutOfFrontEnd)
        --JustOutOfFrontEnd;
}

// 0x541DD0
void CPad::UpdatePads() {
    GetPad(0)->UpdateMouse();
    ProcessPad(PAD1);

#if 0 // TODO(port): CControllerConfigManager (ControlsManager) not ported yet.
    ControlsManager.ClearSimButtonPressCheckers();
    ControlsManager.AffectPadFromKeyBoard();
    ControlsManager.AffectPadFromMouse();
#endif

    GetPad(PAD1)->Update(PAD1);
    GetPad(PAD2)->Update(PAD2);

    OldKeyState = NewKeyState;
    NewKeyState = TempKeyState;
}

// 0x53F3C0 - DirectInput-only. Converted logic kept for the input-backend
// port; TODO(port): DirectInput mouse device (RsGlobal diMouse),
// IsForegroundApp, FrontEndMenuManager (m_bMenuActive, mouse-invert prefs),
// IsVideoModeExclusive, diMouseInit.
void CPad::UpdateMouse() {
#if 0
    if (!IsForegroundApp())
        return;

    if (!s_bMouseInitialised) { // DAT_00c8cfa4
        const bool exclusive = !FrontEndMenuManager.m_bMenuActive && IsVideoModeExclusive();
        diMouseInit(exclusive);
        if (!s_bMouseInitialised)
            return;
    }

    DIMOUSESTATE2 mouseState{};
    if (GetMouseState(&mouseState) < 0) // FAILED(hr)
        return;

    int32 invertX = 1, invertY = 1;
    if (!FrontEndMenuManager.m_bMenuActive) {
        // _bInvertMouseX packs both axes: low byte = X, high byte = Y.
        if (FrontEndMenuManager.m_bInvertMouseX)
            invertX = -1;
        if (FrontEndMenuManager.m_bInvertMouseY)
            invertY = -1;
    }

    TempMouseControllerState.isMouseLeftButtonPressed = (mouseState.rgbButtons[0] & 0x80) != 0;
    TempMouseControllerState.m_AmountMoved.x = (float)(mouseState.lX * invertX);
    TempMouseControllerState.isMouseRightButtonPressed = (mouseState.rgbButtons[1] & 0x80) != 0;
    TempMouseControllerState.isMouseMiddleButtonPressed = (mouseState.rgbButtons[2] & 0x80) != 0;
    TempMouseControllerState.m_AmountMoved.y = (float)(mouseState.lY * invertY);
    TempMouseControllerState.m_fWheelMoved = (float)mouseState.lZ;
    TempMouseControllerState.isMouseFirstXPressed = (mouseState.rgbButtons[3] & 0x80) != 0;
    TempMouseControllerState.isMouseSecondXPressed = (mouseState.rgbButtons[4] & 0x80) != 0;
    TempMouseControllerState.isMouseWheelMovedUp = mouseState.lZ > 0;
    TempMouseControllerState.isMouseWheelMovedDown = mouseState.lZ < 0;

    OldMouseControllerState = NewMouseControllerState;
    NewMouseControllerState = TempMouseControllerState;

    if (NewMouseControllerState.CheckForInput())
        SetTouched();
#endif
}

// 0x746A10 - DirectInput-only. Converted logic kept for the input-backend
// port; TODO(port): DirectInput joystick devices (RsGlobal diDevice1/2),
// CControllerConfigManager joy state, RsPadEventHandler, AllValidWinJoys,
// FrontEndMenuManager pad prefs (invert/swap axes).
void CPad::ProcessPad(ePadID padID) {
#if 0
    constexpr float kAxisScale = 1.0f / 2000.0f; // deviceAxisMin/Max = -2000/2000

    IDirectInputDevice8* device = nullptr;
    if (padID == PAD1)
        device = RsGlobal.ps->diDevice1;
    else if (padID == PAD2)
        device = RsGlobal.ps->diDevice2;
    else
        return;
    if (!device)
        return;

    HRESULT hr = device->Poll();
    if (FAILED(hr)) {
        hr = device->Acquire();
        while (hr == DIERR_INPUTLOST)
            hr = device->Acquire();
        if (FAILED(hr))
            return;
        hr = device->Poll();
        if (FAILED(hr))
            return;
    }

    DIJOYSTATE2 joyState{};
    if (FAILED(device->GetDeviceState(sizeof(joyState), &joyState)))
        return;

    // Track new/old device state in the controller config manager.
    DIJOYSTATE2 prevJoyState = std::exchange(ControlsManager.m_NewJoyState, joyState);
    if (ControlsManager.m_bJoyJustInitialised) {
        ControlsManager.m_OldJoyState = ControlsManager.m_NewJoyState;
        ControlsManager.m_bJoyJustInitialised = false;
    } else {
        ControlsManager.m_OldJoyState = prevJoyState;
    }

    RsPadEventHandler(RsEvent::rsPADBUTTONUP, &padID);

    float leftX = (float)joyState.lX * kAxisScale;
    float leftY = (float)joyState.lY * kAxisScale;
    if (LOWORD(joyState.rgdwPOV[0]) != 0xFFFF) { // POV hat overrides the stick
        const float angle = (float)joyState.rgdwPOV[0] / 100.0f * (3.14159265f / 180.0f);
        leftX = sinf(angle);
        leftY = -cosf(angle);
    }
    float rightX = 0.0f, rightY = 0.0f;
    if (AllValidWinJoys.JoyStickNum[padID].bZRotPresent &&
        AllValidWinJoys.JoyStickNum[padID].bZAxisPresent) {
        rightX = (float)joyState.lZ * kAxisScale;
        rightY = (float)joyState.lRz * kAxisScale;
    }

    RsPadEventHandler(RsEvent::rsPADBUTTONUP, &padID);
    RsPadEventHandler(RsEvent::rsPADBUTTONDOWN, &padID);

    CPad& pad = *GetPad(padID);
    auto updateStick = [](float pos, int16& outNormal, int16& outSwapped, bool invert, bool swap) {
        if (std::fabs(pos) <= 0.3f)
            return;
        if (invert)
            pos = -pos;
        (swap ? outSwapped : outNormal) = (int16)(pos * 128.0f);
    };
    // Left stick: X then Y (note the crossed outNormal/outSwapped pairs).
    updateStick(leftX, pad.PCTempJoyState.LeftStickY, pad.PCTempJoyState.LeftStickX,
                FrontEndMenuManager.m_bInvertPadX1, FrontEndMenuManager.m_bSwapPadAxis1);
    updateStick(leftY, pad.PCTempJoyState.LeftStickX, pad.PCTempJoyState.LeftStickY,
                FrontEndMenuManager.m_bInvertPadY1, FrontEndMenuManager.m_bSwapPadAxis1);
    // Right stick.
    updateStick(rightX, pad.PCTempJoyState.RightStickY, pad.PCTempJoyState.RightStickX,
                FrontEndMenuManager.m_bInvertPadX2, FrontEndMenuManager.m_bSwapPadAxis2);
    updateStick(rightY, pad.PCTempJoyState.RightStickX, pad.PCTempJoyState.RightStickY,
                FrontEndMenuManager.m_bInvertPadY2, FrontEndMenuManager.m_bSwapPadAxis2);
#endif
}

// 0x53FB40 - empty in the original.
void CPad::ProcessPCSpecificStuff() {
}

// 0x53F530
CControllerState& CPad::ReconcileTwoControllersInput(CControllerState& out,
        const CControllerState& controllerA, const CControllerState& controllerB) {
    s_reconcileTemp.Clear();

    // Buttons: pressed on either controller -> full deflection (0xFF).
    static int16 CControllerState::*const kButtons[] = {
        &CControllerState::LeftShoulder1, &CControllerState::LeftShoulder2,
        &CControllerState::RightShoulder1, &CControllerState::RightShoulder2,
        &CControllerState::Start, &CControllerState::Select,
        &CControllerState::ButtonSquare, &CControllerState::ButtonTriangle,
        &CControllerState::ButtonCross, &CControllerState::ButtonCircle,
        &CControllerState::ShockButtonL, &CControllerState::ShockButtonR,
        &CControllerState::m_bChatIndicated, &CControllerState::m_bPedWalk,
        &CControllerState::m_bRadioTrackSkip, &CControllerState::m_bVehicleMouseLook,
    };
    for (auto field : kButtons) {
        if (controllerA.*field || controllerB.*field)
            s_reconcileTemp.*field = 0xFF;
    }

    // Sticks: same direction -> the stronger deflection wins;
    // opposite directions cancel to 0.
    auto reconcileAxis = [](int16 a, int16 b) -> int16 {
        if ((a > 0 && b < 0) || (a < 0 && b > 0))
            return 0;
        if (a >= 0 && b >= 0)
            return std::max(a, b);
        if (a <= 0 && b <= 0)
            return std::min(a, b);
        return 0;
    };
    s_reconcileTemp.LeftStickX = reconcileAxis(controllerA.LeftStickX, controllerB.LeftStickX);
    s_reconcileTemp.LeftStickY = reconcileAxis(controllerA.LeftStickY, controllerB.LeftStickY);
    s_reconcileTemp.RightStickX = reconcileAxis(controllerA.RightStickX, controllerB.RightStickX);
    s_reconcileTemp.RightStickY = reconcileAxis(controllerA.RightStickY, controllerB.RightStickY);

    // DPad: pressed on either -> 0xFF.
    if (controllerA.DPadUp || controllerB.DPadUp)
        s_reconcileTemp.DPadUp = 0xFF;
    if (controllerA.DPadDown || controllerB.DPadDown)
        s_reconcileTemp.DPadDown = 0xFF;
    if (controllerA.DPadLeft || controllerB.DPadLeft)
        s_reconcileTemp.DPadLeft = 0xFF;
    if (controllerA.DPadRight || controllerB.DPadRight)
        s_reconcileTemp.DPadRight = 0xFF;

    // DPad fights the stick: if both push opposite ways, drop both.
    if ((s_reconcileTemp.DPadUp || s_reconcileTemp.LeftStickY < 0) &&
        (s_reconcileTemp.DPadDown || s_reconcileTemp.LeftStickY > 0)) {
        s_reconcileTemp.DPadUp = 0;
        s_reconcileTemp.DPadDown = 0;
        s_reconcileTemp.LeftStickY = 0;
    }
    if ((s_reconcileTemp.DPadLeft || s_reconcileTemp.LeftStickX < 0) &&
        (s_reconcileTemp.DPadRight || s_reconcileTemp.LeftStickX > 0)) {
        s_reconcileTemp.DPadLeft = 0;
        s_reconcileTemp.DPadRight = 0;
        s_reconcileTemp.LeftStickX = 0;
    }

    out = s_reconcileTemp;
    return out;
}

// 0x53F200
void CPad::SetTouched() {
    LastTimeTouched = CTimer::m_snTimeInMilliseconds;
}

// 0x53F210
uint32 CPad::GetTouchedTimeDelta() const {
    return CTimer::m_snTimeInMilliseconds - LastTimeTouched;
}

// 0x53F920 (body at 0x53F925)
void CPad::StartShake(int16 time, uint8 frequency, uint32 arg2) {
    // in_AL = FrontEndMenuManager::m_PrefsUseVibration.
    if (!s_bPrefsUseVibration || s_bCutsceneRunning)
        return;
    if (frequency == 0) {
        ShakeDur = 0;
        ShakeFreq = 0;
        return;
    }
    // NOTE: the original reads/writes NoShakeBeforeThis as a raw uint32
    // millisecond timestamp here, despite the float declaration in the header.
    uint32& noShakeBefore = reinterpret_cast<uint32&>(NoShakeBeforeThis);
    if (noShakeBefore <= CTimer::m_snTimeInMilliseconds || NoShakeFreq < frequency) {
        if (ShakeDur < time) {
            ShakeDur = time;
            ShakeFreq = (char)frequency;
        }
        noShakeBefore = CTimer::m_snTimeInMilliseconds + arg2;
        NoShakeFreq = (char)frequency;
    }
}

// 0x53F9A0
void CPad::StartShake_Distance(int16 time, uint8 frequency, CVector pos) {
    if (!s_bPrefsUseVibration || s_bCutsceneRunning)
        return;
    const CVector camPos = *TheCamera.GetGameCamPosition();
    const float dx = camPos.x - pos.x;
    const float dy = camPos.y - pos.y;
    const float dz = camPos.z - pos.z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 70.0f) {
        if (frequency == 0) {
            ShakeDur = 0;
            ShakeFreq = 0;
            return;
        }
        if (ShakeDur < time) {
            ShakeDur = time;
            ShakeFreq = (char)frequency;
        }
    }
}

// 0x53FA70
void CPad::StartShake_Train(const CVector2D& point) {
    if (!s_bPrefsUseVibration || s_bCutsceneRunning)
        return;
    CVehicle* const vehicle = FindPlayerVehicle(-1, false);
    if (vehicle && vehicle->IsTrain())
        return;
    const CVector camPos = *TheCamera.GetGameCamPosition();
    const float dx = camPos.x - point.x;
    const float dy = camPos.y - point.y;
    if (dx * dx + dy * dy < 4900.0f /* 70^2 */ && ShakeDur < 100) {
        ShakeDur = 100;
        // Decompiled: ShakeFreq = (char)float->int-thunk(ST0) with the float
        // source lost by Ghidra; reconstructed as a random frequency.
        // TODO(port): verify against disassembly.
        ShakeFreq = (char)(std::rand() & 0xFF);
    }
}

// 0x541D70 - empty in the original.
void CPad::StopPadsShaking() {
}

// 0x53FB50 - empty in the original.
void CPad::StopShaking(int16 pad) {
    (void)pad;
}

// 0x53FC50
int16 CPad::GetCarGunLeftRight() const {
    if (!DisablePlayerControls) {
        if ((uint16)Mode < 3)
            return NewState.RightStickX;
        if (Mode == 3)
            return (int16)((NewState.DPadRight - NewState.DPadLeft) / 2);
    }
    return 0;
}

// 0x53FC10
int16 CPad::GetCarGunUpDown() const {
    if (!DisablePlayerControls) {
        if ((uint16)Mode < 3)
            return NewState.RightStickY;
        if (Mode == 3)
            return (int16)((NewState.DPadUp - NewState.DPadDown) / 2);
    }
    return 0;
}

// 0x53FF90
int16 CPad::GetCarGunFired() const {
    if (!DisablePlayerControls) {
        if ((uint16)Mode < 3) {
            if (NewState.ButtonCircle)
                return 1;
            if (!bDisablePlayerFireWeaponWithL1 && NewState.LeftShoulder1)
                return 2;
        } else if (Mode == 3 && NewState.RightShoulder1) {
            return 1;
        }
    }
    return 0;
}

// 0x53FFE0
int16 CPad::CarGunJustDown() const {
    if (!DisablePlayerControls) {
        if ((uint16)Mode < 3) {
            if (NewState.ButtonCircle && !OldState.ButtonCircle)
                return 1;
            if (!bDisablePlayerFireWeaponWithL1 && IsLeftShoulder1Pressed())
                return 2;
        } else if (Mode == 3 && NewState.RightShoulder1 && !OldState.RightShoulder1) {
            return 1;
        }
    }
    return 0;
}

// 0x53FB80
int16 CPad::GetSteeringLeftRight() {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 2:
        case 3:
            SteeringLeftRightBuffer[0] = NewState.LeftStickX;
            // Drunk-driving delay line; index 0 = newest.
            return SteeringLeftRightBuffer[DrunkDrivingBufferUsed];
        default:
            break;
        }
    }
    return 0;
}

// 0x53FBD0
int16 CPad::GetSteeringUpDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 2:
        case 3:
            return NewState.LeftStickY;
        default:
            break;
        }
    }
    return 0;
}

// 0x540DC0
int16 CPad::GetPedWalkLeftRight(CPed* ped) const {
    if (DisablePlayerControls)
        return 0;
    int16 result = 0;
    if (s_bPad3rdPersonMouseLook || CCamera::GetUseMouse3rdPerson()) {
        if (ped && ped->bIsInTheAir)
            return 0;
        result = s_bPedWalkUseLeftStick ? NewState.LeftStickX : NewState.RightStickX;
    }
    return result;
}

// 0x53FC90
int16 CPad::GetPedWalkLeftRight() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 2:
        case 3:
            return NewState.LeftStickX;
        default:
            break;
        }
    }
    return 0;
}

// 0x540E20
int16 CPad::GetPedWalkUpDown(CPed* ped) const {
    if (DisablePlayerControls)
        return 0;
    int16 result = 0;
    if (s_bPad3rdPersonMouseLook || CCamera::GetUseMouse3rdPerson()) {
        if (ped && ped->bIsInTheAir)
            return 0;
        result = s_bPedWalkUseLeftStick ? NewState.LeftStickY : NewState.RightStickY;
    }
    return result;
}

// 0x53FD30
int16 CPad::GetPedWalkUpDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 2:
        case 3:
            return NewState.LeftStickY;
        default:
            break;
        }
    }
    return 0;
}

// 0x53FDD0
bool CPad::GetLookLeft() const {
    return !DisablePlayerControls &&
           NewState.LeftShoulder2 && OldState.LeftShoulder2 &&
           !NewState.RightShoulder2 && !OldState.RightShoulder2;
}

// 0x53FE10
bool CPad::GetLookRight() const {
    return !DisablePlayerControls &&
           NewState.RightShoulder2 && OldState.RightShoulder2 &&
           !NewState.LeftShoulder2 && !OldState.LeftShoulder2;
}

// 0x53FEE0
bool CPad::GetHorn() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 3:
            return NewState.ShockButtonL != 0;
        case 1:
            return NewState.LeftShoulder1 != 0;
        case 2:
            return NewState.RightShoulder1 != 0;
        default:
            break;
        }
    }
    return false;
}

// 0x53FF30
bool CPad::HornJustDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 3:
            return NewState.ShockButtonL && !OldState.ShockButtonL;
        case 1:
            return IsLeftShoulder1Pressed();
        case 2:
            return IsRightShoulder1Pressed();
        default:
            break;
        }
    }
    return false;
}

// 0x540080
int16 CPad::GetBrake() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 2:
            return NewState.ButtonSquare;
        case 3: {
            // max(2 * RightStickY, 0); the decompiled masks off negatives.
            const int32 scaled = (int32)NewState.RightStickY * 2;
            return (int16)(scaled < 0 ? 0 : scaled);
        }
        default:
            break;
        }
    }
    return 0;
}

// 0x540040
int16 CPad::GetHandBrake() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
            return NewState.RightShoulder1;
        case 2:
            return NewState.ButtonTriangle;
        case 3:
            return NewState.LeftShoulder1;
        default:
            break;
        }
    }
    return 0;
}

// 0x5400D0
bool CPad::GetExitVehicle() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 3:
            return NewState.ButtonTriangle != 0;
        case 2:
            return NewState.LeftShoulder1 != 0;
        default:
            break;
        }
    }
    return false;
}

// 0x540120
bool CPad::ExitVehicleJustDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 3:
            return NewState.ButtonTriangle && !OldState.ButtonTriangle;
        case 2:
            return NewState.LeftShoulder1 && !OldState.LeftShoulder1;
        default:
            break;
        }
    }
    return false;
}

// 0x540340
// NOTE: the inverted-looking polarity is original: with
// bCheckButtonCircleStateOnly true it checks MORE buttons, not fewer.
uint8 CPad::GetMeleeAttack(bool bCheckButtonCircleStateOnly) const {
    if (!DisablePlayerControls) {
        if (NewState.ButtonCircle)
            return 1;
        if (bCheckButtonCircleStateOnly) {
            if (NewState.ButtonCross)
                return 2;
            if (NewState.ButtonSquare)
                return 3;
            if (NewState.ButtonTriangle)
                return 4;
        }
    }
    return 0;
}

// 0x540390
uint8 CPad::MeleeAttackJustDown(bool bCheckButtonCircleStateOnly) const {
    if (!DisablePlayerControls) {
        if (NewState.ButtonCircle && !OldState.ButtonCircle)
            return 1;
        if (bCheckButtonCircleStateOnly) {
            if (NewState.ButtonCross && !OldState.ButtonCross)
                return 2;
            if (NewState.ButtonSquare)
                return 3;
            if (IsTrianglePressed())
                return 4;
        }
    }
    return 0;
}

// 0x5403F0
int16 CPad::GetAccelerate() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 2:
            return NewState.ButtonCross;
        case 3: {
            // max(-2 * RightStickY, 0); the decompiled masks off negatives.
            const int32 scaled = (int32)NewState.RightStickY * -2;
            return (int16)(scaled < 0 ? 0 : scaled);
        }
        default:
            break;
        }
    }
    return 0;
}

// 0x540440
bool CPad::GetAccelerateJustDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 2:
            return NewState.ButtonCross && !OldState.ButtonCross;
        case 1:
            return IsCrossPressed();
        case 3:
            return NewState.RightStickY && !OldState.RightStickY;
        default:
            break;
        }
    }
    return false;
}

// 0x540610
bool CPad::CycleWeaponLeftJustDown() const {
    return !DisablePlayerControls && !bDisablePlayerCycleWeapon &&
           NewState.LeftShoulder2 && !OldState.LeftShoulder2;
}

// 0x540640
bool CPad::CycleWeaponRightJustDown() const {
    return !DisablePlayerControls && !bDisablePlayerCycleWeapon &&
           NewState.RightShoulder2 && !OldState.RightShoulder2;
}

// 0x540670
bool CPad::GetTarget() const {
    if (!DisablePlayerControls) {
        if ((uint16)Mode < 3)
            return NewState.RightShoulder1 != 0;
        if (Mode == 3)
            return NewState.LeftShoulder1 != 0;
    }
    return false;
}

// 0x5407A0
bool CPad::GetSprint() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 3:
            return NewState.ButtonCross != 0;
        case 2:
            return NewState.ButtonCircle != 0;
        default:
            break;
        }
    }
    return false;
}

// 0x5407F0
bool CPad::SprintJustDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 3:
            return NewState.ButtonCross && !OldState.ButtonCross;
        case 2:
            return NewState.ButtonCircle && !OldState.ButtonCircle;
        default:
            break;
        }
    }
    return false;
}

// 0x5408B0
int16 CPad::GetDisplayVitalStats(CPed* ped) const {
    if (DisablePlayerControls || bDisablePlayerDisplayVitalStats)
        return 0;
    // NOTE: unlike WeaponJustDown/GetWeapon, the original has no
    // m_pAttachedTo arm here.
    const bool aiming = CanPedFireGun(ped);
    if ((uint16)Mode < 4 && !aiming)
        return NewState.LeftShoulder1 != 0;
    return 0;
}

// 0x540A70
bool CPad::CollectPickupJustDown() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
            return NewState.LeftShoulder1 && !OldState.LeftShoulder1;
        case 2:
            return NewState.ButtonTriangle && !OldState.ButtonTriangle;
        case 3:
            return NewState.ButtonCircle && !OldState.ButtonCircle;
        default:
            break;
        }
    }
    return false;
}

// 0x540AE0
bool CPad::GetForceCameraBehindPlayer() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
            return NewState.LeftShoulder1 != 0;
        case 2:
            return NewState.ButtonTriangle != 0;
        case 3:
            return NewState.ButtonCircle != 0;
        default:
            break;
        }
    }
    return false;
}

// 0x540B30
bool CPad::SniperZoomIn() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 3:
            return NewState.LeftShoulder2 || NewState.ButtonSquare;
        case 2:
            return NewState.ButtonTriangle != 0;
        default:
            break;
        }
    }
    return false;
}

// 0x540B80
bool CPad::SniperZoomOut() const {
    if (!DisablePlayerControls) {
        switch (Mode) {
        case 0:
        case 1:
        case 3:
            return NewState.RightShoulder2 || NewState.ButtonCross;
        case 2:
            return NewState.ButtonSquare != 0;
        default:
            break;
        }
    }
    return false;
}

// 0x540250
bool CPad::WeaponJustDown(CPed* ped) const {
    if (DisablePlayerControls || bDisablePlayerFireWeapon)
        return false;
    const bool canFire = CanPedFireGun(ped);
    bool fireWithL1 = canFire;
    if (bDisablePlayerFireWeaponWithL1)
        fireWithL1 = false;
    switch (Mode) {
    case 0:
    case 1:
        break;
    case 2:
        return IsCrossPressed();
    case 3:
        return IsRightShoulder1Pressed();
    default:
        return false;
    }
    return IsCirclePressed() || (fireWithL1 && IsLeftShoulder1Pressed());
}

// 0x5406B0
bool CPad::GetEnterTargeting() const {
    if (DisablePlayerControls)
        return false;
    if ((uint16)Mode < 3)
        return NewState.RightShoulder1 && !OldState.RightShoulder1;
    if (Mode == 3)
        return NewState.LeftShoulder1 && !OldState.LeftShoulder1;
    return false;
}

// 0x540180
int32 CPad::GetWeapon(CPed* ped) const {
    if (DisablePlayerControls || bDisablePlayerFireWeapon)
        return 0;
    const bool canFire = CanPedFireGun(ped);
    bool fireWithL1 = canFire;
    if (bDisablePlayerFireWeaponWithL1)
        fireWithL1 = false;
    switch (Mode) {
    case 0:
    case 1: {
        const int32 circle = NewState.ButtonCircle;
        if (fireWithL1)
            return circle + NewState.LeftShoulder1;
        return circle;
    }
    case 2:
        return NewState.ButtonCross;
    case 3:
        return NewState.RightShoulder1;
    default:
        break;
    }
    return 0;
}

// 0x541040
int16 CPad::AimWeaponLeftRight(CPed* ped) const {
    if (DisablePlayerControls)
        return 0;
    int16 x = NewState.RightStickX;
    const int16 leftX = NewState.LeftStickX;
    if (!CCamera::GetUseMouse3rdPerson() && ped &&
        (PedIsAttachedTo(ped) ||
         (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle->m_pDriver != ped))) {
        if (std::abs(x) < std::abs(leftX))
            x = leftX;
    }
    return x;
}

// 0x5410C0
int16 CPad::AimWeaponUpDown(CPed* ped) const {
    if (DisablePlayerControls)
        return 0;
    int16 y = NewState.RightStickY;
    const int16 leftY = NewState.LeftStickY;
    if (!CCamera::GetUseMouse3rdPerson() && ped &&
        (PedIsAttachedTo(ped) ||
         (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle->m_pDriver != ped))) {
        if (std::abs(y) < std::abs(leftY))
            y = leftY;
    }
    if (bInvertLook4Pad)
        y = -y;
    return y;
}

// 0x53FE70
bool CPad::GetLookBehindForCar() const noexcept {
    if (DisablePlayerControls)
        return false;
    const bool l2 = NewState.LeftShoulder2 || OldState.LeftShoulder2;
    const bool r2 = NewState.RightShoulder2 || OldState.RightShoulder2;
    return l2 && r2;
}

// 0x540BD0
int16 CPad::LookAroundLeftRight(CPed* entity) noexcept {
    if (DisablePlayerControls)
        return 0;
    int16 main, alt;
    if (!s_bPedWalkUseLeftStick) {
        main = NewState.LeftStickX;
        alt = NewState.RightStickX;
    } else {
        main = NewState.RightStickX;
        alt = NewState.LeftStickX;
    }
    if (!CCamera::GetUseMouse3rdPerson() &&
        (s_bPad3rdPersonMouseLook == 0 || (entity && PedIsAttachedTo(entity))) &&
        std::abs(main) < std::abs(alt)) {
        main = alt;
    }
    if (std::abs(main) > 35.0f) {
        // Deadzone 35, rescaled to the full [-128, 128] range
        // (128/93). Decompiled: float result via the 0x821B40 thunk.
        const float rescaled = (main > 0 ? main - 35 : main + 35) * (128.0f / 93.0f);
        return (int16)rescaled;
    }
    return 0;
}

// 0x540CC0
int16 CPad::LookAroundUpDown(CPed* ped) noexcept {
    if (DisablePlayerControls)
        return 0;
    int16 main, alt;
    if (!s_bPedWalkUseLeftStick) {
        main = NewState.LeftStickY;
        alt = NewState.RightStickY;
    } else {
        main = NewState.RightStickY;
        alt = NewState.LeftStickY;
    }
    if (!CCamera::GetUseMouse3rdPerson() &&
        (s_bPad3rdPersonMouseLook == 0 || (ped && PedIsAttachedTo(ped))) &&
        std::abs(main) < std::abs(alt)) {
        main = alt;
    }
    if (bInvertLook4Pad)
        main = -main;
    if (std::abs(main) > 35.0f) {
        const float rescaled = (main > 0 ? main - 35 : main + 35) * (128.0f / 93.0f);
        return (int16)rescaled;
    }
    return 0;
}

// 0x541290 - samples the current weapon-input value into
// AverageWeapon/AverageEntries (read back via sub_541320()).
// NOTE: the decompiled export is void; the header declares int32, so the
// sampled value is returned.
int32 CPad::sub_541290() {
    if (DisablePlayerControls == 0 && !bDisablePlayerFireWeapon) {
        switch (Mode) {
        case 0:
        case 1:
            AverageWeapon = NewState.ButtonCircle;
            AverageEntries = 1;
            return AverageWeapon;
        case 2:
            AverageWeapon = NewState.ButtonCross;
            AverageEntries = 1;
            return AverageWeapon;
        case 3:
            AverageWeapon = NewState.RightShoulder1;
            break;
        default:
            AverageWeapon = 0;
            break;
        }
        AverageEntries = 1;
        return AverageWeapon;
    }
    AverageWeapon = 0;
    AverageEntries = 1;
    return AverageWeapon;
}

// 0x540A40 - fires once when the left stick returns to center from the right.
bool CPad::sub_540A40() {
    const int16 leftStickX = GetPad()->GetLeftStickX();
    if (leftStickX == 0 && s_oldfStickX_540A40 > 0) {
        s_oldfStickX_540A40 = leftStickX;
        return true;
    }
    s_oldfStickX_540A40 = leftStickX;
    return false;
}

// 0x540A10 - fires once when the left stick returns to center from the left.
bool CPad::sub_540A10() {
    const int16 leftStickX = GetPad()->GetLeftStickX();
    if (leftStickX == 0 && s_oldfStickX_540A10 < 0) {
        s_oldfStickX_540A10 = leftStickX;
        return true;
    }
    s_oldfStickX_540A10 = leftStickX;
    return false;
}

// 0x540950
bool CPad::GetAnaloguePadUp() {
    const int16 y = GetPad()->GetLeftStickY();
    if (y < -15 && s_oldfStickY_Up >= -5) {
        s_oldfStickY_Up = y;
        return true;
    }
    s_oldfStickY_Up = y;
    return false;
}

// 0x5409B0
bool CPad::GetAnaloguePadLeft() {
    const int16 x = GetPad()->GetLeftStickX();
    if (x < -15 && s_oldfStickX_Left >= -5) {
        s_oldfStickX_Left = x;
        return true;
    }
    s_oldfStickX_Left = x;
    return false;
}

// 0x5409E0
bool CPad::GetAnaloguePadRight() {
    const int16 x = GetPad()->GetLeftStickX();
    if (x > 15 && s_oldfStickX_Right <= 5) {
        s_oldfStickX_Right = x;
        return true;
    }
    s_oldfStickX_Right = x;
    return false;
}

// 0x540980
bool CPad::GetAnaloguePadDown() {
    const int16 y = GetPad()->GetLeftStickY();
    if (y > 15 && s_oldfStickY_Down <= 5) {
        s_oldfStickY_Down = y;
        return true;
    }
    s_oldfStickY_Down = y;
    return false;
}

// 0x540530
bool CPad::sub_540530() const noexcept {
    switch (Mode) {
    case 0:
    case 2:
    case 3: {
        if (!NewState.Select)
            return sub_5404F0();
        if (OldState.Select == 0)
            return true;
        return sub_5404F0();
    }
    case 1: {
        if (NewState.DPadUp && OldState.DPadUp == 0)
            return true;
        return sub_5404F0();
    }
    default:
        return false;
    }
}

// DebugMenuJustDown - from the public source (no decompiled export):
// Ctrl+M or F7.
bool CPad::DebugMenuJustPressed() {
    return (IsCtrlPressed() && IsStandardKeyJustPressed('M')) || IsF7JustPressed();
}

// 0x541490
int GetCurrentKeyPressed(RsKeyCodes& keys) {
    keys = 0x420;
    for (int k = 0; k < 0xFF; ++k) {
        if (CPad::NewKeyState.standardKeys[k] && !CPad::OldKeyState.standardKeys[k])
            keys = k;
    }
    for (int k = 0; k < 12; ++k) {
        if (CPad::NewKeyState.FKeys[k] && !CPad::OldKeyState.FKeys[k])
            keys = 0x3E9 + k;
    }
    // Named keys, in decompiled order (later entries win ties).
    static const struct {
        int16 CKeyboardState::*key;
        int code;
    } kNamedKeys[] = {
        { &CKeyboardState::esc, 1000 },
        { &CKeyboardState::insert, 0x3F5 }, { &CKeyboardState::del, 0x3F6 },
        { &CKeyboardState::home, 0x3F7 }, { &CKeyboardState::end, 0x3F8 },
        { &CKeyboardState::pgup, 0x3F9 }, { &CKeyboardState::pgdn, 0x3FA },
        { &CKeyboardState::up, 0x3FB }, { &CKeyboardState::down, 0x3FC },
        { &CKeyboardState::left, 0x3FD }, { &CKeyboardState::right, 0x3FE },
        { &CKeyboardState::scroll, 0x410 }, { &CKeyboardState::pause, 0x411 },
        { &CKeyboardState::numlock, 0x409 }, { &CKeyboardState::div, 0x3FF },
        { &CKeyboardState::mul, 0x400 }, { &CKeyboardState::sub, 0x402 },
        { &CKeyboardState::add, 0x401 }, { &CKeyboardState::enter, 0x40F },
        { &CKeyboardState::decimal, 0x403 }, { &CKeyboardState::num1, 0x404 },
        { &CKeyboardState::num2, 0x405 }, { &CKeyboardState::num3, 0x406 },
        { &CKeyboardState::num4, 0x407 }, { &CKeyboardState::num5, 0x408 },
        { &CKeyboardState::num6, 0x40A }, { &CKeyboardState::num7, 0x40B },
        { &CKeyboardState::num8, 0x40C }, { &CKeyboardState::num9, 0x40D },
        { &CKeyboardState::num0, 0x40E }, { &CKeyboardState::back, 0x412 },
        { &CKeyboardState::tab, 0x413 }, { &CKeyboardState::capslock, 0x414 },
        { &CKeyboardState::extenter, 0x415 }, { &CKeyboardState::lshift, 0x416 },
        { &CKeyboardState::shift, 0x418 }, { &CKeyboardState::rshift, 0x417 },
        { &CKeyboardState::lctrl, 0x419 }, { &CKeyboardState::rctrl, 0x41A },
        { &CKeyboardState::lalt, 0x41B }, { &CKeyboardState::ralt, 0x41C },
        { &CKeyboardState::lwin, 0x41D }, { &CKeyboardState::rwin, 0x41E },
        { &CKeyboardState::apps, 0x41F },
    };
    for (const auto& nk : kNamedKeys) {
        if ((CPad::NewKeyState.*(nk.key)) != 0 && (CPad::OldKeyState.*(nk.key)) == 0)
            keys = nk.code;
    }
    return keys;
}
