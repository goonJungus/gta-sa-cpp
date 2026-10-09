// CMenuManager - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/MenuManager.h
// Decompiled bodies: src/CMenuManager/*.c
// TODO: verify each method against decomp.

#pragma once

#include "CPad.h" // for RsKeyCodes
#include "CPedModelInfo.h" // for eRadioID

// eHelperText: helper text message IDs (minimal; full enum TODO)
enum eHelperText : int32_t {
    HELPER_TEXT_NONE = 0,
};

#include "RenderTypes.h" // CRGBA, RwRaster
#include "CSprite2d.h"   // CSprite2d by value in m_aFrontEndSprites
#include "CVector.h"     // CVector2D
#include "CRect.h"       // CRect (DrawWindow takes const CRect&)

#include <cstdint>

#include "CFont.h" // eFontAlignment (was inlined here; deduped 2026-10-09)

// ---- inlined from gta-reversed Frontend/MenuManager_Internal.h (not yet converted) ----
// TODO: move these to MenuManager_Internal.h when the frontend internals are converted.
enum eMenuScreen : int8_t {
    SCREEN_GO_BACK                         = -2,
    SCREEN_NONE                            = -1,
    SCREEN_STATS                           =  0,
    SCREEN_NOP                             =  0,
    SCREEN_START_GAME,                     //  1  New Game, Load Game, Delete Game
    SCREEN_BRIEF,                          //  2
    SCREEN_AUDIO_SETTINGS,                 //  3
    SCREEN_DISPLAY_SETTINGS,               //  4
    SCREEN_MAP,                            //  5
    SCREEN_NEW_GAME_ASK,                   //  6
    SCREEN_SELECT_GAME,                    //  7
    SCREEN_MISSION_PACK_LOADING_ASK,       //  8
    SCREEN_LOAD_GAME,                      //  9
    SCREEN_DELETE_GAME,                    // 10
    SCREEN_LOAD_GAME_ASK,                  // 11
    SCREEN_DELETE_GAME_ASK,                // 12
    SCREEN_LOAD_FIRST_SAVE,                // 13
    SCREEN_DELETE_FINISHED,                // 14
    SCREEN_DELETE_SUCCESSFUL,              // 15
    SCREEN_GAME_SAVE,                      // 16
    SCREEN_SAVE_WRITE_ASK,                 // 17
    SCREEN_SAVE_DONE_1,                    // 18
    SCREEN_SAVE_DONE_2,                    // 19
    SCREEN_GAME_SAVED,                     // 20
    SCREEN_GAME_LOADED,                    // 21
    SCREEN_GAME_WARNING_DONT_SAVE,         // 22
    SCREEN_ASK_DISPLAY_DEFAULT_SETS,       // 23
    SCREEN_ASK_AUDIO_DEFAULT_SETS,         // 24
    SCREEN_CONTROLS_RESET,                 // 25
    SCREEN_USER_TRACKS_OPTIONS,            // 26
    SCREEN_DISPLAY_ADVANCED,               // 27
    SCREEN_LANGUAGE,                       // 28
    SCREEN_SAVE_GAME_DONE,                 // 29
    SCREEN_SAVE_GAME_FAILED,               // 30
    SCREEN_SAVE_WRITE_FAILED,              // 31
    SCREEN_SAVE_FAILED_FILE_ERROR,         // 32
    SCREEN_OPTIONS,                        // 33
    SCREEN_MAIN_MENU,                      // 34
    SCREEN_QUIT_GAME_ASK,                  // 35
    SCREEN_CONTROLLER_SETUP,               // 36
    SCREEN_REDEFINE_CONTROLS,              // 37
    SCREEN_CONTROLS_DEFINITION,            // 38
    SCREEN_MOUSE_SETTINGS,                 // 39
    SCREEN_JOYPAD_SETTINGS,                // 40
    SCREEN_PAUSE_MENU,                     // 41
    SCREEN_INITIAL,                        // 42
    SCREEN_EMPTY,                          // 43

    SCREEN_COUNT,                          // Screen count
};

