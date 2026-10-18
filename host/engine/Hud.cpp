// Hud.cpp - see Hud.h. Layout numbers for the classic style are CHud's, in
// 640x448 design units (CHud::DrawPlayerInfo @ 0058eaf0, DrawWanted, DrawAmmo,
// DrawWeaponIcon, DrawAreaName; CRadar::TransformRealWorldToTexCoordSpace).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Hud.h"

#include "D3DRenderer.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

const float kPi = 3.14159265f;

// CHudColours
const uint32_t kRed = 0xFFB4191D, kGreen = 0xFF36682C, kLightBlue = 0xFFACCBF1, kLightGray = 0xFFE1E1E1,
               kGold = 0xFF906210, kCream = 0xFFE2C063, kBlack = 0xFF000000;

uint32_t Alpha(uint32_t argb, float a) {
    a = std::max(0.0f, std::min(1.0f, a));
    const uint32_t base = argb >> 24;
    return (((uint32_t)(base * a)) << 24) | (argb & 0x00FFFFFF);
}
uint32_t Mul(uint32_t argb, float k) {
    auto ch = [&](int sh) { return (uint32_t)std::min(255.0f, ((argb >> sh) & 0xFF) * k) << sh; };
    return (argb & 0xFF000000) | ch(16) | ch(8) | ch(0);
}

std::string MoneyText(int money, bool rewrite) {
    char b[32];
    if (!rewrite) {
        std::snprintf(b, sizeof(b), money < 0 ? "-$%07d" : "$%08d", money < 0 ? -money : money);
        return b;
    }
    std::string digits = std::to_string(money < 0 ? -money : money), out;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return (money < 0 ? "-$" : "$") + out;
}

std::string Upper(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

IDirect3DTexture9* Upload(D3DRenderer& r, const TxdTexture& t) {
    return r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
}

}  // namespace

bool Hud::Init(D3DRenderer& r, const std::string& gameDir) {
    m_r = &r;
    m_text.Load(gameDir, "american.gxt");
    m_font.Init(r, gameDir);
    for (const TxdTexture& t : TxdLoader::Load(gameDir + "\\models\\hud.txd"))
        if (IDirect3DTexture9* tex = Upload(r, t)) m_sprites[TxdLoader::KeyOf(t.name)] = tex;
    m_haveImg = m_gta3.Open(gameDir + "\\models\\gta3.img");
    // info.zon: name, type, box, island, GXT key
    std::ifstream zf(gameDir + "\\data\\info.zon");
    for (std::string line; std::getline(zf, line);) {
        for (char& c : line) if (c == ',') c = ' ';
        std::istringstream in(line);
        std::string name, key;
        int type = 0, island = 0;
        Zone z;
        if (!(in >> name >> type >> z.x0 >> z.y0 >> z.z0 >> z.x1 >> z.y1 >> z.z1 >> island >> key)) continue;
        z.key = key;
        m_zones.push_back(z);
    }
    std::fprintf(stderr, "[HUD] %zu sprites, %zu zones, font %s\n", m_sprites.size(), m_zones.size(),
                 m_font.Ready() ? "ok" : "MISSING");
    m_ready = m_font.Ready();
    return m_ready;
}

void Hud::Shutdown() {
    m_font.Shutdown();
    if (!m_r) return;
    for (auto& kv : m_sprites) if (kv.second) m_r->DestroyTexture(kv.second);
    m_sprites.clear();
    for (IDirect3DTexture9*& t : m_tiles) if (t) { m_r->DestroyTexture(t); t = nullptr; }
    m_ready = false;
}

IDirect3DTexture9* Hud::Sprite(const char* name) const {
    auto it = m_sprites.find(TxdLoader::KeyOf(name));
    return it == m_sprites.end() ? nullptr : it->second;
}

// radarNN.txd in gta3.img, NN = ty * 12 + tx, north row first.
IDirect3DTexture9* Hud::RadarTile(int tx, int ty) {
    if (tx < 0 || tx > 11 || ty < 0 || ty > 11) return nullptr;
    const int idx = ty * 12 + tx;
    if (m_tiles[idx] || m_tileTried[idx] || !m_haveImg) return m_tiles[idx];
    m_tileTried[idx] = true;
    char name[32];
    std::snprintf(name, sizeof(name), "radar%02d.txd", idx);
    const std::vector<uint8_t> data = m_gta3.Extract(name);
    if (data.empty()) return nullptr;
    const std::vector<TxdTexture> txd = TxdLoader::LoadFromMemory(data.data(), data.size());
    if (!txd.empty()) m_tiles[idx] = Upload(*m_r, txd[0]);
    return m_tiles[idx];
}

