// CTimer - adapted from gta-reversed for clean-room C++ build
// Method bodies ported from decompiled: src/CTimer/*.c
//   Initialise_005617e0, Shutdown_005618c0, Suspend_005619d0, Resume_00561a00,
//   Stop_00561aa0, StartUserPause_00561af0, EndUserPause_00561b00,
//   GetCyclesPerMillisecond_00561a40, GetCyclesPerFrame_00561a50,
//   GetCurrentTimeInCycles_00561a80, GetIsSlowMotionActive_00561ad0,
//   UpdateVariables_005618d0, Update_00561b10, GetMillisecondTime (0x5617C0)
//
// Conversion notes:
// - Ghidra types mapped: ulonglong->uint64_t, uint->uint32, BOOL->bool.
//   __cdecl stripped. MSVC SEH frame boilerplate removed.
// - The decompiled reads/writes the x87 FPU through an opaque float<->int
//   conversion thunk (CGeneral::unk_00821b40 @ 0x821b40); Ghidra renders the
//   surrounding float math lossily (a dropped /m_snTimerDivider and `=` for
//   `+=`). The ported UpdateVariables follows the tested public-source form:
//   frameDelta = timeElapsed / m_snTimerDivider (ms), NonClipped +=
//   (uint32_t)frameDelta, ms_fTimeStepNonClipped = frameDelta / 20.0f,
//   m_snTimeInMilliseconds += min(frameDelta, 300.0f), then the 0.01 floor
//   (unpaused) and clamp to [0.00001f, 3.0f].
// - GetMillisecondTime() is a free function (RsTimer() == timeGetTime()).
// - Win32 performance-counter API: real includes on Windows, minimal
//   declarations on other platforms so the TU syntax-checks with g++
//   (TODO(port): platform timer abstraction).

#include "CTimer.h"

#include <algorithm> // std::min/std::max/std::clamp (UpdateVariables)

#ifdef _WIN32
#define NOMINMAX // windows.h min/max macros break std::min/std::max (2026-10-09)
#include <windows.h>  // QueryPerformanceFrequency/QueryPerformanceCounter
#include <mmsystem.h> // timeGetTime
#pragma comment(lib, "winmm.lib")
#else
// Minimal Win32 timer API surface used below (see note above).
struct LARGE_INTEGER {
    union {
        struct {
            uint32_t LowPart;
            int32_t  HighPart;
        };
        int64_t QuadPart;
    };
};
extern "C" {
int      QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
int      QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
uint32_t timeGetTime();
}
#endif

// ---------------------------------------------------------------------------
// Shims for not-yet-ported pieces. Declared here only so this TU
// syntax-checks; the real declarations belong in their subsystem headers.
// TODO(port): delete this block as the subsystems land (see BUILD_NOTES.md).
// ---------------------------------------------------------------------------

// CSpecialFX.h not ported yet.
struct CSpecialFX {
    static bool bSnapShotActive;
};
bool CSpecialFX::bSnapShotActive = false;

// ---------------------------------------------------------------------------

// OS timer helpers (gta-reversed oswrapper.h equivalents).
static uint64_t GetOSWPerformanceFrequency() {
    LARGE_INTEGER freq{};
    return QueryPerformanceFrequency(&freq) ? (uint64_t)freq.QuadPart : 0;
}

static uint64_t GetOSWPerformanceTime() {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart;
}

// 0x5617C0 - free function, 64-bit RsTimer() wrapper.
uint64_t GetMillisecondTime() {
    return (uint64_t)timeGetTime();
}

// StaticRef<T>(addr) -> plain statics (see header).
// Original GTA SA 1.0 addresses kept as comments.
CTimer::TimerFunction_t CTimer::ms_fnTimerFunction{}; // 0xB7CB28

bool    CTimer::m_sbEnableTimeDebug{};      // 0xB7CB40
bool    CTimer::bSkipProcessThisFrame{};    // 0xB7CB89
bool    CTimer::bSlowMotionActive{};        // 0xB7CB88
float   CTimer::game_FPS{};                 // 0xB7CB50

bool    CTimer::m_CodePause{};              // 0xB7CB48
bool    CTimer::m_UserPause{};              // 0xB7CB49
uint32  CTimer::m_FrameCounter{};           // 0xB7CB4C
float   CTimer::ms_fTimeStepNonClipped{};   // 0xB7CB58
float   CTimer::ms_fTimeStep{};             // 0xB7CB5C
uint32  CTimer::m_snTimerDivider{};         // 0xB7CB2C

float   CTimer::ms_fOldTimeStep{};          // 0xB7CB54
float   CTimer::ms_fSlowMotionScale{};      // 0xB7CB60

float   CTimer::ms_fTimeScale{1.0f};                    // 0xB7CB64
uint32  CTimer::m_snTimeInMillisecondsPauseMode{};      // 0xB7CB7C
uint32  CTimer::m_snTimeInMillisecondsNonClipped{};     // 0xB7CB80
uint32  CTimer::m_snPreviousTimeInMillisecondsNonClipped{}; // 0xB7CB68
uint32  CTimer::m_snTimeInMilliseconds{};               // 0xB7CB84
uint64_t CTimer::m_snRenderStartTime{};                 // 0xB7CB38
uint64_t CTimer::m_snRenderPauseTime{};                 // 0xB7CB30
uint32  CTimer::m_snRenderTimerPauseCount{};            // 0xB7CB44

uint32  CTimer::m_snPPPPreviousTimeInMilliseconds{};    // 0xB7CB6C
uint32  CTimer::m_snPPPreviousTimeInMilliseconds{};     // 0xB7CB70
uint32  CTimer::m_snPPreviousTimeInMilliseconds{};      // 0xB7CB74
uint32  CTimer::m_snPreviousTimeInMilliseconds{};       // 0xB7CB78

