// CHud - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Hud.h
// Decompiled bodies: src/CHud/*.c
// TODO: verify each method against decomp.

#pragma once

#include "RenderTypes.h" // CRGBA, GxtChar
#include "CSprite2d.h"   // CSprite2d by value in the Sprites array
#include "CCamera.h"     // eNameState (deduped 2026-10-09: CCamera.h also defines it)

#include <array>
#include <cstdint>

// ---- inlined from gta-reversed Enums/eHud.h (not yet converted) ----
// TODO: move these to eHud.h when the HUD enums are converted.
enum eMessageStyle : uint16_t {
    STYLE_MIDDLE,                // In The Middle
    STYLE_BOTTOM_RIGHT,          // At The Bottom Right
    STYLE_WHITE_MIDDLE,          // White Text In The Middle
    STYLE_MIDDLE_SMALLER,        // In The Middle Smaller
    STYLE_MIDDLE_SMALLER_HIGHER, // In The Middle Smaller A Bit Higher On The Screen
    STYLE_WHITE_MIDDLE_SMALLER,  // Small White Text In The Middle Of The Screen
    STYLE_LIGHT_BLUE_TOP,        // Light Blue Text On Top Of The Screen

    NUM_MESSAGE_STYLES
};

enum eHudItem : int16_t {
    ITEM_NONE   = -1,
    ITEM_ARMOUR =  3,
    ITEM_HEALTH =  4,
    ITEM_RADAR  =  8,
    ITEM_BREATH = 10,
};

enum DRAW_FADE_STATE {
    WANTED_STATE        = 0,
    ENERGY_LOST_STATE   = 1,
    DISPLAY_SCORE_STATE = 2,
    WEAPON_STATE        = 3,
};

// eNameState lives in CCamera.h (deduped 2026-10-09; identical values).
// TODO: move to eHud.h when the HUD enums are converted.

// ---- inlined from gta-reversed Stats.h (not yet converted) ----
// TODO: move to Stats.h when converted.
enum eStatUpdateState {
    STAT_UPDATE_DECREASE = 0,
    STAT_UPDATE_INCREASE = 1
};

class CPed;
class CPlayerInfo;
class CPlayerPed;

class CHud {
public:
    static constexpr auto BIG_MESSAGE_SIZE = 128;

    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CHud.cpp.
    // TODO: re-resolve for the clean-room build.
    static bool bScriptDontDisplayAreaName;      // 0xBAA3F8
    static bool bScriptDontDisplayVehicleName;   // 0xBAA3F9
    static bool bScriptForceDisplayWithCounters; // 0xBAA3FA
    static bool bScriptDontDisplayRadar;         // 0xBAA3FB

    static bool bDrawClock; // 0xBAA400

    static const GxtChar* m_pVehicleNameToPrint; // 0xBAA444
    static eNameState m_VehicleState;            // 0xBAA448
    static int32_t m_VehicleFadeTimer;           // 0xBAA44C
    static int32_t m_VehicleNameTimer;           // 0xBAA450
    static const GxtChar* m_pLastVehicleName;    // 0xBAA454
    static const GxtChar* m_pVehicleName;        // 0xBAA458

    static bool m_bDraw3dMarkers;    // 0xBAA45C
    static bool m_Wants_To_Draw_Hud; // 0xBAA45D

    static float m_fHelpMessageTime;            // 0xBAA460, in seconds
    static float m_fHelpMessageBoxWidth;        // 0x8D0934, default 200.0
    static bool m_bHelpMessagePermanent;        // 0xBAA464
    static float m_fHelpMessageStatUpdateValue; // 0xBAA468
    static uint16_t m_nHelpMessageMaxStatValue; // 0xBAA46C
    static uint16_t m_nHelpMessageStatId;       // 0xBAA470
    static bool m_bHelpMessageQuick;            // 0xBAA472
    static int32_t m_nHelpMessageState;         // 0xBAA474
    static uint32_t m_nHelpMessageFadeTimer;    // 0xBAA478
    static uint32_t m_nHelpMessageTimer;        // 0xBAA47C
    static GxtChar m_pHelpMessageToPrint[400];  // 0xBAA480
    static GxtChar m_pLastHelpMessage[400];     // 0xBAA610
    static GxtChar m_pHelpMessage[400];         // 0xBAA7A0

    static eNameState m_ZoneState;         // 0xBAA930
    static int32_t m_ZoneFadeTimer;        // 0xBAA934
    static uint32_t m_ZoneNameTimer;       // 0xBAA938
    static const GxtChar* m_ZoneToPrint;   // 0xBAB1D0
    static const GxtChar* m_pLastZoneName; // 0xBAB1D4
    static const GxtChar* m_pZoneName;     // 0xBAB1D8

    static eHudItem m_ItemToFlash;   // 0xBAB1DC
    static bool bDrawingVitalStats;  // 0xBAB1DE

    static int32_t m_LastBreathTime; // 0xBAA3FC

