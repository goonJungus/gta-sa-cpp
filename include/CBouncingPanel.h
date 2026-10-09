// CBouncingPanel.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/BouncingPanel.h
// Bouncing vehicle panel (bonnet/boot/bumpers).
// Hierarchy: standalone (member of CAutomobile)
//
// Adaptations:
//   stripped InjectHooks()
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   StaticRef statics -> plain static members (defined in CBouncingPanel.cpp)
//   RwFrame -> forward-declared
// TODO:
//   verify each method against decomp src/CBouncingPanel/*.c

#pragma once

#include <cstdint>

#include "GrTypes.h"

#include "CVector.h"

class CVehicle;
struct RwFrame;



class CVehicle;
struct RwFrame;

class  CBouncingPanel {
    static float BOUNCE_SPRING_DAMP_MULT; // game address: 0x8D3954 ; DEFERRED - was StaticRef (was: 0.95)
    static float BOUNCE_SPRING_RETURN_MULT; // game address: 0x8D3958 ; DEFERRED - was StaticRef (was: 0.1)
    static float BOUNCE_VEL_CHANGE_LIMIT; // game address: 0x8D395C ; DEFERRED - was StaticRef (was: 0.1)
    static float BOUNCE_HANGING_DAMP_MULT; // game address: 0x8D3960 ; DEFERRED - was StaticRef (was: 0.98)
    static float BOUNCE_HANGING_RETURN_MULT; // game address: 0x8D3964 ; DEFERRED - was StaticRef (was: 0.02)

public:
    uint16 m_nFrameId{(uint16)-1};
    uint16 m_nAxis{};
    float    m_fAngleLimit{};
    CVector  m_vecRotation{};
    CVector  m_vecPos{};

public:
    CBouncingPanel() = default;


    void ResetPanel();
    void SetPanel(int16 frameId, int16 axis, float angleLimit);
    float GetAngleChange(float velocity) const;
    void ProcessPanel(CVehicle* vehicle, RwFrame* frame, CVector arg2, CVector arg3, float arg4, float arg5);
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CBouncingPanel) == 0x20, "CBouncingPanel size mismatch");
#endif