enum eFrontend : int8_t {
    FRONTEND1_START       = 0,
    FRONTEND2_START       = 13,
    FRONTEND3_START       = 21,
    FRONTEND4_START       = 23, // PC
    FRONTEND_SPRITE_COUNT = 25,

    // FRONTEND 1 - Radio
    FRONTEND_SPRITE_ARROW        = FRONTEND1_START,
    FRONTEND_SPRITE_PLAYBACK,
    FRONTEND_SPRITE_KROSE,
    FRONTEND_SPRITE_KDST,
    FRONTEND_SPRITE_BOUNCE,
    FRONTEND_SPRITE_SFUR,
    FRONTEND_SPRITE_RLS,
    FRONTEND_SPRITE_RADIOX,
    FRONTEND_SPRITE_CSR,
    FRONTEND_SPRITE_KJAH,
    FRONTEND_SPRITE_MASTER_SOUNDS,
    FRONTEND_SPRITE_WCTR,
    FRONTEND_SPRITE_TPLAYER,

    // FRONTEND 2 - Background
    FRONTEND_SPRITE_BACK2        = FRONTEND2_START,
    FRONTEND_SPRITE_BACK3,
    FRONTEND_SPRITE_BACK4,
    FRONTEND_SPRITE_BACK5,
    FRONTEND_SPRITE_BACK6,
    FRONTEND_SPRITE_BACK7,
    FRONTEND_SPRITE_BACK8,
    FRONTEND_SPRITE_MAP,

    // FRONTEND 3 - AdditionalBackground
    FRONTEND_SPRITE_BACK8_TOP    = FRONTEND3_START,
    FRONTEND_SPRITE_BACK8_RIGHT,

    // FRONTEND PC - Mouse
    FRONTEND_SPRITE_MOUSE        = FRONTEND4_START,
    FRONTEND_SPRITE_CROSS_HAIR,
};

// ---- inlined from gta-reversed Enums/eLanguage.h (not yet converted) ----
// TODO: move to eLanguage.h when converted.
enum class eLanguage : uint8_t {
    // ENGLISH
    AMERICAN = 0,
    FRENCH   = 1,
    GERMAN   = 2,
    ITALIAN  = 3,
    SPANISH  = 4,
    RUSSIAN  = 5, // mobile
    JAPANESE = 6, // mobile
};

// ---- inlined from gta-reversed Enums/eRadioID.h (not yet converted) ----
// TODO: move to eRadioID.h when converted. (The NOTSA EnumToString helper is skipped.)
// eRadioID from CPedModelInfo.h



// ---- inlined from gta-reversed Enums/eControllerType.h (not yet converted) ----
// TODO: move to eControllerType.h when converted.
enum class eControllerType {
    KEYBOARD,
    OPTIONAL_EXTRA_KEY,
    MOUSE,
    JOY_STICK,

    CONTROLLER_TYPES_COUNT
};

// RsKeyCodes lives in the RenderWare/input layer (gta-reversed
// RenderWare/rw/skeleton.h) - far too large to inline here. Opaque declaration
// with the fixed underlying type; pull the real enum when the input subsystem
// is converted.


enum eRadarMode : int32_t {
    RADAR_MODE_MAPS_AND_BLIPS = 0,
    RADAR_MODE_BLIPS_ONLY     = 1,
    RADAR_MODE_OFF            = 2,

    RADAR_MODE_COUNT
};

enum eRadioMode : int8_t {
    RADIO_MODE_RADIO      = 0,
    RADIO_MODE_RANDOM     = 1,
    RADIO_MODE_SEQUENTIAL = 2,

    RADIO_MODE_COUNT
};

struct MPack {
    uint8_t m_Id;
    char  m_Name[260];
};

enum class eController : int8_t {
    MOUSE_PLUS_KEYS = 0,
    JOYPAD          = 1
};

enum class eControlMode : uint8_t {
    FOOT    = 0,
    VEHICLE = 1
};

enum class eControllerError : int8_t {
    NONE     = 0,
    VEHICLE  = 1,
    FOOT     = 2,
    NOT_SETS = 3
};

enum class eMouseInBounds {
    /*
    LEFT             = -1,
    NONE_SIDE        = 0,
    RIGNT            = 1,
    */

