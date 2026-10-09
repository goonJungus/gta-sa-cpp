// Fx.h - minimal stand-in (gta-reversed/source/game_sa/Fx/Fx.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Values verified 2026-10-09 vs gta-reversed.
// The full Fx system lands with the effects batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include "Common.h"
#include "CVector.h"
#include "RenderWare.h" // RwRGBA, RwRGBAReal

#include <cstdint>

// gta-reversed/source/game_sa/Fx/Fx.h (values verified 2026-10-09).
enum class eSparkType : int32_t {
    SPARK_PARTICLE_SPARK2 = 0,
    // TODO(port): full eSparkType enum.
};

// gta-reversed/source/game_sa/Fx/FxPrtMult.h (verified 2026-10-09).
class FxPrtMult_c {
public:
    RwRGBAReal m_Color{};
    float      m_fSize{};
    float      m_Rot{};
    float      m_fLife{};

    FxPrtMult_c() = default;
    FxPrtMult_c(float red, float green, float blue, float alpha, float size, float rot, float life)
        : m_fSize(size), m_Rot(rot), m_fLife(life) {
        m_Color.red = red; m_Color.green = green; m_Color.blue = blue; m_Color.alpha = alpha;
    }
    // TODO(port): real SetUp.
    void SetUp(float red, float green, float blue, float alpha, float size, float rot, float life) {
        (void)red; (void)green; (void)blue; (void)alpha; (void)size; (void)rot; (void)life;
    }
};

// Minimal Fx_c stand-in (gta-reversed/source/game_sa/Fx/Fx.h).
// TODO(port): full Fx_c implementation.
class Fx_c {
public:
    class FxSystem_c* m_SmokeHuge{};      // gta-reversed Fx.h
    class FxSystem_c* m_SmokeII3expand{}; // gta-reversed Fx.h

    // TODO(port): real implementations.
    void AddSparks(const CVector& origin, const CVector& direction, float force, int32_t amount, CVector across, eSparkType sparksType, float spread, float life) {
        (void)origin; (void)direction; (void)force; (void)amount; (void)across; (void)sparksType; (void)spread; (void)life;
    }
    void AddDebris(const CVector& posn, const RwRGBA& color, float scale, int32_t amount) {
        (void)posn; (void)color; (void)scale; (void)amount;
    }
    void TriggerTankFire(const CVector& origin, const CVector& target) {
        (void)origin; (void)target;
    }
    // gta-reversed Fx.h: void TriggerWaterSplash(const CVector& posn).
    // TODO(port): real implementation. Added 2026-10-09 for CPed.
    void TriggerWaterSplash(const CVector& posn) {
        (void)posn;
    }
    // gta-reversed Fx.h: void TriggerFootSplash(const CVector& posn).
    // TODO(port): real implementation. Added 2026-10-09 for CPed.
    void TriggerFootSplash(const CVector& posn) {
        (void)posn;
    }
    // gta-reversed Fx.h: FxQuality_e GetFxQuality() const.
    // TODO(port): real implementation. Added 2026-10-09 for CPed.
    int32_t GetFxQuality() const { return 0; }
};

// gta-reversed Fx.h (0xA9AE00 in gta-reversed; verify address when the fx batch lands).
// TODO(port): verify StaticRef address.
extern Fx_c g_fx;