std::string Hud::ZoneAt(float x, float y, float z) const {
    const Zone* best = nullptr;
    float bestArea = 1e30f;
    for (const Zone& zn : m_zones) {
        if (x < zn.x0 || x > zn.x1 || y < zn.y0 || y > zn.y1 || z < zn.z0 || z > zn.z1) continue;
        const float area = (zn.x1 - zn.x0) * (zn.y1 - zn.y0);
        if (area < bestArea) { bestArea = area; best = &zn; }
    }
    return best ? m_text.Get(best->key.c_str()) : std::string();
}

void Hud::ShowMessage(const std::string& text, float seconds) {
    m_msg = text;
    m_msgTimer = seconds;
}

// ---- Drawing helpers ----

void Hud::Text(Fe2d& g, FeFont::Style style, float x, float y, const std::string& s, float scaleX, float scaleY,
               uint32_t color, FeFont::Align align, int edge) {
    m_font.SetStyle(style);
    m_font.SetScale(m_font.Sx(scaleX), m_font.Sy(scaleY));
    m_font.SetAlign(align);
    m_font.SetWrapWidth(0);
    m_font.SetColor(color);
    m_font.SetDropColor(Alpha(kBlack, (color >> 24) / 255.0f));
    if (edge > 0) m_font.SetEdge(edge); else m_font.SetEdge(0);
    m_font.Print(g, x, y, s);
    m_font.SetEdge(0);
}

// DrawBarChart: black border, dim remainder, bright fill.
void Hud::Bar(Fe2d& g, float x, float y, float w, float h, float pct, uint32_t color, bool rewrite) {
    pct = std::max(0.0f, std::min(100.0f, pct));
    const float b = std::max(1.0f, std::floor(h / 9.0f));
    if (!rewrite) {
        g.DrawRect(x - b, y - b, x + w + b, y + h + b, Alpha(kBlack, 1.0f));
        g.DrawRect(x, y, x + w, y + h, Mul(color, 0.35f));
        g.DrawRect(x, y, x + w * pct / 100.0f, y + h, color);
        return;
    }
    g.DrawRect(x - b, y - b, x + w + b, y + h + b, 0x99000000);
    g.DrawRect(x, y, x + w, y + h, Alpha(Mul(color, 0.3f), 0.85f));
    const float fw = w * pct / 100.0f;
    // lit fill with a soft top highlight
    g.DrawRect(x, y, x + fw, y + h, color);
    g.DrawRect(x, y, x + fw, y + h * 0.4f, 0x30FFFFFF);
}

void Hud::RotatedSprite(Fe2d& g, IDirect3DTexture9* tex, float cx, float cy, float w, float h, float angle,
                        uint32_t color) {
    if (!tex) return;
    const float c = std::cos(angle), s = std::sin(angle);
    const float hx[4] = {-w / 2, w / 2, w / 2, -w / 2}, hy[4] = {-h / 2, -h / 2, h / 2, h / 2};
    const float u[4] = {0, 1, 1, 0}, v[4] = {0, 0, 1, 1};
    Fe2d::Vert q[4];
    for (int i = 0; i < 4; i++)
        q[i] = {cx + hx[i] * c - hy[i] * s, cy + hx[i] * s + hy[i] * c, u[i], v[i], color};
    const Fe2d::Vert tris[6] = {q[0], q[1], q[2], q[0], q[2], q[3]};
    g.DrawTris(tex, tris, 6);
}