    MENU_ITEM        = 2,
    BACK_BUTTON      = 3,
    ENTER_MENU       = 4,
    SELECT           = 5,
    SLIDER_RIGHT     = 6,
    SLIDER_LEFT      = 7,
    DRAW_DIST_RIGHT  = 8,
    DRAW_DIST_LEFT   = 9,
    RADIO_VOL_RIGHT  = 10,
    RADIO_VOL_LEFT   = 11,
    SFX_VOL_RIGHT    = 12,
    SFX_VOL_LEFT     = 13,
    MOUSE_SENS_RIGHT = 14,
    MOUSE_SENS_LEFT  = 15,
    NONE             = 16
};

constexpr auto FRONTEND_MAP_RANGE_MIN = 300.0f;
constexpr auto FRONTEND_MAP_RANGE_MAX = 1100.0f;

class CMenuManager {
    enum {
        MPACK_COUNT  = 25,
        SPRITE_COUNT = 25,
    };

public:
    static constexpr uint32_t SETTINGS_FILE_VERSION = 6u;

    int8_t    m_nStatsScrollDirection;
    float     m_fStatsScrollSpeed;
    uint8_t   m_nSelectedRow; // CMenuSystem
    int32_t   m_CurrentGallerySelected; // unused
    int32_t   m_CurrentMaxGalleryImages; // unused
    int32_t   m_CurrentMaxRealPhotos; // unused
    int32_t   m_CurrentSelectedRealPhoto; // unused
    uint32_t  m_ReloadImageTime; // unused
    bool      m_PrefsUseVibration;
    bool      m_bHudOn;
    eRadarMode m_nRadarMode;
    int32_t   field_28;
    int32_t   m_nTargetBlipIndex; // blip script handle
    int8_t    m_nSysMenu; // CMenuSystem
    bool      m_DisplayControllerOnFoot;
    bool      m_bDontDrawFrontEnd;
    bool      m_bActivateMenuNextFrame;
    bool      m_bMenuAccessWidescreen;
    bool      field_35;
    RsKeyCodes m_KeyPressedCode;
    int32_t   m_PrefsBrightness;
    float     m_fDrawDistance;

    bool      m_bShowSubtitles;
    union {
        struct {
            bool m_ShowLocationsBlips;
            bool m_ShowContactsBlips;
            bool m_ShowMissionBlips;
            bool m_ShowOtherBlips;
            bool m_ShowGangAreaBlips;
        };
        bool m_abPrefsMapBlips[5];
    };
    bool      m_bMapLegend;
    bool      m_bWidescreenOn;
    bool      m_bPrefsFrameLimiter;
    bool      m_bRadioAutoSelect;
    bool      m_PrefsAudioOutputMode;
    int8_t    m_nSfxVolume;
    int8_t    m_nRadioVolume;
    bool      m_bRadioEq;

    eRadioID  m_nRadioStation;
    bool      m_RecheckNumPhotos; // unused
    int32_t   m_nCurrentScreenItem; // CurrentOption
    bool      m_bQuitGameNoDVD; // CMenuManager::WaitForUserCD 0x57C5E0

    bool      m_bDrawingMap;
    bool      m_bStreamingDisabled;
    bool      m_bAllStreamingStuffLoaded;

    bool      m_bMenuActive;
    bool      m_bStartGameLoading;
    int8_t    m_nGameState;
    bool      m_bIsSaveDone;
    bool      m_bLoadingData;
    float     m_fMapZoom;
    CVector2D m_vMapOrigin;
    CVector2D m_vMousePos;  // Red marker position (world coordinates)
    bool      m_bMapLoaded;

    int32_t   m_nTitleLanguage; // Value is PRIMARYLANGID(GetSystemDefaultLCID())
    int32_t   m_nTextLanguage; // TODO: Change to `eLanguage`
    eLanguage m_nPrefsLanguage;
    eLanguage m_nPreviousLanguage;
    int32_t   m_SystemLanguage;
    bool      m_LoadedLanguage;
    int32_t   m_ListSelection;      // controller related
    int32_t   m_RenderScreenOnce;   // unused
    uint8_t*  m_GalleryImgBuffer;   //!< +0x98  \see JPegCompress file
    RwRaster* m_GpJpgTex; // unused
    bool      m_StartUpFrontEndRequestedForPads; // unused
    int32_t   m_ScreenXOffset; // unused
    int32_t   m_ScreenYOffset; // unused
    uint32_t  m_UserTrackIndex;
    eRadioMode m_RadioMode;