// 0x5617E0
void CTimer::Initialise() {
    m_UserPause = false;
    m_CodePause = false;
    bSlowMotionActive = false;
    bSkipProcessThisFrame = false;
    m_snPPPPreviousTimeInMilliseconds = 0;
    m_snPPPreviousTimeInMilliseconds = 0;
    m_snPPreviousTimeInMilliseconds = 0;
    m_snPreviousTimeInMilliseconds = 0;
    m_snPreviousTimeInMillisecondsNonClipped = 0;
    m_snTimeInMilliseconds = 0;
    m_FrameCounter = 0;
    m_snRenderTimerPauseCount = 0;
    m_sbEnableTimeDebug = false;
    game_FPS = 0.0f;
    m_snTimeInMillisecondsNonClipped = 1;
    m_snTimeInMillisecondsPauseMode = 1;
    ms_fTimeScale = 1.0f;
    ms_fSlowMotionScale = -1.0f; // unused
    ms_fTimeStep = 1.0f;
    ms_fOldTimeStep = 1.0f;

    TimerFunction_t timerFunc;
    const uint64_t frequency = GetOSWPerformanceFrequency();
    if (frequency) {
        timerFunc = GetOSWPerformanceTime;
        m_snTimerDivider = (uint32_t)(frequency / 1000);
    } else {
        timerFunc = GetMillisecondTime;
        m_snTimerDivider = 1;
    }
    ms_fnTimerFunction = timerFunc;
    m_snRenderStartTime = timerFunc();
}

// 0x5618C0
void CTimer::Shutdown() {
    m_sbEnableTimeDebug = false;
}

// 0x5619D0
void CTimer::Suspend() {
    if (m_sbEnableTimeDebug) {
        if (++m_snRenderTimerPauseCount <= 1)
            m_snRenderPauseTime = ms_fnTimerFunction();
    }
}

// 0x561A00
void CTimer::Resume() {
    if (m_sbEnableTimeDebug) {
        if (!--m_snRenderTimerPauseCount)
            m_snRenderStartTime = ms_fnTimerFunction() - m_snRenderPauseTime + m_snRenderStartTime;
    }
}

// 0x561AA0
void CTimer::Stop() {
    m_snPPPPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPPPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_sbEnableTimeDebug = false;
    m_snPreviousTimeInMillisecondsNonClipped = m_snTimeInMillisecondsNonClipped;
}

// 0x561AF0
void CTimer::StartUserPause() {
    m_UserPause = true;
}

// 0x561B00
void CTimer::EndUserPause() {
    m_UserPause = false;
}

// 0x561A40
uint32 CTimer::GetCyclesPerMillisecond() {
    return m_snTimerDivider;
}

// 0x561A50 - cycles per ms * 20
uint32 CTimer::GetCyclesPerFrame() {
    return (uint32_t)((float)m_snTimerDivider * 20.0f);
}

// 0x561A80
uint32 CTimer::GetCurrentTimeInCycles() {
    // TODO: make it use 64-bit timestamps (public-source note).
    return (uint32_t)(GetOSWPerformanceTime() - m_snRenderStartTime);
}

// 0x561AD0
bool CTimer::GetIsSlowMotionActive() {
    return ms_fTimeScale < 1.0f;
}

// 0x5618D0
void CTimer::UpdateVariables(float timeElapsed) {
    const float frameDelta = timeElapsed / (float)m_snTimerDivider;

    m_snTimeInMillisecondsNonClipped += (uint32_t)frameDelta;
    ms_fTimeStepNonClipped = frameDelta / TIMESTEP_LEN_IN_MS;
    m_snTimeInMilliseconds += (uint32_t)std::min(frameDelta, 300.0f);

    if (!m_UserPause && !m_CodePause && !CSpecialFX::bSnapShotActive) {
        // Make it be something at least, to avoid division by 0
        ms_fTimeStepNonClipped = std::max(ms_fTimeStepNonClipped, 0.01f);
    }
    ms_fOldTimeStep = ms_fTimeStep;
    SetTimeStep(std::clamp(ms_fTimeStepNonClipped, 0.00001f, 3.0f));
}

// 0x561B10
void CTimer::Update() {
    if (!ms_fnTimerFunction)
        return;
    m_sbEnableTimeDebug = true;

    game_FPS = 1000.0f / (float)(m_snTimeInMillisecondsNonClipped - m_snPreviousTimeInMillisecondsNonClipped);

    // Update history
    m_snPPPPreviousTimeInMilliseconds = m_snPPPreviousTimeInMilliseconds;
    m_snPPPreviousTimeInMilliseconds = m_snPPreviousTimeInMilliseconds;
    m_snPPreviousTimeInMilliseconds = m_snPreviousTimeInMilliseconds;
    m_snPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPreviousTimeInMillisecondsNonClipped = m_snTimeInMillisecondsNonClipped;

    const uint64_t nRenderTimeBefore = m_snRenderStartTime;
    m_snRenderStartTime = ms_fnTimerFunction();

    float fTimeDelta = (float)(m_snRenderStartTime - nRenderTimeBefore);
    if (!GetIsPaused())
        fTimeDelta *= ms_fTimeScale;

    m_snTimeInMillisecondsPauseMode += (uint32_t)(fTimeDelta / (float)m_snTimerDivider);

    if (GetIsPaused())
        fTimeDelta = 0.0f;
    UpdateVariables(fTimeDelta);
    m_FrameCounter++;
}
