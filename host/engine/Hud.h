#pragma once
// Hud - the in-game heads-up display and radar (port of CHud / CRadar).
//
// Classic style follows retail: CHud::DrawPlayerInfo places the clock, weapon
// icon, ammo, armour / breath / health bars, money and wanted stars in 640x448
// design units anchored to the right of the screen; the radar sits bottom-left.
// Units scale with the screen height only, so any aspect ratio (4:3 up to
// ultrawide) keeps the same proportions with the HUD hugging the edges.
//
// Rewrite style is the modern layout: rounded radar with health and armour
// bars beneath it, cash / wanted / clock / weapon at the top right, a dot
// crosshair, hit markers and damage vignettes.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Fe2d.h"
#include "FeFont.h"
#include "FeText.h"
#include "ImgLoader.h"

class D3DRenderer;

// Text and bars anchored in the world or on the screen (shooting range, floating numbers).
struct HudLabel {
    float x = 0, y = 0;            // screen position, 0..1 of the width / height
    std::string text;
    uint32_t color = 0xFFFFFFFF;
    float scale = 1.0f;
    float bar = -1.0f;             // 0..100 draws a bar under the text, < 0 none
    bool left = false;             // left aligned instead of centred
};

// One slot of the weapon wheel; the fists are slot 0.
struct WheelItem {
    std::string name;
    IDirect3DTexture9* icon = nullptr;
    int clip = 0, total = 0, clipSize = 0;
    bool melee = false;
    int cat = 0;                   // wheel category 0..7 (see kWheelCats)
};

// A radar blip (CRadar::ms_RadarTrace): a sprite blip from the scripts, drawn on the radar (a
// short-range one only while it is in range, others pinned to the rim) and on the pause map.
struct RadarBlip {
    int handle = 0;
    float x = 0, y = 0, z = 0;
    int sprite = 0;               // CRadar::RadarBlipFileNames index (0 = a plain coloured marker)
    bool shortRange = false;
    int display = 2;              // CHANGE_BLIP_DISPLAY: 0 none, 1 marker only, 2 blip only, 3 both
    uint32_t colour = 0xFFE10000;
};
// CRadar::RadarBlipFileNames @ 008d0720: the sprite of each blip id.
const char* RadarBlipSpriteName(int sprite);

struct HudData {
    float dt = 0.016f;
    bool rewrite = false;
    float scale = 1.0f;            // rewrite: HUD scale setting
    bool showHud = true;
    bool radarOn = true;
    bool crosshairDot = true, hitMarker = true, damageFlash = true;

    float health = 100, maxHealth = 100, armor = 0;
    float breath = 1.0f;           // 0..1
    float stamina = 1.0f;          // 0..1, shown when it is not full
    bool showBreath = false;
    float damageFlashAmount = 0.0f;
    int money = 0;
    int hour = 12, minute = 0;
    int wanted = 0;
    // Weapon
    IDirect3DTexture9* weaponIcon = nullptr;   // null = fists
    bool armed = false;
    bool hideAmmo = false;
    int ammoClip = 0, ammoTotal = 0, clipSize = 0;
    bool reloading = false;
    const char* weaponName = "";
    bool aiming = false;
    float crossX = 0.5f, crossY = 0.5f;
    float crossRadius = 0.2f;              // CPlayerPed::GetWeaponRadiusOnScreen
    bool lockOn = false;                   // a locked-on target: CWeaponEffects marker instead of the crosshair
    float lockX = 0, lockY = 0;            // its screen position, pixels
    float lockSize = 1.0f;                 // 1 - distance * 0.02
    float lockHealth = 1.0f;               // 0..1: red .. green   // crosshair, fraction of the screen (CCamera::m_f3rdPersonCHairMultX / Y)
    float spread = 0.0f;           // 0..1 crosshair widening (moving, firing)
    bool firstPerson = false;
    // Radar and map
    float x = 0, y = 0, z = 0;
    float camYaw = 0;              // radians, world yaw the camera looks along
    float heading = 0;             // radians, world yaw the player faces
    std::string zone;              // GXT text of the area name; empty = none
    std::vector<HudLabel> labels;
    bool wheel = false;            // weapon wheel up
    std::vector<WheelItem> wheelItems;
    int wheelSel = 0;              // highlighted item (index into wheelItems)
    int wheelCat = 0;              // highlighted category
    int wheelPick[8] = {-1, -1, -1, -1, -1, -1, -1, -1};   // the weapon shown in each category (item index, -1 empty)
    std::vector<RadarBlip> blips;  // script blips
    bool hasWaypoint = false;      // map waypoint blip
    float wpX = 0, wpY = 0;
};

// What the mission script puts on screen on top of the game: fade, cinematic bars, subtitles and its text commands.
struct ScriptTextItem {
    float x = 0, y = 0;                // 640 x 448 design units
    float sx = 0.48f, sy = 1.12f;      // SET_TEXT_SCALE
    uint32_t colour = 0xFFE1E1E1;      // ARGB
    bool centre = false, right = false, justify = false;
    float wrap = 640.0f;               // SET_TEXT_WRAPX
    int font = 1;                      // SET_TEXT_FONT
    int shadow = 0;                    // SET_TEXT_DROPSHADOW size, 0 none
    uint32_t shadowColour = 0xFF000000;
    bool beforeFade = false;           // SET_TEXT_DRAW_BEFORE_FADE
    std::string text;
};
struct ScriptOverlay {
    float fade = 0.0f;                 // 0 none .. 1 solid
    uint32_t fadeRgb = 0;
    float wide = 0.0f;                 // cinematic bars, 0 off .. 1 fully in
    std::string subtitle;              // PRINT_NOW / cutscene text
    std::vector<ScriptTextItem> text;
};