    bool      m_bInvertPadX1;
    bool      m_bInvertPadY1;
    bool      m_bInvertPadX2;
    bool      m_bInvertPadY2;
    bool      m_bSwapPadAxis1;
    bool      m_bSwapPadAxis2;

    eControlMode m_RedefiningControls;
    bool      m_DisplayTheMouse; // m_bMouseMoved
    int32_t   m_nMousePosX;
    int32_t   m_nMousePosY;
    bool      m_bPrefsMipMapping;
    bool      m_bTracksAutoScan;
    int32_t   m_nPrefsAntialiasing;
    int32_t   m_nDisplayAntialiasing;
    eController m_ControlMethod;
    int32_t   m_nPrefsVideoMode;
    int32_t   m_nDisplayVideoMode;
    int32_t   m_nCurrentRwSubsystem; // initialized | not used

    int32_t   m_nMousePosWinX; // xPos = GET_X_LPARAM(lParam); 0x748323
    int32_t   m_nMousePosWinY; // yPos = GET_Y_LPARAM(lParam);

    bool      m_bSavePhotos;
    bool      m_bMainMenuSwitch;
    uint8_t   m_nPlayerNumber;
    bool      m_bLanguageChanged; // useless?
    int32_t   field_EC;
    RsKeyCodes* m_pPressedKey; // any pressed key, in order of CKeyboardState; rsNULL means no key pressed
    bool      m_isPreInitialised;

    union {
        struct {
            CSprite2d m_apRadioSprites[FRONTEND2_START];
            CSprite2d m_apBackgroundTextures[FRONTEND3_START - FRONTEND2_START];
            CSprite2d m_apAdditionalBackgroundTextures[FRONTEND4_START - FRONTEND3_START];
            CSprite2d m_apMouseTextures[FRONTEND_SPRITE_COUNT - FRONTEND4_START];
        };
        CSprite2d m_aFrontEndSprites[FRONTEND_SPRITE_COUNT];
    };

    bool        m_bTexturesLoaded;
    eMenuScreen m_nCurrentScreen;
    eMenuScreen m_nPrevScreen; // Used only in SwitchToNewScreen
    uint8_t     m_SelectedSlot;
    uint8_t     m_nMissionPackGameId;
    MPack       m_MissionPacks[MPACK_COUNT];
    bool        m_bDoVideoModeUpdate;
    RsKeyCodes  m_nPressedMouseButton; // used in redefine controls
    int32_t     m_nJustDownJoyButton;  // used in redefine controls; set via CControllerConfigManager::GetJoyButtonJustDown
    bool        m_MenuIsAbleToQuit;
    bool        m_RadioAvailable;
    eControllerError m_ControllerError;
    bool        m_bScanningUserTracks;
    int32_t     m_nHelperTextFadingAlpha;
    bool        m_KeyPressed[5];
    int32_t     m_nOldMousePosX;
    int32_t     m_nOldMousePosY;
    eMouseInBounds m_MouseInBounds;
    int32_t     m_CurrentMouseOption;
    bool        m_bJustOpenedControlRedefWindow;
    bool        m_EditingControlOptions;
    bool        m_DeleteAllBoundControls;
    bool        m_DeleteAllNextDefine;
    int32_t     m_OptionToChange;
    int32_t     m_OptionProcessing; // unused
    bool        m_CanBeDefined;
    bool        m_JustExitedRedefine;
    eHelperText m_HelperText;
    uint32_t    m_TimeToStopPadShaking; // useless

