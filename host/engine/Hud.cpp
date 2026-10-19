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
        // CSprite2d::DrawBarChart: lit part, the rest at half colour, then a black border
        // drawn inside the bar. Edges snap to whole pixels so the bar stays crisp.
        const float x0 = std::floor(x + 0.5f), y0 = std::floor(y + 0.5f);
        const float x1 = std::floor(x + w + 0.5f), y1 = std::floor(y + h + 0.5f);
        const float cx = std::floor(x0 + (x1 - x0) * pct / 100.0f + 0.5f);
        const float t = std::max(1.0f, std::floor(2.0f * h / 9.0f + 0.5f));
        g.DrawRect(x0, y0, cx, y1, color);
        g.DrawRect(cx, y0, x1, y1, Mul(color, 0.5f));
        g.DrawRect(x0, y0, x1, y0 + t, kBlack);
        g.DrawRect(x0, y1 - t, x1, y1, kBlack);
        g.DrawRect(x0, y0, x0 + t, y1, kBlack);
        g.DrawRect(x1 - t, y0, x1, y1, kBlack);
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

void Hud::Goggles(D3DRenderer& r, int mode) {
    if (!m_ready || !r.GetDevice() || mode < 44) return;
    const float sw = (float)r.Width(), sh = (float)r.Height();
    const uint32_t col = mode == 44 ? Alpha(0xFF10FF20, 0.22f) : Alpha(0xFF2040FF, 0.22f);
    Fe2d& g = m_2d;
    g.Begin(r.GetDevice());
    const Fe2d::Vert q[6] = {{0, 0, 0, 0, col}, {sw, 0, 0, 0, col}, {0, sh, 0, 0, col},
                             {sw, 0, 0, 0, col}, {sw, sh, 0, 0, col}, {0, sh, 0, 0, col}};
    g.DrawTris(nullptr, q, 6);
    g.End();
}

void Hud::Scope(D3DRenderer& r, bool camera, bool rocket) {
    if (!m_ready || !r.GetDevice()) return;
    const float sw = (float)r.Width(), sh = (float)r.Height();
    const float cx = sw * 0.5f, cy = sh * 0.5f;
    Fe2d& g = m_2d;
    g.Begin(r.GetDevice());
    std::vector<Fe2d::Vert> v;
    auto quad = [&](float x0, float y0, float x1, float y1, uint32_t c) {
        const Fe2d::Vert q[6] = {{x0, y0, 0, 0, c}, {x1, y0, 0, 0, c}, {x0, y1, 0, 0, c},
                                 {x1, y0, 0, 0, c}, {x1, y1, 0, 0, c}, {x0, y1, 0, 0, c}};
        v.insert(v.end(), q, q + 6);
    };
    const uint32_t line = 0xFFB4FFB4;   // the scope colour (180, 255, 180)
    const float t = std::max(1.0f, sh * 0.002f);
    if (rocket) {   // the launcher sight: four corner brackets round a centre dot, no lens
        const float h = sh * 0.045f, a = sh * 0.02f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                const float x = cx + sx * h, y = cy + sy * h;
                quad(std::min(x, x - sx * a), y - t, std::max(x, x - sx * a), y + t, 0xFFFFFFFF);
                quad(x - t, std::min(y, y - sy * a), x + t, std::max(y, y - sy * a), 0xFFFFFFFF);
            }
        quad(cx - t, cy - t, cx + t, cy + t, 0xFFFFFFFF);
    } else if (camera) {   // viewfinder brackets at the corners of a 4:3-ish frame
        const float hw = sh * 0.42f, hh = sh * 0.40f, a = sh * 0.06f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                const float x = cx + sx * hw, y = cy + sy * hh;
                quad(std::min(x, x - sx * a), y - t, std::max(x, x - sx * a), y + t, line);
                quad(x - t, std::min(y, y - sy * a), x + t, std::max(y, y - sy * a), line);
            }
        quad(cx - a * 0.5f, cy - t * 0.5f, cx + a * 0.5f, cy + t * 0.5f, line);
        quad(cx - t * 0.5f, cy - a * 0.5f, cx + t * 0.5f, cy + a * 0.5f, line);
    } else {   // CHud::DrawSniperCrosshair: the lens is a circle, everything outside it black
        const float rad = sh * 0.48f;
        const uint32_t black = 0xFF000000;
        const int n = 64;
        for (int i = 0; i < n; i++) {   // the surround as wedges between the circle and the screen box
            const float a0 = (float)i / n * 6.2831853f, a1 = (float)(i + 1) / n * 6.2831853f;
            const float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
            const float k = (sw + sh) * 2.0f;   // far enough to leave the screen
            const Fe2d::Vert q[6] = {{cx + c0 * rad, cy + s0 * rad, 0, 0, black}, {cx + c0 * k, cy + s0 * k, 0, 0, black}, {cx + c1 * rad, cy + s1 * rad, 0, 0, black},
                                     {cx + c1 * rad, cy + s1 * rad, 0, 0, black}, {cx + c0 * k, cy + s0 * k, 0, 0, black}, {cx + c1 * k, cy + s1 * k, 0, 0, black}};
            v.insert(v.end(), q, q + 6);
        }
        quad(cx - rad, cy - t * 0.5f, cx + rad, cy + t * 0.5f, line);
        quad(cx - t * 0.5f, cy - rad, cx + t * 0.5f, cy + rad, line);
    }
    g.DrawTris(nullptr, v.data(), (int)v.size());
    g.End();
}