void Hud::Vignette(Fe2d& g, float sw, float sh, uint32_t rgb, float strength) {
    if (strength <= 0.01f) return;
    const float t = std::min(sw, sh) * 0.32f;
    const uint32_t in = rgb & 0x00FFFFFF;
    const uint32_t out = Alpha(0xFF000000 | in, strength);
    const uint32_t zero = 0x00000000 | in;
    auto strip = [&](float ax, float ay, float bx, float by, float ix, float iy, float jx, float jy) {
        const Fe2d::Vert q[6] = {{ax, ay, 0, 0, out}, {bx, by, 0, 0, out}, {ix, iy, 0, 0, zero},
                                 {bx, by, 0, 0, out}, {jx, jy, 0, 0, zero}, {ix, iy, 0, 0, zero}};
        g.DrawTris(nullptr, q, 6);
    };
    strip(0, 0, sw, 0, 0, t, sw, t);
    strip(0, sh, sw, sh, 0, sh - t, sw, sh - t);
    strip(0, 0, 0, sh, t, 0, t, sh);
    strip(sw, 0, sw, sh, sw - t, 0, sw - t, sh);
}

namespace {
// Perimeter of a rounded rectangle (radius >= half extent -> ellipse), clockwise.
void Perimeter(std::vector<std::pair<float, float>>& out, float cx, float cy, float hw, float hh, float r) {
    r = std::min(r, std::min(hw, hh));
    const int seg = 14;
    const float cxs[4] = {cx + hw - r, cx + hw - r, cx - hw + r, cx - hw + r};   // TR, BR, BL, TL
    const float cys[4] = {cy - hh + r, cy + hh - r, cy + hh - r, cy - hh + r};
    for (int k = 0; k < 4; k++) {
        const float a0 = -kPi / 2 + k * kPi / 2;
        for (int i = 0; i <= seg; i++) {
            const float a = a0 + (kPi / 2) * i / seg;
            out.push_back({cxs[k] + r * std::cos(a), cys[k] + r * std::sin(a)});
        }
    }
}
}  // namespace