    bool        m_TexturesSwapped;
    uint8_t     m_nNumberOfMenuOptions;
    uint32_t    m_StatsScrollTime;
    bool        m_bViewRadar;
    uint32_t    m_RadarVisibilityChangeTime;
    uint32_t    m_BriefsArrowBlinkTime;
    uint16_t    m_StatusDisablePlayerControls;
    uint32_t    m_LastActionTime;
    bool        m_CurrentlyLoading;
    bool        m_CurrentlyDeleting;
    bool        m_CurrentlySaving; // mpack related
    uint32_t    m_UserTrackScanningTime;
    bool        m_ErrorPendingReset;
    uint32_t    m_ErrorStartTime;

    union {
        struct {
            bool bWereError : 1;
            bool bScanningUserTracks : 1;
        };
        int32_t field_1B4C;
    };

    eFrontend m_BackgroundSprite;
    bool      m_InputWaitBlink;
    uint32_t  m_LastBlinkTime;
    uint32_t  m_HelperTextUpdatedTime;
    bool      m_OptionFlashColorState;
    uint32_t  m_LastHighlightToggleTime;
    uint32_t  m_LastTransitionTime;
    uint32_t  m_SlideLeftMoveTime;
    uint32_t  m_SlideRightMoveTime;
    int32_t   m_nMouseHoverScreen;

    union {
        struct {
            bool bMouseHoverInitialised : 1;
        };
        int32_t field_1B74; // ???
    };

    // NOTE: gta-reversed declared this `static int32&` (bound to a fixed
    // address via StaticRef). It was never defined in gta-reversed, so it is a
    // plain static here; definition in CMenuManager.cpp.
    // TODO: re-resolve for the clean-room build.
    static int32_t nLastMenuPage;

    static bool bInvertMouseX; // 0xBA6744
    static bool bInvertMouseY; // 0xBA6745

    // notsa colors
    // NOTE: parens -> braces: CRGBA is an aggregate, paren-init is C++20-only.
    static constexpr CRGBA MENU_BG              = CRGBA{0, 0, 0, 255};       // Black background
    static constexpr CRGBA MENU_BUILD_INFO_TEXT = CRGBA{255, 255, 255, 100};
    static constexpr CRGBA MENU_CONTROLLER_BG   = CRGBA{49, 101, 148, 100};
    static constexpr CRGBA MENU_CURSOR_SHADOW   = CRGBA{100, 100, 100, 50};
    static constexpr CRGBA MENU_ERROR           = CRGBA{200, 50, 50, 255};   // Error/Warning color
    static constexpr CRGBA MENU_MAP_BACKGROUND  = CRGBA{111, 137, 170, 255}; // Map background
    static constexpr CRGBA MENU_MAP_BORDER      = CRGBA{100, 100, 100, 255}; // Map border
    static constexpr CRGBA MENU_MAP_FOG         = CRGBA{111, 137, 170, 200};
    static constexpr CRGBA MENU_PROGRESS_BG     = CRGBA{50, 50, 50, 255};
    static constexpr CRGBA MENU_SHADOW          = CRGBA{0, 0, 0, 200};       // Semi-transparent shadow
    static constexpr CRGBA MENU_TEXT_DISABLED   = CRGBA{14, 30, 47, 255};
    static constexpr CRGBA MENU_TEXT_INACTIVE   = CRGBA{255, 255, 255, 30};
    static constexpr CRGBA MENU_TEXT_LIGHT_GRAY = CRGBA{225, 225, 225, 255}; // Light gray text
    static constexpr CRGBA MENU_TEXT_NORMAL     = CRGBA{74, 90, 107, 255};   // Plain text (not selected)
    static constexpr CRGBA MENU_TEXT_SELECTED   = CRGBA{172, 203, 241, 255}; // Highlighted text
    static constexpr CRGBA MENU_TEXT_WHITE      = CRGBA{255, 255, 255, 255}; // White text

public:
    CMenuManager();
    ~CMenuManager();
    CMenuManager* Constructor();
    CMenuManager* Destructor();

    void Initialise();

    void LoadAllTextures();
    void SwapTexturesRound(bool slot);
    void UnloadTextures();

    void InitialiseChangedLanguageSettings(bool reinitControls);
    bool HasLanguageChanged();