    static uint32_t m_WeaponState;     // 0xBAA404
    static uint32_t m_WeaponFadeTimer; // 0xBAA408
    static uint32_t m_WeaponTimer;     // 0xBAA40C
    static uint32_t m_LastWeapon;      // 0xBAA410

    static uint32_t m_WantedState;     // 0xBAA414
    static uint32_t m_WantedFadeTimer; // 0xBAA418
    static uint32_t m_WantedTimer;     // 0xBAA41C
    static uint32_t m_LastWanted;      // 0xBAA420

    static uint32_t m_DisplayScoreState;     // 0xBAA424
    static uint32_t m_DisplayScoreFadeTimer; // 0xBAA428
    static uint32_t m_DisplayScoreTimer;     // 0xBAA42C
    static uint32_t m_LastDisplayScore;      // 0xBAA430

    static uint32_t m_EnergyLostState;      // 0xBAA434
    static uint32_t m_EnergyLostFadeTimer;  // 0xBAA438
    static uint32_t m_EnergyLostTimer;
    static bool m_bDrawClock;      // 0xBAA43C
    static uint32_t m_LastTimeEnergyLost;   // 0xBAA440

    static GxtChar m_Message[400];                                      // 0xBAB040
    static GxtChar m_BigMessage[NUM_MESSAGE_STYLES][BIG_MESSAGE_SIZE];  // 0xBAACC0
    static GxtChar LastBigMessage[NUM_MESSAGE_STYLES][BIG_MESSAGE_SIZE];// 0xBAA940
    static float BigMessageAlpha[NUM_MESSAGE_STYLES];                   // 0xBAA3A4
    static float BigMessageInUse[NUM_MESSAGE_STYLES];                   // 0xBAA3C0
    static float BigMessageX[NUM_MESSAGE_STYLES];                       // 0xBAA3DC

    static std::array<CSprite2d, 6> Sprites; // 0xBAB1FC

    static int16_t TimerMainCounterHideState;            // 0xBAA388
    static bool TimerMainCounterWasDisplayed;             // 0xBAA38A
    static std::array<int16_t, 4> TimerCounterHideState;    // 0xBAA38C
    static std::array<int16_t, 4> TimerCounterWasDisplayed;   // 0xBAA394

    static float OddJob2OffTimer; // 0xBAA398
    static float OddJob2XOffset;  // 0xBAA39C
    static uint16_t OddJob2Timer; // 0xBAA3A0
    static uint16_t OddJob2On;    // 0xBAB1E0

    static float PagerXOffset;     // 0x8D0938, 150.0f
    static bool HelpTripSkipShown; // 0xBAB229

public:
    static void Initialise();
    static void ReInitialise();
    static void Shutdown();

    static void GetRidOfAllHudMessages(bool arg0);
    static float GetYPosBasedOnHealth(uint8_t playerId, float pos, int8_t offset);
    static bool HelpMessageDisplayed();

    static void SetMessage(const GxtChar* message);
    static void SetBigMessage(GxtChar* message, eMessageStyle style);
    static void SetHelpMessage(const GxtChar* text, bool quickMessage = false, bool permanent = false, bool addToBrief = false);
    static void SetHelpMessageStatUpdate(eStatUpdateState state, uint16_t statId, float diff, float max);
    static void SetHelpMessageWithNumber(const GxtChar* text, int32_t number, bool quickMessage, bool permanent);
    static void SetVehicleName(const GxtChar* name);
    static void SetZoneName(const GxtChar* name, bool displayImmediately);

    static void Draw();
    static void DrawAfterFade();
    static void DrawAreaName();
    static void DrawBustedWastedMessage();
    static void ResetWastedText();
    static void DrawCrossHairs();
    static float DrawFadeState(DRAW_FADE_STATE fadeState, int32_t arg1);
    static void DrawHelpText();
    static void DrawMissionTimers();
    static void DrawMissionTitle();
    static void DrawOddJobMessage(bool displayImmediately);
    static void DrawRadar();
    static void DrawScriptText(bool displayImmediately);
    static void DrawSubtitles();
    static void DrawSuccessFailedMessage();
    static void DrawVehicleName();
    static void DrawVitalStats();
    static void DrawAmmo(CPed* ped, int32_t x, int32_t y, float alpha);
    static void DrawPlayerInfo();
    // NOTE: gta-reversed declared these three `static inline`; the inline is
    // dropped here (definitions live in CHud.cpp) - signatures unchanged.
    static void DrawClock();
    static void DrawMoney(const CPlayerInfo& playerInfo, uint8_t alpha);
    static void DrawWeapon(CPlayerPed* ped0, CPlayerPed* ped1);

    static void DrawTripSkip();
    static void DrawWanted();
    static void DrawWeaponIcon(CPed* ped, int32_t x, int32_t y, float alpha);
    static void RenderArmorBar(int32_t playerId, int32_t x, int32_t y);
    static void RenderBreathBar(int32_t playerId, int32_t x, int32_t y);
    static void RenderHealthBar(int32_t playerId, int32_t x, int32_t y);
};