void Hud::RoundedRect(std::vector<Fe2d::Vert>& out, float x0, float y0, float x1, float y1, float r,
                      uint32_t color) const {
    std::vector<std::pair<float, float>> p;
    Perimeter(p, (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2, r);
    out.push_back({(x0 + x1) / 2, (y0 + y1) / 2, 0, 0, color});
    for (auto& q : p) out.push_back({q.first, q.second, 0, 0, color});
    out.push_back({p[0].first, p[0].second, 0, 0, color});
}

// The radar: tiles of the world map rotated with the camera, clipped by the
// stencil to an ellipse (classic) or a rounded rectangle (rewrite).
void Hud::DrawRadar(Fe2d& g, const HudData& d, float cx, float cy, float hw, float hh, float corner, float range,
                    float alpha, bool rewrite) {
    IDirect3DDevice9* dev = g.Device();
    if (!dev) return;
    std::vector<std::pair<float, float>> per;
    Perimeter(per, cx, cy, hw, hh, corner);
    std::vector<Fe2d::Vert> fan;
    fan.push_back({cx, cy, 0, 0, 0xFFFFFFFF});
    for (auto& q : per) fan.push_back({q.first, q.second, 0, 0, 0xFFFFFFFF});
    fan.push_back({per[0].first, per[0].second, 0, 0, 0xFFFFFFFF});

    // 1. stencil <- shape
    dev->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);
    dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    dev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILREF, 1);
    dev->SetRenderState(D3DRS_STENCILMASK, 0xFF);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
    g.DrawFan(nullptr, fan.data(), (int)fan.size());
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);

    // 2. the sea, then the tiles
    for (auto& v : fan) v.c = Alpha(0xFF6F8FB0, alpha);
    g.DrawFan(nullptr, fan.data(), (int)fan.size());

    const float sy = std::sin(d.camYaw), cyw = std::cos(d.camYaw);
    const float k = std::min(hw, hh) / range;   // px per world unit
    auto toScreen = [&](float wx, float wy, float& sx, float& sy2) {
        const float dx = wx - d.x, dy = wy - d.y;
        sx = cx + k * (dx * sy - dy * cyw);
        sy2 = cy - k * (dx * cyw + dy * sy);
    };
    const float reach = range * 1.5f;
    for (int ty = 0; ty < 12; ty++) {
        for (int tx = 0; tx < 12; tx++) {
            const float x0 = 500.0f * tx - 3000.0f, x1 = x0 + 500.0f, y1 = 3000.0f - 500.0f * ty, y0 = y1 - 500.0f;
            if (x1 < d.x - reach || x0 > d.x + reach || y1 < d.y - reach || y0 > d.y + reach) continue;
            IDirect3DTexture9* tex = RadarTile(tx, ty);
            if (!tex) continue;
            Fe2d::Vert q[4];
            toScreen(x0, y1, q[0].x, q[0].y); q[0].u = 0; q[0].v = 0;
            toScreen(x1, y1, q[1].x, q[1].y); q[1].u = 1; q[1].v = 0;
            toScreen(x1, y0, q[2].x, q[2].y); q[2].u = 1; q[2].v = 1;
            toScreen(x0, y0, q[3].x, q[3].y); q[3].u = 0; q[3].v = 1;
            for (auto& v : q) v.c = Alpha(0xFFFFFFFF, alpha);
            const Fe2d::Vert tris[6] = {q[0], q[1], q[2], q[0], q[2], q[3]};
            g.DrawTris(tex, tris, 6);
        }
    }
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);

    // 3. outline
    std::vector<std::pair<float, float>> outer;
    const float t = rewrite ? 2.0f : 3.0f;
    Perimeter(outer, cx, cy, hw + t, hh + t, corner + t);
    std::vector<Fe2d::Vert> ring;
    const uint32_t rc = rewrite ? 0xE0101820 : 0xFF000000;
    for (size_t i = 0; i < per.size(); i++) {
        const size_t j = (i + 1) % per.size();
        ring.push_back({per[i].first, per[i].second, 0, 0, rc});
        ring.push_back({outer[i].first, outer[i].second, 0, 0, rc});
        ring.push_back({per[j].first, per[j].second, 0, 0, rc});
        ring.push_back({per[j].first, per[j].second, 0, 0, rc});
        ring.push_back({outer[i].first, outer[i].second, 0, 0, rc});
        ring.push_back({outer[j].first, outer[j].second, 0, 0, rc});
    }
    g.DrawTris(nullptr, ring.data(), (int)ring.size());

    // 4. north marker on the rim, then the player arrow (radar_centre)
    {
        // north in screen space: world (0,1) -> (-cos, -sin of camYaw)
        const float ex = -cyw, ey = -sy;   // screen vector (x right, y down) pointing to north
        // distance to the rim along that direction
        const float tx = std::fabs(ex) > 1e-4f ? hw / std::fabs(ex) : 1e9f, ty = std::fabs(ey) > 1e-4f ? hh / std::fabs(ey) : 1e9f;
        const float rim = std::min(std::min(tx, ty), corner >= std::min(hw, hh) ? std::min(hw, hh) : 1e9f);
        const float px = cx + ex * rim, py = cy + ey * rim;
        IDirect3DTexture9* north = Sprite("radar_north");
        const float s = std::min(hw, hh) * 0.22f;
        if (north) RotatedSprite(g, north, px, py, s, s, 0.0f, Alpha(0xFFFFFFFF, alpha));
    }
    if (IDirect3DTexture9* me = Sprite("radar_centre")) {
        const float s = std::min(hw, hh) * (rewrite ? 0.2f : 0.17f);
        RotatedSprite(g, me, cx, cy, s, s, d.camYaw - d.heading, Alpha(0xFFFFFFFF, alpha));
    }
}

void Hud::Draw(D3DRenderer& r, const HudData& d) {
    if (!m_ready || !r.GetDevice() || !d.showHud) return;
    const float sw = (float)r.Width(), sh = (float)r.Height();
    m_time += d.dt;

    // Fades: the weapon block shows after a change and while armed.
    m_weaponFade = std::min(1.0f, std::max(0.0f, m_weaponFade + (d.armed ? 3.0f : -1.5f) * d.dt));
    if (d.money != m_lastMoney) { m_moneyFade = 4.0f; m_lastMoney = d.money; }
    m_moneyFade = std::max(0.0f, m_moneyFade - d.dt);
    if (d.wanted != m_lastWanted) { m_wantedFade = 4.0f; m_lastWanted = d.wanted; }
    m_wantedFade = std::max(0.0f, m_wantedFade - d.dt);
    m_hitTimer = std::max(0.0f, m_hitTimer - d.dt);
    m_msgTimer = std::max(0.0f, m_msgTimer - d.dt);
    m_msgFade = std::min(1.0f, std::max(0.0f, m_msgFade + (m_msgTimer > 0.0f ? 4.0f : -4.0f) * d.dt));

    // Zone name state (CHud::DrawAreaName): fade in 1 s, show 3 s, fade out 1 s,
    // a new name fades the old one out first.
    {
        if (d.zone != m_lastZone) {
            m_lastZone = d.zone;
            m_zoneTimer = 0.0f;   // restart: fade-in below follows the timer
        }
        m_zoneTimer += d.dt;
    }

    Fe2d& g = m_2d;
    g.Begin(r.GetDevice());
    if (d.rewrite) DrawRewrite(g, d, sw, sh); else DrawClassic(g, d, sw, sh);
    g.End();
}