    void DoSettingsBeforeStartingAGame();
    float StretchX(float x);
    float StretchY(float y);
    void SwitchToNewScreen(eMenuScreen screen);
    void ScrollRadioStations(int8_t numStations);
    void SetFrontEndRenderStates();
    void SetDefaultPreferences(eMenuScreen screen);
    uint32_t GetNumberOfMenuOptions();

    void JumpToGenericMessageScreen(eMenuScreen screen, const char* titleKey, const char* textKey);

    void DrawFrontEnd();
    void DrawBuildInfo();
    void DrawBackground();
    void DrawStandardMenus(bool);
    void DrawWindow(const CRect& coords, const char* key, uint8_t color, CRGBA backColor, bool unused, bool background);
    void DrawWindowedText(float x, float y, float wrap, const char* title, const char* message, eFontAlignment alignment);
    void DrawQuitGameScreen();
    void DrawControllerScreenExtraText(int32_t);
    void DrawControllerBound(uint16_t verticalOffset, bool isOppositeScreen);
    void DrawControllerSetupScreen();
#ifdef USE_GALLERY
    void DrawGallery();
    void DrawGallerySaveMenu();
#endif

    void CentreMousePointer();

    void LoadSettings();
    void SaveSettings();
    void SaveStatsToFile();
    void SaveLoadFileError_SetUpErrorScreen();

    void CheckSliderMovement(int32_t value);
    [[nodiscard]] bool CheckFrontEndUpInput() const;
    [[nodiscard]] bool CheckFrontEndDownInput() const;
    [[nodiscard]] bool CheckFrontEndLeftInput() const;
    [[nodiscard]] bool CheckFrontEndRightInput() const;
    void CheckForMenuClosing();
    [[nodiscard]] bool CheckHover(float left, float right, float top, float bottom) const;
    bool CheckMissionPackValidMenu();
    void CheckCodesForControls(eControllerType type);

    int32_t DisplaySlider(float x, float y, float h1, float h2, float length, float value, int32_t spacing);

    void DisplayHelperText(const char* key);
    void SetHelperText(eHelperText messageId);
    void ResetHelperText();
    void NoDiskInDriveMessage();

    void MessageScreen(const char* key, bool blackBackground, bool cameraUpdateStarted);
    void SmallMessageScreen(const char* key);

    void CalculateMapLimits(float& bottom, float& top, float& left, float& right);

    void PlaceRedMarker();
    void RadarZoomIn();

    void PrintMap();
    void PrintStats();
    void PrintBriefs();
    void PrintRadioStationList();

    void UserInput();
    void AdditionalOptionInput(bool* upPressed, bool* downPressed);
    bool CheckRedefineControlInput();
    void RedefineScreenUserInput(bool* accept, bool* cancel);

    void Process();
    void ProcessStreaming(bool streamAll);
    void ProcessFileActions();
    void ProcessUserInput(bool GoDownMenu, bool GoUpMenu, bool EnterMenuOption, bool GoBackOneMenu, int8_t LeftRight);
    void ProcessMenuOptions(int8_t pressedLR, bool& cancelPressed, bool acceptPressed);
    bool ProcessPCMenuOptions(int8_t pressedLR, bool acceptPressed);
    void ProcessMissionPackNewGame();

    // NOTSA
    const char* GetMovieFileName() const {
        switch (m_nTitleLanguage) {
        case 12:
        case 7:
            return "movies\\GTAtitlesGER.mpg";
        }
        return "movies\\GTAtitles.mpg";
    }
    uint32_t GetMaxAction();
    uint32_t GetVerticalSpacing();

    //! Simulate that we came into the menu and clicked to load game
    //! @param newGame If we should start a new game
    //! @param slot    Slot of the save-game to load (Ignored if `newGame`)
    void SimulateGameLoad(bool newGame, uint32_t slot);
private:
    static void SetBrightness(float brightness, bool arg2);
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CMenuManager) == 0x1B78, "CMenuManager layout changed");
#endif

// Clean-room: gta-reversed bound this to 0xBA6748 via StaticRef<CMenuManager>.
// Backed by a plain static instance in CMenuManager.cpp.
// TODO: re-resolve for the clean-room build.
extern CMenuManager& FrontEndMenuManager;
