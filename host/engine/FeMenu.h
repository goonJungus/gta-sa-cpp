#pragma once
// FeMenu - the SA pause menu (port of CMenuManager's standard menus).
//
// Screens come from the retail aScreens table (FeScreens.h) and their text
// from american.gxt, drawn the way CMenuManager::DrawBackground /
// DrawStandardMenus / DisplaySlider do: black backdrop + fronten2 artwork,
// gothic title, menu-font entries with the fronten1 arrow on the selected
// one, values right-aligned, the fronten_pc mouse pointer.
//
// Ported so far: Resume, Options > Display Setup > Advanced (draw distance
// slider, frame limiter 60/120/144/off, plus a VSync entry that retail does
// not have) and Quit Game. Every other retail screen is reachable and shows
// its entries greyed out (retail's own "unavailable" colour) until ported.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Fe2d.h"
#include "FeFont.h"
#include "FeText.h"

class D3DRenderer;

struct FeSettings {
    int frameLimit = 0;       // 0 = off (unlimited), else frames per second
    bool vsync = false;
    float drawDist = 1.1f;    // CMenuManager::m_fDrawDistance, 0.925 .. 1.8
    // Gameplay style: 0 = classic (retail), 1 = rewrite (momentum handling /
    // modern shoulder cameras).
    int movementStyle = 0;
    int cameraStyle = 0;
    // Display and HUD.
    float fov = 70.0f;        // vertical degrees (rewrite only; retail is fixed)
    float hudScale = 1.0f;
    bool showHud = true;
    int radarMode = 0;        // 0 = radar, 1 = off
    // Audio.
    float masterVol = 0.8f, sfxVol = 1.0f, musicVol = 0.7f;
    // Rewrite extras.
    bool hitMarker = true;
    bool damageFlash = true;
    bool crosshairDot = true;
    bool autoReload = true;
};

class FeMenu {
public:
    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown();

    bool IsOpen() const { return m_open; }
    void Open(int screen = kPauseScreen);
    void Close();

    // WM_KEYDOWN virtual keys (auto-repeat included).
    void OnKey(int vk);
    // Menu sound cue since the last call: 0 none, 1 highlight, 2 select, 3 back.
    int TakeUiEvent() { int e = m_uiEvent; m_uiEvent = 0; return e; }
    // Mouse in backbuffer pixels; `click` = left button went down this frame,
    // `held` = it is down.
    void OnMouse(float x, float y, bool click, bool held);

    void Draw(D3DRenderer& r);

    FeSettings& Settings() { return m_set; }
    bool TakeSettingsChanged() { bool c = m_changed; m_changed = false; return c; }
    bool TakeQuit() { bool q = m_quit; m_quit = false; return q; }

    // Dev-key list shown beside the root pause screen: {key, what (state)}.
    std::vector<std::pair<std::string, std::string>> devKeys;

    static const int kPauseScreen = 41;

private:
    enum : uint8_t {
        A_TEXT = 1, A_BACK = 2, A_GOTO = 5, A_FRAME_LIMITER = 24, A_YES_QUIT = 53,
        A_RESUME = 54, A_NO = 55, A_DRAW_DIST = 61,
        A_VSYNC = 200,   // ours: not in retail
        A_TOGGLE = 210, A_CHOICE = 211, A_SLIDER = 212, A_REWRITE = 213,
    };
    struct Entry {
        uint8_t action = 0;
        std::string name;
        int type = 0, target = 0, x = 0, y = 0, align = 0;
        bool live = false;               // ported: selectable
        bool needRewrite = false;        // greyed out unless rewrite mode is on
        bool* b = nullptr; int* i = nullptr; float* f = nullptr;
        float lo = 0, hi = 1, step = 0.05f;
        std::vector<std::string> choices;
        float hx0 = 0, hy0 = 0, hx1 = 0, hy1 = 0;   // last drawn hit box
    };
    struct Screen {
        std::string title;
        int prev = -1, start = 0;
        std::vector<Entry> items;        // all 12 slots (retail indices)
    };

    void BuildScreens();
    void SwitchTo(int screen, int fromChild = -1);
    void Back();
    void Activate(int dir);              // dir 0 = Enter/click, -1/+1 = left/right
    void Move(int dir);
    bool Live(const Entry& e) const { return e.live && (!e.needRewrite || m_set.movementStyle == 1); }
    void AddScreen(const char* title, int prev, std::vector<Entry> items);
    Entry MakeToggle(const char* name, bool* b, bool rewrite);
    Entry MakeChoice(const char* name, int* i, std::vector<std::string> ch, bool rewrite);
    Entry MakeSlider(const char* name, float* f, float lo, float hi, float step, bool rewrite);
    Entry MakeGoto(const char* name, int target);
    bool Visible(const Entry& e) const { return e.action != 0 && e.action != A_TEXT && !e.name.empty(); }
    std::string ValueText(const Entry& e) const;
    void DrawSlider(Fe2d& g, float value, float y125);
    void DrawDevPanel(Fe2d& g);
    float Sx(float v) const { return v * m_w / 640.0f; }
    float Sy(float v) const { return v * m_h / 448.0f; }

    std::vector<Screen> m_screens;
    int m_screen = kPauseScreen, m_sel = 0;
    bool m_open = false, m_changed = false, m_quit = false;
    FeSettings m_set;
    float m_w = 640, m_h = 448, m_ox = 0, m_oy = 0;
    float m_mx = -1, m_my = -1;
    int m_uiEvent = 0;
    bool m_mouseMoved = false, m_mouseClick = false, m_mouseHeld = false;

    Fe2d m_2d;
    FeFont m_font;
    FeText m_text;
    D3DRenderer* m_r = nullptr;
    IDirect3DTexture9* m_arrow = nullptr;
    IDirect3DTexture9* m_mouse = nullptr;
    IDirect3DTexture9* m_back[7] = {};   // back2 .. back8
};