// ---- Classic (retail) ----

void Hud::DrawClassic(Fe2d& g, const HudData& d, float sw, float sh) {
    const float u = sh / 448.0f;               // pixels per design unit
    m_font.SetScreen(640.0f * u, 448.0f * u);
    const float right = sw - 32.0f * u;
    const float off = d.maxHealth < 101.0f ? 1.0f : 0.0f;   // GetYPosBasedOnHealth

    // Clock: pricedown 0.55 x 1.1, light gray
    char clk[16];
    std::snprintf(clk, sizeof(clk), "%02d:%02d", d.hour, d.minute);
    Text(g, FeFont::PRICEDOWN, right, 22.0f * u, clk, 0.55f, 1.1f, kLightGray, FeFont::RIGHT, (int)(2 * u));

    // Weapon icon (47 x 58) and ammo
    {
        const float x = sw - (32.0f + 111.0f) * u, y = 20.0f * u;
        IDirect3DTexture9* icon = d.weaponIcon ? d.weaponIcon : Sprite("fist");
        if (icon) g.DrawTex(icon, x, y, x + 47.0f * u, y + 58.0f * u, 0xFFFFFFFF);
        if (d.armed && !d.hideAmmo) {
            char a[32];
            if (d.clipSize <= 1 || d.clipSize >= 1000) std::snprintf(a, sizeof(a), "%d", d.ammoClip + d.ammoTotal);
            else std::snprintf(a, sizeof(a), "%d-%d", std::min(9999, d.ammoTotal), d.ammoClip);
            Text(g, FeFont::SUBTITLES, x + 23.5f * u, 63.0f * u, a, 0.3f, 0.7f, kLightBlue, FeFont::CENTER, (int)(1 * u));
        }
    }

    // Bars: armour 62 wide, breath 62, health 109 (right aligned)
    if (d.armor > 1.0f) Bar(g, right - 62.0f * u, (48.0f - 3.0f * off) * u, 62.0f * u, 9.0f * u, d.armor, kLightGray, false);
    if (d.showBreath) Bar(g, right - 62.0f * u, (62.0f - 6.0f * off) * u, 62.0f * u, 9.0f * u, d.breath * 100.0f, kLightBlue, false);
    const bool blink = ((int)(m_time * 8.0f) & 1) && d.health < 10.0f;
    if (!blink) Bar(g, right - 109.0f * u, (77.0f - 10.0f * off) * u, 109.0f * u, 9.0f * u, d.health * 100.0f / d.maxHealth, kRed, false);

    // Money: pricedown, green, y 89 - 12
    Text(g, FeFont::PRICEDOWN, right, (89.0f - 12.0f * off) * u, MoneyText(d.money, false), 0.55f, 1.1f,
         d.money < 0 ? kRed : kGreen, FeFont::RIGHT, (int)(2 * u));

    // Wanted stars: gothic ']' at 114 - 12, 18 apart, from the right
    if (d.wanted > 0 || m_wantedFade > 0.0f) {
        for (int i = 0; i < 6; i++) {
            const bool lit = i < d.wanted;
            const bool flash = ((int)(m_time * 8.0f) & 1) != 0;
            uint32_t col = lit ? (flash && m_wantedFade > 0.0f ? kGold : kCream) : 0x40000000;
            Text(g, FeFont::GOTHIC, sw - 29.0f * u - i * 18.0f * u, (114.0f - 12.0f * off) * u, "]",
                 0.605f, 1.21f, lit ? col : 0x60000000, FeFont::RIGHT, lit ? (int)(1 * u) : 0);
        }
    }

    // Radar: bottom-left, centre (87, H-66), 102 x 84 units
    if (d.radarOn) DrawRadar(g, d, 87.0f * u, sh - 66.0f * u, 48.0f * u, 48.0f * u, 48.0f * u, 180.0f, 1.0f, false);

    // Area name: gothic 1.2 x 1.9, light blue, right aligned, bottom at H-28
    if (!d.zone.empty()) {
        float a = 1.0f;
        if (m_zoneTimer < 1.0f) a = m_zoneTimer;
        else if (m_zoneTimer > 4.0f) a = std::max(0.0f, 5.0f - m_zoneTimer);
        if (a > 0.0f) {
            Text(g, FeFont::GOTHIC, right, sh - 28.0f * u - 1.9f * 32.0f * 0.5f * u, d.zone, 1.2f, 1.9f,
                 Alpha(kLightBlue, a), FeFont::RIGHT, (int)(2 * u));
        }
    }
    // Message box (help text)
    if (m_msgFade > 0.0f && !m_msg.empty()) {
        g.DrawRect(34.0f * u, 18.0f * u, 34.0f * u + 220.0f * u, 18.0f * u + 40.0f * u, Alpha(0xFF000000, 0.6f * m_msgFade));
        m_font.SetStyle(FeFont::SUBTITLES);
        m_font.SetScale(m_font.Sx(0.4f), m_font.Sy(0.95f));
        m_font.SetAlign(FeFont::LEFT);
        m_font.SetWrapWidth(206.0f * u);
        m_font.SetColor(Alpha(kLightGray, m_msgFade));
        m_font.SetEdge(0);
        m_font.Print(g, 40.0f * u, 22.0f * u, m_msg);
        m_font.SetWrapWidth(0);
    }
    // Crosshair: the retail sprite while aiming
    if (d.aiming && d.armed) {
        if (IDirect3DTexture9* site = Sprite("siteM16")) {
            const float s = 28.0f * u;
            g.DrawTex(site, sw / 2 - s / 2, sh / 2 - s / 2, sw / 2 + s / 2, sh / 2 + s / 2, 0xFFFFFFFF);
        }
    }
}