void Hud::Flash(D3DRenderer& r, float a) {
    if (!m_ready || !r.GetDevice() || a <= 0.0f) return;
    const float sw = (float)r.Width(), sh = (float)r.Height();
    const uint32_t col = Alpha(0xFFFFFFFF, std::min(1.0f, a));
    Fe2d& g = m_2d;
    g.Begin(r.GetDevice());
    const Fe2d::Vert q[6] = {{0, 0, 0, 0, col}, {sw, 0, 0, 0, col}, {0, sh, 0, 0, col},
                             {sw, 0, 0, 0, col}, {sw, sh, 0, 0, col}, {0, sh, 0, 0, col}};
    g.DrawTris(nullptr, q, 6);
    g.End();
}

void Hud::Brightness(D3DRenderer& r, float level) {
    if (!m_ready || !r.GetDevice() || std::fabs(level - 0.5f) < 0.02f) return;
    const float sw = (float)r.Width(), sh = (float)r.Height();
    const bool dark = level < 0.5f;
    const float a = std::min(0.6f, std::fabs(level - 0.5f) * (dark ? 1.2f : 0.7f));
    const uint32_t col = Alpha(dark ? 0xFF000000 : 0xFFFFFFFF, a);
    Fe2d& g = m_2d;
    g.Begin(r.GetDevice());
    const Fe2d::Vert q[6] = {{0, 0, 0, 0, col}, {sw, 0, 0, 0, col}, {0, sh, 0, 0, col},
                             {sw, 0, 0, 0, col}, {sw, sh, 0, 0, col}, {0, sh, 0, 0, col}};
    g.DrawTris(nullptr, q, 6);
    g.End();
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

    // Script blips: a short-range one only inside the radar, the rest pinned to the rim.
    for (const RadarBlip& b : d.blips) {
        if (b.display == 0 || b.display == 1) continue;
        float bx, by;
        toScreen(b.x, b.y, bx, by);
        float ox = bx - cx, oy = by - cy;
        const float fx = std::fabs(ox) > 1e-3f ? 0.86f * hw / std::fabs(ox) : 1e9f;
        const float fy = std::fabs(oy) > 1e-3f ? 0.86f * hh / std::fabs(oy) : 1e9f;
        const float f = std::min(1.0f, std::min(fx, fy));
        if (b.shortRange && f < 1.0f) continue;
        const float s = std::min(hw, hh) * 0.2f;
        if (const char* nm = RadarBlipSpriteName(b.sprite)) {
            if (IDirect3DTexture9* tx = Sprite(nm)) RotatedSprite(g, tx, cx + ox * f, cy + oy * f, s, s, 0.0f, Alpha(0xFFFFFFFF, alpha));
        } else {
            const float q = s * 0.25f;
            g.DrawRect(cx + ox * f - q - 1, cy + oy * f - q - 1, cx + ox * f + q + 1, cy + oy * f + q + 1, Alpha(0xFF000000, alpha));
            g.DrawRect(cx + ox * f - q, cy + oy * f - q, cx + ox * f + q, cy + oy * f + q, Alpha(b.colour, alpha));
        }
    }

    // Waypoint blip, pinned to the rim when it is out of range.
    if (d.hasWaypoint) {
        float bx, by;
        toScreen(d.wpX, d.wpY, bx, by);
        float ox = bx - cx, oy = by - cy;
        const float fx = std::fabs(ox) > 1e-3f ? 0.86f * hw / std::fabs(ox) : 1e9f;
        const float fy = std::fabs(oy) > 1e-3f ? 0.86f * hh / std::fabs(oy) : 1e9f;
        const float f = std::min(1.0f, std::min(fx, fy));
        if (IDirect3DTexture9* wp = Sprite("radar_waypoint")) {
            const float s = std::min(hw, hh) * 0.2f;
            RotatedSprite(g, wp, cx + ox * f, cy + oy * f, s, s, 0.0f, Alpha(0xFFFFFFFF, alpha));
        }
    }

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

// Weapon wheel: a ring of slots around the centre (fists at the top, then clockwise), the
// highlighted slot lit white, its name and ammo in the middle.
void Hud::DrawWheel(Fe2d& g, const HudData& d, float sw, float sh) {
    const int n = (int)d.wheelItems.size();
    if (n <= 0) return;
    const float ref = sh / 1080.0f, cx = sw * 0.5f, cy = sh * 0.5f;
    const float r0 = 150.0f * ref, r1 = 300.0f * ref;
    g.DrawRect(0, 0, sw, sh, 0x78000000);
    const float span = 2.0f * kPi / n, gap = 0.035f;
    for (int i = 0; i < n; i++) {
        const bool sel = i == d.wheelSel;
        const float a0 = -kPi / 2 + span * (i - 0.5f) + gap, a1 = -kPi / 2 + span * (i + 0.5f) - gap;
        const uint32_t col = sel ? 0xF0FFFFFF : 0x99000000;
        const int seg = 14;
        std::vector<Fe2d::Vert> v;
        const float ro = sel ? r1 + 10.0f * ref : r1;
        for (int k = 0; k < seg; k++) {
            const float t0 = a0 + (a1 - a0) * k / seg, t1 = a0 + (a1 - a0) * (k + 1) / seg;
            const Fe2d::Vert p00 = {cx + std::cos(t0) * r0, cy + std::sin(t0) * r0, 0, 0, col};
            const Fe2d::Vert p01 = {cx + std::cos(t0) * ro, cy + std::sin(t0) * ro, 0, 0, col};
            const Fe2d::Vert p10 = {cx + std::cos(t1) * r0, cy + std::sin(t1) * r0, 0, 0, col};
            const Fe2d::Vert p11 = {cx + std::cos(t1) * ro, cy + std::sin(t1) * ro, 0, 0, col};
            v.push_back(p00); v.push_back(p01); v.push_back(p10);
            v.push_back(p10); v.push_back(p01); v.push_back(p11);
        }
        g.DrawTris(nullptr, v.data(), (int)v.size());
        const WheelItem& it = d.wheelItems[i];
        const float am = (a0 + a1) * 0.5f, rm = (r0 + r1) * 0.5f + (sel ? 5.0f * ref : 0.0f);
        IDirect3DTexture9* icon = it.icon ? it.icon : Sprite("fist");
        const float iw = (sel ? 118.0f : 100.0f) * ref, ih = iw * 0.66f;
        const float ix = cx + std::cos(am) * rm, iy = cy + std::sin(am) * rm;
        if (icon) g.DrawTex(icon, ix - iw / 2, iy - ih / 2, ix + iw / 2, iy + ih / 2, sel ? 0xFF000000 : 0xFFFFFFFF);
        else Text(g, FeFont::SUBTITLES, ix, iy - 8.0f * ref, it.name, 0.4f, 0.9f, sel ? 0xFF000000 : 0xFFFFFFFF, FeFont::CENTER, 0);
    }
    const WheelItem& cur = d.wheelItems[std::max(0, std::min(n - 1, d.wheelSel))];
    Text(g, FeFont::PRICEDOWN, cx, cy - 54.0f * ref, Upper(cur.name), 0.75f, 1.5f, 0xFFFFFFFF, FeFont::CENTER, 2);
    if (!cur.melee) {
        char b[48];
        if (cur.clipSize > 1) std::snprintf(b, sizeof(b), "%d / %d", cur.clip, cur.total);
        else std::snprintf(b, sizeof(b), "%d", cur.clip + cur.total);
        Text(g, FeFont::SUBTITLES, cx, cy + 6.0f * ref, b, 0.55f, 1.2f, 0xFFC8D4E0, FeFont::CENTER, 1);
    }
}

void Hud::Draw(D3DRenderer& r, const HudData& d) {
    if (!m_ready || !r.GetDevice() || !d.showHud) return;
    const float sw = (float)r.Width(), sh = (float)r.Height();
    m_time += d.dt;

    // Fades: the weapon block shows after a change and while armed.
    m_weaponFade = std::min(1.0f, std::max(0.0f, m_weaponFade + (d.armed ? 3.0f : -1.5f) * d.dt));
    if (d.money != m_lastMoney) {
        m_moneyFade = 4.0f;
        if (m_moneyInit) { m_moneyDiff = d.money - m_lastMoney; m_diffTimer = 2.0f; }
        m_moneyInit = true;
        m_lastMoney = d.money;
    }
    m_moneyFade = std::max(0.0f, m_moneyFade - d.dt);
    m_diffTimer = std::max(0.0f, m_diffTimer - d.dt);
    if (d.ammoClip != m_lastClip || d.ammoTotal != m_lastTotal || d.armed != m_lastArmed) {
        if (d.armed) m_ammoTimer = 8.0f;
        m_lastClip = d.ammoClip; m_lastTotal = d.ammoTotal; m_lastArmed = d.armed;
    }
    m_ammoTimer = std::max(0.0f, m_ammoTimer - d.dt);
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
    if (d.lockOn) DrawLockOn(g, d, sh);
    if (d.wheel) { m_font.SetScreen(640.0f * (sh / 448.0f), 448.0f * (sh / 448.0f)); DrawWheel(g, d, sw, sh); }
    {
        const float u = sh / 448.0f;
        m_font.SetScreen(640.0f * u, 448.0f * u);
        for (const HudLabel& l : d.labels) {
            const float px = std::floor(l.x * sw + 0.5f), py = std::floor(l.y * sh + 0.5f);
            Text(g, FeFont::SUBTITLES, px, py, l.text, 0.36f * l.scale, 0.8f * l.scale, l.color,
                 l.left ? FeFont::LEFT : FeFont::CENTER, 1);
            if (l.bar >= 0.0f) Bar(g, px - 22.0f * u * l.scale, py + 15.0f * u * l.scale, 44.0f * u * l.scale, std::max(3.0f, 5.0f * u * l.scale), l.bar, 0xFFD23C3C, d.rewrite);
        }
    }
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
    Text(g, FeFont::PRICEDOWN, right, 22.0f * u, clk, 0.55f, 1.1f, kLightGray, FeFont::RIGHT, 2);

    // Bars: armour 62 wide, breath 62, health 109 (right aligned)
    if (d.armor > 1.0f) Bar(g, right - 62.0f * u, (48.0f - 3.0f * off) * u, 62.0f * u, 9.0f * u, d.armor, kLightGray, false);
    if (d.showBreath) Bar(g, right - 62.0f * u, (62.0f - 6.0f * off) * u, 62.0f * u, 9.0f * u, d.breath * 100.0f, kLightBlue, false);
    const bool blink = ((int)(m_time * 8.0f) & 1) && d.health < 10.0f;
    if (!blink) Bar(g, right - 109.0f * u, (77.0f - 10.0f * off) * u, 109.0f * u, 9.0f * u, d.health * 100.0f / d.maxHealth, kRed, false);

    // Weapon icon (47 x 58) and ammo
    {
        const float x = sw - (32.0f + 111.0f) * u, y = 20.0f * u;
        IDirect3DTexture9* icon = d.weaponIcon ? d.weaponIcon : Sprite("fist");
        if (icon) g.DrawTex(icon, x, y, x + 47.0f * u, y + 58.0f * u, 0xFFFFFFFF);
        if (d.armed && !d.hideAmmo) {
            char a[32];
            if (d.clipSize <= 1 || d.clipSize >= 1000) std::snprintf(a, sizeof(a), "%d", d.ammoClip + d.ammoTotal);
            else std::snprintf(a, sizeof(a), "%d-%d", std::min(9999, d.ammoTotal), d.ammoClip);
            Text(g, FeFont::SUBTITLES, x + 23.5f * u, 63.0f * u, a, 0.3f, 0.7f, kLightBlue, FeFont::CENTER, 1);
        }
    }

    // Money: pricedown, green, y 89 - 12
    Text(g, FeFont::PRICEDOWN, right, (89.0f - 12.0f * off) * u, MoneyText(d.money, false), 0.55f, 1.1f,
         d.money < 0 ? kRed : kGreen, FeFont::RIGHT, 2);

    // Wanted stars: gothic ']' at 114 - 12, 18 apart, from the right
    if (d.wanted > 0 || m_wantedFade > 0.0f) {
        for (int i = 0; i < 6; i++) {
            const bool lit = i < d.wanted;
            const bool flash = ((int)(m_time * 8.0f) & 1) != 0;
            uint32_t col = lit ? (flash && m_wantedFade > 0.0f ? kGold : kCream) : 0x40000000;
            Text(g, FeFont::GOTHIC, sw - 29.0f * u - i * 18.0f * u, (114.0f - 12.0f * off) * u, "]",
                 0.605f, 1.21f, lit ? col : 0x60000000, FeFont::RIGHT, lit ? 1 : 0);
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
                 Alpha(kLightBlue, a), FeFont::RIGHT, 2);
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
    // CHud::DrawCrossHairs: the siteM16 quarter ring drawn four times, mirrored about the
    // crosshair point, 64 * radius units across; a 2 x 2 dot at the smallest radius.
    if (d.aiming && d.armed && !d.lockOn) {
        const float cx = sw * d.crossX, cy = sh * d.crossY, r = d.crossRadius;
        if (r == 0.2f) g.DrawRect(cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f, 0xFFFFFFFF);
        if (IDirect3DTexture9* site = Sprite("siteM16")) {
            const float w = sw / 640.0f * 64.0f * r * 0.5f, h = sh / 448.0f * 64.0f * r * 0.5f;
            g.DrawTex(site, cx - w, cy - h, cx, cy, 0xFFFFFFFF, 0, 0, 1, 1);
            g.DrawTex(site, cx, cy - h, cx + w, cy, 0xFFFFFFFF, 1, 0, 0, 1);
            g.DrawTex(site, cx - w, cy, cx, cy + h, 0xFFFFFFFF, 0, 1, 1, 0);
            g.DrawTex(site, cx, cy, cx + w, cy + h, 0xFFFFFFFF, 1, 1, 0, 0);
        }
    }
}

// CWeaponEffects::Render @ 00742cf0, a ped target: three triangles around it, turning 0.05 rad a
// frame, each a black one with a smaller one in the target's health colour on top; the ring
// breathes in and out. Sizes are retail pixels at 448 lines.
void Hud::DrawLockOn(Fe2d& g, const HudData& d, float sh) {
    const float u = sh / 448.0f, frames = d.dt * 30.0f;
    const float size = std::clamp(d.lockSize, 0.3f, 1.2f);
    float inner = 5.0f * size, outer = 25.0f * size;
    if (d.crossRadius > 0.0f) { inner = d.crossRadius * 30.0f; outer = size * 20.0f + inner; }
    m_lockRot += 0.05f * frames;
    if (m_lockRot > 6.2831853f) m_lockRot = 0.0f;
    if (!m_lockGrow) { m_lockPulse -= 2.0f * frames; if (m_lockPulse < 0.0f) m_lockGrow = true; }
    else { m_lockPulse += size * 2.0f * frames; if (m_lockPulse > size * 20.0f) m_lockGrow = false; }
    const float h = std::clamp(d.lockHealth, 0.0f, 1.0f);
    const uint32_t col = 0xFF000000u | ((uint32_t)((1.0f - h) * 255.0f) << 16) | ((uint32_t)(h * 255.0f) << 8);
    const float pulse = std::max(0.0f, m_lockPulse);
    for (int i = 0; i < 3; i++) {
        const float a = (float)i * 120.0f * 0.0174533f + m_lockRot;
        const float b = ((float)i * 120.0f + 15.0f) * 0.0174533f + m_lockRot, c = ((float)i * 120.0f - 15.0f) * 0.0174533f + m_lockRot;
        float p[6] = {-std::sin(a) * (inner + pulse), -std::cos(a) * (inner + pulse), -std::sin(b) * (outer + pulse),
                      -std::cos(b) * (outer + pulse), -std::sin(c) * (outer + pulse), -std::cos(c) * (outer + pulse)};
        Fe2d::Vert v[3];
        for (int k = 0; k < 3; k++) v[k] = {d.lockX + p[k * 2] * u, d.lockY + p[k * 2 + 1] * u, 0, 0, 0xFF000000u};
        g.DrawTris(nullptr, v, 3);
        const float mx = (p[0] + p[2] + p[4]) / 3.0f, my = (p[1] + p[3] + p[5]) / 3.0f;
        for (int k = 0; k < 3; k++)
            v[k] = {d.lockX + ((p[k * 2] - mx) * 0.75f + mx) * u, d.lockY + ((p[k * 2 + 1] - my) * 0.75f + my) * u, 0, 0, col};
        g.DrawTris(nullptr, v, 3);
    }
}

const char* RadarBlipSpriteName(int sprite) {
    static const char* const kNames[] = {
        nullptr, nullptr, "radar_centre", "arrow", "radar_north", "radar_airYard", "radar_ammugun", "radar_barbers",
        "radar_BIGSMOKE", "radar_boatyard", "radar_burgerShot", "radar_bulldozer", "radar_CATALINAPINK",
        "radar_CESARVIAPANDO", "radar_chicken", "radar_CJ", "radar_CRASH1", "radar_diner", "radar_emmetGun",
        "radar_enemyAttack", "radar_fire", "radar_girlfriend", "radar_hostpitaL", "radar_LocoSyndicate",
        "radar_MADDOG", "radar_mafiaCasino", "radar_MCSTRAP", "radar_modGarage", "radar_OGLOC", "radar_pizza",
        "radar_police", "radar_propertyG", "radar_propertyR", "radar_race", "radar_RYDER", "radar_saveGame",
        "radar_school", "radar_qmark", "radar_SWEET", "radar_tattoo", "radar_THETRUTH", "radar_waypoint",
        "radar_TorenoRanch", "radar_triads", "radar_triadsCasino", "radar_tshirt", "radar_WOOZIE", "radar_ZERO",
        "radar_dateDisco", "radar_dateDrink", "radar_dateFood", "radar_truck", "radar_cash", "radar_flag",
        "radar_gym", "radar_impound", "radar_light", "radar_runway", "radar_gangB", "radar_gangP", "radar_gangY",
        "radar_gangN", "radar_gangG", "radar_spray"};
    return sprite >= 0 && sprite < (int)(sizeof(kNames) / sizeof(kNames[0])) ? kNames[sprite] : nullptr;
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

    // Layout in 1920x1080 reference units, HUD kept inside the 16:9 area on wider screens.
    const float ref = sh / 1080.0f * d.scale;
    const bool wide = sw / sh > 16.0f / 9.0f;
    auto hx = [&](float x) { return wide ? sw * 0.5f + (x - 960.0f) * ref : x * ref; };   // from the left
    auto hr = [&](float x) { return sw - hx(x); };                                         // from the right
    auto hy = [&](float y) { return y * ref; };                                            // from the top
    auto hb = [&](float y) { return sh - y * ref; };                                       // from the bottom
    const float cashShown = 1.0f;   // the cash stays on screen
    const float fk = 0.77f * d.scale;

    // ---- radar, bottom left, flat 270x172 box with the vitals strip under it ----
    if (d.radarOn) {
        const float l = hx(96.0f), r2 = hx(366.0f), t = hb(244.0f), b = hb(72.0f);
        g.DrawRect(l - 3 * ref, t - 3 * ref, r2 + 3 * ref, b + 3 * ref, 0x96000000);
        DrawRadar(g, d, (l + r2) * 0.5f, (t + b) * 0.5f, (r2 - l) * 0.5f, (b - t) * 0.5f, 0.0f, 150.0f, 1.0f, true);
        const float by = hb(68.0f), bh = 9.0f * ref;
        const bool lowHp = d.health < 30.0f;
        const uint32_t hc = lowHp ? ((int)(m_time * 6.0f) & 1 ? 0xCDE04040 : 0xCDB03030) : 0xCD6CC46A;
        Bar(g, hx(96.0f), by, 134.0f * ref, bh, d.health * 100.0f / d.maxHealth, hc, true);
        Bar(g, hx(233.0f), by, (d.showBreath ? 65.0f : 133.0f) * ref, bh, d.armor, 0xCD5E9FE0, true);
        if (d.showBreath) Bar(g, hx(301.0f), by, 65.0f * ref, bh, d.breath * 100.0f, 0xCDB8E4FF, true);
        if (d.stamina < 0.995f) Bar(g, hx(96.0f), by - 8.0f * ref, 270.0f * ref, 4.0f * ref, d.stamina * 100.0f, 0xCDF2C94C, true);
    } else {
        const float bh = 9.0f * ref;
        Bar(g, hx(96.0f), hb(68.0f), 134.0f * ref, bh, d.health * 100.0f / d.maxHealth, 0xCD6CC46A, true);
        if (d.armor > 1.0f) Bar(g, hx(233.0f), hb(68.0f), 133.0f * ref, bh, d.armor, 0xCD5E9FE0, true);
    }

    // ---- zone name, bottom right ----
    if (!d.zone.empty()) {
        float a = 1.0f;
        if (m_zoneTimer < 1.0f) a = m_zoneTimer;
        else if (m_zoneTimer > 4.0f) a = std::max(0.0f, 5.0f - m_zoneTimer);
        if (a > 0.0f)
            Text(g, FeFont::SUBTITLES, hr(96.0f), hb(125.0f), Upper(d.zone), 1.1f * 0.77f * d.scale, 2.3f * 0.77f * d.scale, Alpha(0xFFFFFFFF, a), FeFont::RIGHT, 0);
    }

    // ---- top right: wanted stars, cash (+ change), ammo ----
    float shift = 0.0f;
    const float k = std::min(1.0f, 12.0f * d.dt);
    if (d.wanted > 0 || m_wantedFade > 0.0f) {
        float sp = 0.0f;
        for (int i = 0; i < 6; i++) {
            const bool lit = i < d.wanted;
            const bool flash = d.wanted > 0 && m_wantedFade > 0.0f && ((int)(m_time * 6.0f) & 1);
            const uint32_t col = lit ? (flash ? 0xFFFFFFFF : 0xFFF2C94C) : 0x50FFFFFF;
            Text(g, FeFont::GOTHIC, hr(96.0f + 32.0f + sp) + 32.0f * ref, hy(54.0f) - 6.0f * ref, "]", 0.7f * 0.77f * d.scale, 1.4f * 0.77f * d.scale,
                 col, FeFont::RIGHT, lit ? 1 : 0);
            sp += 30.0f;
        }
        shift += 35.0f * ref;
    }
    const float cashH = 62.0f * ref;
    m_cashY += (((cashShown > 0.01f) ? cashH + 2.0f * ref + (m_diffTimer > 0.0f ? cashH * 0.86f : 0.0f) : 0.0f) - m_cashY) * k;
    if (cashShown > 0.01f) {
        Text(g, FeFont::PRICEDOWN, hr(96.0f), hy(54.0f) + shift, MoneyText(d.money, true), 1.02f * fk, 2.1f * fk,
             Alpha(d.money < 0 ? 0xFFE05050 : 0xFFFFFFFF, cashShown), FeFont::RIGHT, 2);
        if (m_diffTimer > 0.0f && m_moneyDiff != 0) {
            char df[24];
            std::snprintf(df, sizeof(df), "%s$%d", m_moneyDiff > 0 ? "+" : "-", std::abs(m_moneyDiff));
            Text(g, FeFont::PRICEDOWN, hr(96.0f), hy(56.0f) + shift + cashH, df, 1.02f * fk * 0.86f, 2.1f * fk * 0.86f,
                 Alpha(m_moneyDiff > 0 ? 0xFF6CC46A : 0xFFE05050, std::min(1.0f, m_diffTimer * 2.0f) * cashShown), FeFont::RIGHT, 2);
        }
    }
    shift += m_cashY;

    // ---- ammo: total white, clip grey to its right ----
    if (d.armed && !d.hideAmmo && (m_ammoTimer > 0.0f || d.reloading)) {
        const float a = std::min(1.0f, m_ammoTimer * 2.0f);
        const float sx = 0.86f * fk, sy = 1.8f * fk, ey = hy(54.0f) + shift;
        char big[16], small[16];
        if (d.clipSize > 1) {
            std::snprintf(big, sizeof(big), "%d", d.ammoTotal);
            std::snprintf(small, sizeof(small), "%d", d.ammoClip);
            m_font.SetStyle(FeFont::PRICEDOWN);
            m_font.SetScale(m_font.Sx(sx), m_font.Sy(sy));
            const float cw = m_font.StringWidth(small);
            Text(g, FeFont::PRICEDOWN, hr(96.0f), ey, small, sx, sy, Alpha(d.ammoClip == 0 ? 0xFFE05050 : 0xFF9AA4AE, a), FeFont::RIGHT, 2);
            Text(g, FeFont::PRICEDOWN, hr(96.0f + 12.0f) - cw, ey, big, sx, sy, Alpha(0xFFFFFFFF, a), FeFont::RIGHT, 2);
        } else {
            std::snprintf(big, sizeof(big), "%d", d.ammoClip + d.ammoTotal);
            Text(g, FeFont::PRICEDOWN, hr(96.0f), ey, big, sx, sy, Alpha(0xFFFFFFFF, a), FeFont::RIGHT, 2);
        }
        if (d.reloading)
            Text(g, FeFont::SUBTITLES, hr(96.0f), ey + 34.0f * ref, "RELOADING", 0.34f * 0.77f * d.scale, 0.8f * 0.77f * d.scale, Alpha(0xFFF2C94C, a), FeFont::RIGHT, 0);
    }

    // ---- crosshair and hit marker ----
    if (d.aiming && d.armed && d.crosshairDot && !d.lockOn) {
        const float cx = sw * d.crossX, cyy = sh * d.crossY;
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
