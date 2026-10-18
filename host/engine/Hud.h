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
    float spread = 0.0f;           // 0..1 crosshair widening (moving, firing)
    bool firstPerson = false;
    // Radar and map
    float x = 0, y = 0, z = 0;
    float camYaw = 0;              // radians, world yaw the camera looks along
    float heading = 0;             // radians, world yaw the player faces
    std::string zone;              // GXT text of the area name; empty = none
    std::vector<HudLabel> labels;
    bool hasWaypoint = false;      // map waypoint blip
    float wpX = 0, wpY = 0;
};

class Hud {
public:
    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown();
    bool Ready() const { return m_ready; }

    // The in-game HUD (call after the world, before the pause menu).
    void Draw(D3DRenderer& r, const HudData& d);

    // A hit confirmed on a target (rewrite hit marker); kill = a heavier one.
    void Hit(bool kill) { m_hitTimer = 0.25f; m_hitKill = kill; }
    // A line of text above the radar (zone / street names, pick-ups, help).
    void ShowMessage(const std::string& text, float seconds);

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
    void DrawRadar(Fe2d& g, const HudData& d, float cx, float cy, float hw, float hh, float corner,
                   float range, float alpha, bool rewrite);
    void Bar(Fe2d& g, float x, float y, float w, float h, float pct, uint32_t color, bool rewrite);
    void Text(Fe2d& g, FeFont::Style style, float x, float y, const std::string& s, float scaleX, float scaleY,
              uint32_t color, FeFont::Align align, int edge);
    void Vignette(Fe2d& g, float sw, float sh, uint32_t rgb, float strength);
    void RoundedRect(std::vector<Fe2d::Vert>& out, float x0, float y0, float x1, float y1, float r,
                     uint32_t color) const;

    D3DRenderer* m_r = nullptr;
    bool m_ready = false;
    Fe2d m_2d;
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
    bool m_hitKill = false;
    int m_lastMoney = 0, m_lastWanted = 0;
    bool m_moneyInit = false, m_lastArmed = false;
    int m_moneyDiff = 0, m_lastClip = -1, m_lastTotal = -1;
    float m_diffTimer = 0.0f, m_ammoTimer = 0.0f, m_cashY = 0.0f;
    float m_lastHealth = 100.0f;
    std::string m_msg;
    float m_msgTimer = 0.0f, m_msgFade = 0.0f;
    std::string m_lastZone;
    float m_zoneTimer = 0.0f;
};