// ---- Rewrite (modern) ----

void Hud::DrawRewrite(Fe2d& g, const HudData& d, float sw, float sh) {
    const float u = sh / 448.0f * d.scale;
    m_font.SetScreen(640.0f * u, 448.0f * u);
    const float m = 26.0f * u;                 // safe margin
    const float right = sw - m;

    // damage vignette, low-health pulse
    if (d.damageFlash) {
        Vignette(g, sw, sh, 0xFFB4191D, d.damageFlashAmount * 0.55f);
        const float low = 1.0f - std::min(1.0f, d.health / 30.0f);
        if (d.health > 0.0f && d.health < 30.0f)
            Vignette(g, sw, sh, 0xFF8A0F12, low * (0.25f + 0.2f * std::sin(m_time * (3.0f + 5.0f * low))));
    }

    // ---- radar block, bottom left ----
    const float rw = 94.0f * u, rh = 64.0f * u;
    const float rx = m + rw, ry = sh - m - 20.0f * u - rh;
    if (d.radarOn) {
        // soft shadow
        std::vector<Fe2d::Vert> sd;
        RoundedRect(sd, rx - rw - 6 * u, ry - rh - 4 * u, rx + rw + 6 * u, ry + rh + 12 * u, 14.0f * u, 0x40000000);
        g.DrawFan(nullptr, sd.data(), (int)sd.size());
        DrawRadar(g, d, rx, ry, rw, rh, 12.0f * u, 150.0f, 1.0f, true);
        // vitals under the radar: armour left, health right
        const float by = ry + rh + 8.0f * u, bh = 6.0f * u, gap = 4.0f * u;
        const float half = (2 * rw - gap) / 2;
        const bool lowHp = d.health < 30.0f;
        const uint32_t hc = lowHp ? ((int)(m_time * 6.0f) & 1 ? 0xFFE04040 : 0xFFB03030) : 0xFF72CC6E;
        Bar(g, rx - rw, by, half, bh, d.armor * 100.0f / 100.0f, 0xFF6AA9E8, true);
        Bar(g, rx - rw + half + gap, by, half, bh, d.health * 100.0f / d.maxHealth, hc, true);
        if (d.showBreath) Bar(g, rx - rw, by - 10.0f * u, 2 * rw, 4.0f * u, d.breath * 100.0f, 0xFFB8E4FF, true);
    } else {
        const float by = sh - m, bh = 6.0f * u;
        Bar(g, m, by - bh, 120.0f * u, bh, d.health * 100.0f / d.maxHealth, 0xFF72CC6E, true);
        if (d.armor > 1.0f) Bar(g, m, by - bh - 10.0f * u, 120.0f * u, bh, d.armor, 0xFF6AA9E8, true);
    }

    // ---- zone name, above the radar ----
    if (!d.zone.empty()) {
        float a = 1.0f;
        if (m_zoneTimer < 1.0f) a = m_zoneTimer;
        else if (m_zoneTimer > 4.0f) a = std::max(0.0f, 5.0f - m_zoneTimer);
        if (a > 0.0f) {
            const float zy = ry - rh - 36.0f * u + (1.0f - a) * 6.0f * u;
            Text(g, FeFont::PRICEDOWN, rx - rw, zy, Upper(d.zone), 0.5f, 1.0f, Alpha(0xFFFFFFFF, a), FeFont::LEFT, (int)(1 * u));
        }
    }

    // ---- top right: wanted, cash, clock ----
    float y = m;
    if (d.wanted > 0 || m_wantedFade > 0.0f) {
        for (int i = 0; i < 6; i++) {
            const bool lit = i < d.wanted;
            const bool flash = d.wanted > 0 && m_wantedFade > 0.0f && ((int)(m_time * 6.0f) & 1);
            const uint32_t col = lit ? (flash ? 0xFFFFFFFF : 0xFFF2C94C) : 0x50FFFFFF;
            Text(g, FeFont::GOTHIC, right - i * 24.0f * u, y - 8.0f * u, "]", 0.7f, 1.4f, col, FeFont::RIGHT, lit ? (int)(1 * u) : 0);
        }
        y += 34.0f * u;
    }
    {
        const float a = std::min(1.0f, 0.55f + m_moneyFade * 0.45f);
        Text(g, FeFont::PRICEDOWN, right, y, MoneyText(d.money, true), 0.62f, 1.25f,
             d.money < 0 ? Alpha(0xFFE05050, a) : Alpha(0xFF8FD47A, a), FeFont::RIGHT, (int)(2 * u));
        y += 36.0f * u;
        char clk[16];
        std::snprintf(clk, sizeof(clk), "%02d:%02d", d.hour, d.minute);
        Text(g, FeFont::SUBTITLES, right, y, clk, 0.42f, 0.95f, 0xC0FFFFFF, FeFont::RIGHT, (int)(1 * u));
        y += 24.0f * u;
    }

    // ---- weapon, under the clock ----
    if (m_weaponFade > 0.01f && d.armed) {
        const float a = m_weaponFade;
        const float iw = 70.0f * u, ih = 46.0f * u;
        IDirect3DTexture9* icon = d.weaponIcon ? d.weaponIcon : Sprite("fist");
        // panel
        std::vector<Fe2d::Vert> pn;
        RoundedRect(pn, right - 188.0f * u, y, right + 4.0f * u, y + 54.0f * u, 8.0f * u, Alpha(0xFF0B1014, 0.55f * a));
        g.DrawFan(nullptr, pn.data(), (int)pn.size());
        if (icon) g.DrawTex(icon, right - 184.0f * u, y + 4.0f * u, right - 184.0f * u + iw, y + 4.0f * u + ih, Alpha(0xFFFFFFFF, a));
        if (!d.hideAmmo) {
            char big[16], small[16];
            std::snprintf(big, sizeof(big), "%d", d.ammoClip);
            std::snprintf(small, sizeof(small), "/ %d", d.ammoTotal);
            const bool empty = d.ammoClip == 0;
            if (d.clipSize > 1) {
                Text(g, FeFont::PRICEDOWN, right - 56.0f * u, y + 4.0f * u, big, 0.7f, 1.4f,
                     Alpha(empty ? 0xFFE05050 : 0xFFFFFFFF, a), FeFont::RIGHT, (int)(1 * u));
                Text(g, FeFont::SUBTITLES, right - 52.0f * u, y + 22.0f * u, small, 0.4f, 0.9f, Alpha(0xFFC8D4E0, a), FeFont::LEFT, 0);
            } else {
                std::snprintf(big, sizeof(big), "%d", d.ammoClip + d.ammoTotal);
                Text(g, FeFont::PRICEDOWN, right - 8.0f * u, y + 8.0f * u, big, 0.6f, 1.2f, Alpha(0xFFFFFFFF, a), FeFont::RIGHT, (int)(1 * u));
            }
            if (d.reloading)
                Text(g, FeFont::SUBTITLES, right - 8.0f * u, y + 54.0f * u + 2.0f * u, "RELOADING", 0.34f, 0.8f, Alpha(0xFFF2C94C, a), FeFont::RIGHT, 0);
            else if (d.ammoClip == 0 && d.ammoTotal == 0)
                Text(g, FeFont::SUBTITLES, right - 8.0f * u, y + 54.0f * u + 2.0f * u, "NO AMMO", 0.34f, 0.8f, Alpha(0xFFE05050, a), FeFont::RIGHT, 0);
        }
    }

    // ---- crosshair and hit marker ----
    if (d.aiming && d.armed && d.crosshairDot) {
        const float cx = sw / 2, cyy = sh / 2;
        const float gap = (3.0f + 10.0f * d.spread) * u, len = 5.0f * u, th = 1.4f * u;
        const uint32_t c = 0xE0FFFFFF, sh2 = 0x70000000;
        auto bar = [&](float x0, float y0, float x1, float y1) {
            g.DrawRect(x0 + 1, y0 + 1, x1 + 1, y1 + 1, sh2);
            g.DrawRect(x0, y0, x1, y1, c);
        };
        bar(cx - th, cyy - gap - len, cx + th, cyy - gap);
        bar(cx - th, cyy + gap, cx + th, cyy + gap + len);
        bar(cx - gap - len, cyy - th, cx - gap, cyy + th);
        bar(cx + gap, cyy - th, cx + gap + len, cyy + th);
        g.DrawRect(cx - th, cyy - th, cx + th, cyy + th, c);
    }
    if (d.hitMarker && m_hitTimer > 0.0f) {
        const float cx = sw / 2, cyy = sh / 2, a = m_hitTimer / 0.25f;
        const uint32_t c = Alpha(m_hitKill ? 0xFFE05050 : 0xFFFFFFFF, a);
        const float s = 9.0f * u * (1.5f - 0.5f * a), l = 5.0f * u;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                Fe2d::Vert q[6];
                const float ax = cx + sx * s, ay = cyy + sy * s, bx = cx + sx * (s + l), by = cyy + sy * (s + l), t = 1.1f * u;
                const Fe2d::Vert tris[6] = {{ax - t * sy, ay + t * sx, 0, 0, c}, {ax + t * sy, ay - t * sx, 0, 0, c}, {bx - t * sy, by + t * sx, 0, 0, c},
                                            {ax + t * sy, ay - t * sx, 0, 0, c}, {bx + t * sy, by - t * sx, 0, 0, c}, {bx - t * sy, by + t * sx, 0, 0, c}};
                (void)q;
                g.DrawTris(nullptr, tris, 6);
            }
    }

    // ---- message (help text), bottom centre ----
    if (m_msgFade > 0.0f && !m_msg.empty()) {
        const float a = m_msgFade;
        m_font.SetStyle(FeFont::SUBTITLES);
        m_font.SetScale(m_font.Sx(0.5f), m_font.Sy(1.1f));
        const float w = m_font.StringWidth(m_msg);
        std::vector<Fe2d::Vert> pn;
        RoundedRect(pn, sw / 2 - w / 2 - 16.0f * u, sh - 96.0f * u, sw / 2 + w / 2 + 16.0f * u, sh - 60.0f * u, 10.0f * u,
                    Alpha(0xFF0B1014, 0.65f * a));
        g.DrawFan(nullptr, pn.data(), (int)pn.size());
        Text(g, FeFont::SUBTITLES, sw / 2, sh - 91.0f * u, m_msg, 0.5f, 1.1f, Alpha(0xFFFFFFFF, a), FeFont::CENTER, 0);
    }
}