class Hud {
public:
    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown();
    bool Ready() const { return m_ready; }
    // Swap the text table (GXT file name, e.g. "spanish.gxt").
    void SetLanguage(const std::string& gameDir, const char* file) { m_text.Load(gameDir, file); }

    // The in-game HUD (call after the world, before the pause menu).
    void Draw(D3DRenderer& r, const HudData& d);

    // Display > Brightness: a flat black or white wash over the whole frame (0.5 = none).
    void Brightness(D3DRenderer& r, float level);
    // CPostEffects night vision (44, green) / infrared (45, blue-orange) tint over the frame.
    void Goggles(D3DRenderer& r, int mode);
    // A flat white wash that fades out: the camera flash (a = 0..1).
    void Flash(D3DRenderer& r, float a);
    // The sniper scope (black surround, circle, cross hairs) or the camera viewfinder (corner brackets).
    void Scope(D3DRenderer& r, bool camera, bool rocket = false);

    // A hit confirmed on a target (rewrite hit marker); kill = a heavier one.
    void Hit(bool kill) { m_hitTimer = 0.25f; m_hitKill = kill; }
    // A line of text above the radar (zone / street names, pick-ups, help).
    void ShowMessage(const std::string& text, float seconds);
    // CHud::SetHelpMessageStatUpdate: the "+" skill box with a stat bar (pct 0..100), shown for 5 s.
    void StatUpdate(float pct, bool increase = true) { m_statPct = pct; m_statUp = increase; m_statTimer = 5.0f; }
    // PRINT_NOW: the subtitle line at the bottom centre (CHud::DrawSubtitles), for a number of seconds.
    void Subtitle(const std::string& text, float seconds) { m_sub = text; m_subTimer = seconds; }

    // The script's fade, bars, subtitle line and text commands, drawn after the HUD.
    void DrawScript(D3DRenderer& r, const ScriptOverlay& o);

    // Rotated textured quad (blips, arrows), angle in radians clockwise.
    void RotatedSprite(Fe2d& g, IDirect3DTexture9* tex, float cx, float cy, float w, float h, float angle,
                       uint32_t color);

    // World -> tile texture for the radar and the pause map.
    IDirect3DTexture9* RadarTile(int tx, int ty);   // tx east 0..11, ty south 0..11
    IDirect3DTexture9* Sprite(const char* name) const;
    FeFont& Font() { return m_font; }
    FeText& Text() { return m_text; }

    // The area-name lookup (info.zon): GXT text for a world position, or "".
    std::string ZoneAt(float x, float y, float z) const;

private:
    struct Zone { std::string key; float x0, y0, z0, x1, y1, z1; };

    void DrawClassic(Fe2d& g, const HudData& d, float sw, float sh);
    void DrawRewrite(Fe2d& g, const HudData& d, float sw, float sh);
    void DrawWheel(Fe2d& g, const HudData& d, float sw, float sh);
    void DrawRadar(Fe2d& g, const HudData& d, float cx, float cy, float hw, float hh, float corner,
                   float range, float alpha, bool rewrite);
    void Bar(Fe2d& g, float x, float y, float w, float h, float pct, uint32_t color, bool rewrite);
    void Text(Fe2d& g, FeFont::Style style, float x, float y, const std::string& s, float scaleX, float scaleY,
              uint32_t color, FeFont::Align align, int edge);
    void DrawLockOn(Fe2d& g, const HudData& d, float sh);
    void Vignette(Fe2d& g, float sw, float sh, uint32_t rgb, float strength);
    void RoundedRect(std::vector<Fe2d::Vert>& out, float x0, float y0, float x1, float y1, float r,
                     uint32_t color) const;

    D3DRenderer* m_r = nullptr;
    bool m_ready = false;
    Fe2d m_2d;
    IDirect3DTexture9* m_grain = nullptr;   // CPostEffects grain raster: random grey, random alpha
    int m_gogglesPrev = 0;
    FeFont m_font;
    FeText m_text;
    ImgLoader m_gta3;
    bool m_haveImg = false;
    std::unordered_map<std::string, IDirect3DTexture9*> m_sprites;
    std::vector<IDirect3DTexture9*> m_tiles = std::vector<IDirect3DTexture9*>(144, nullptr);
    std::vector<bool> m_tileTried = std::vector<bool>(144, false);
    std::vector<Zone> m_zones;

    // Fades and timers
    float m_time = 0.0f;
    float m_weaponFade = 0.0f, m_moneyFade = 0.0f, m_vitalsFade = 1.0f, m_wantedFade = 0.0f;
    float m_hitTimer = 0.0f;
    float m_lockRot = 0.0f, m_lockPulse = 0.0f;   // CWeaponEffects crosshair rotation and pulse
    bool m_lockGrow = false;
    bool m_hitKill = false;
    int m_lastMoney = 0, m_lastWanted = 0;
    bool m_moneyInit = false, m_lastArmed = false;
    int m_moneyDiff = 0, m_lastClip = -1, m_lastTotal = -1;
    float m_diffTimer = 0.0f, m_ammoTimer = 0.0f, m_cashY = 0.0f;
    float m_lastHealth = 100.0f;
    std::string m_msg;
    float m_msgTimer = 0.0f, m_msgFade = 0.0f;
    std::string m_sub;
    float m_subTimer = 0.0f;
    float m_statPct = 0.0f, m_statTimer = 0.0f;
    bool m_statUp = true;
    std::string m_lastZone;
    float m_zoneTimer = 0.0f;
};
