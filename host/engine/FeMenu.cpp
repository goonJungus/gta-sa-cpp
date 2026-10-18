// FeMenu.cpp - see FeMenu.h. Numbers are retail's, in 640x448 design units
// (CMenuManager::StretchX/StretchY scale them to the backbuffer).

#ifndef NOMINMAX
#define NOMINMAX   // windows.h (via d3d9) would turn std::min/max into macros
#endif
#include "FeMenu.h"

#include "D3DRenderer.h"
#include "FeScreens.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// CRGBA values used by DrawStandardMenus / DisplaySlider / CHudColours.
const uint32_t kSelected = 0xFFACCBF1;   // (172,203,241) = HUD_COLOUR_LIGHT_BLUE
const uint32_t kNormal   = 0xFF4A5A6B;   // (74,90,107)
const uint32_t kDisabled = 0xFF0E1E2F;   // (14,30,47): retail's greyed-out entry
const uint32_t kBlack    = 0xFF000000;

// Frame limiter choices (retail only has on = 30 fps / off).
const int kFrameLimits[] = {60, 120, 144, 0};

// Draw distance range and step (CMenuManager::ProcessPCMenuOptions).
const float kDrawDistMin = 0.925f, kDrawDistMax = 1.8f, kDrawDistStep = 7.0f / 128.0f;

// Retail screens shown by this port (the rest of the table is unreachable
// from the pause menu or needs the save system).
bool ScreenShown(int s) {
    switch (s) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 27: case 28:
    case 33: case 35: case 36: case 41:
        return true;
    }
    return s >= 43;   // ours
}

IDirect3DTexture9* LoadSprite(D3DRenderer& r, const std::vector<TxdTexture>& txd, const char* name) {
    for (const TxdTexture& t : txd)
        if (TxdLoader::KeyOf(t.name) == name)
            return r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(),
                                   (uint32_t)t.mip0.size(), &t.extraMips);
    return nullptr;
}

}  // namespace

bool FeMenu::Init(D3DRenderer& r, const std::string& gameDir) {
    m_r = &r;
    m_text.Load(gameDir, "american.gxt");
    m_font.Init(r, gameDir);
    // CMenuManager::LoadAllTextures: fronten1 (arrow + radio logos),
    // fronten2 (backgrounds + map), fronten_pc (mouse pointers).
    std::vector<TxdTexture> fe1 = TxdLoader::Load(gameDir + "\\models\\fronten1.txd");
    std::vector<TxdTexture> fe2 = TxdLoader::Load(gameDir + "\\models\\fronten2.txd");
    std::vector<TxdTexture> fepc = TxdLoader::Load(gameDir + "\\models\\fronten_pc.txd");
    m_arrow = LoadSprite(r, fe1, "arrow");
    m_mouse = LoadSprite(r, fepc, "mouse");
    for (int i = 0; i < 7; i++) {
        char name[8];
        std::snprintf(name, sizeof(name), "back%d", i + 2);
        m_back[i] = LoadSprite(r, fe2, name);
    }
    BuildScreens();
    return m_font.Ready();
}

void FeMenu::Shutdown() {
    m_font.Shutdown();
    if (!m_r) return;
    auto drop = [&](IDirect3DTexture9*& t) { if (t) { m_r->DestroyTexture(t); t = nullptr; } };
    drop(m_arrow);
    drop(m_mouse);
    for (auto*& t : m_back) drop(t);
}


FeMenu::Entry FeMenu::MakeToggle(const char* name, bool* b, bool rewrite) {
    Entry e; e.action = A_TOGGLE; e.name = name; e.type = 12; e.align = 1; e.b = b; e.needRewrite = rewrite; e.live = true;
    return e;
}
FeMenu::Entry FeMenu::MakeChoice(const char* name, int* i, std::vector<std::string> ch, bool rewrite) {
    Entry e; e.action = A_CHOICE; e.name = name; e.type = 12; e.align = 1; e.i = i; e.choices = std::move(ch);
    e.needRewrite = rewrite; e.live = true;
    return e;
}
FeMenu::Entry FeMenu::MakeSlider(const char* name, float* f, float lo, float hi, float step, bool rewrite) {
    Entry e; e.action = A_SLIDER; e.name = name; e.type = 12; e.align = 1; e.f = f; e.lo = lo; e.hi = hi; e.step = step;
    e.needRewrite = rewrite; e.live = true;
    return e;
}
FeMenu::Entry FeMenu::MakeGoto(const char* name, int target) {
    Entry e; e.action = A_GOTO; e.name = name; e.type = 11; e.target = target; e.align = 3; e.live = true;
    return e;
}
void FeMenu::AddScreen(const char* title, int prev, std::vector<Entry> items) {
    Screen sc;
    sc.title = title; sc.prev = prev; sc.start = 0;
    sc.items = std::move(items);
    if (!sc.items.empty()) { sc.items[0].x = 57; sc.items[0].y = 127; }
    Entry back;
    back.action = A_BACK; back.name = "FEDS_TB"; back.type = 11; back.align = 3; back.x = 320; back.y = 380; back.live = true;
    sc.items.push_back(back);
    m_screens.push_back(std::move(sc));
}

void FeMenu::BuildScreens() {
    m_screens.assign(kFeScreenCount, Screen());
    for (int s = 0; s < kFeScreenCount; s++) {
        const FeScreenDef& d = kFeScreens[s];
        Screen& sc = m_screens[s];
        sc.title = d.title;
        sc.prev = d.prev;
        sc.start = d.start;
        for (const FeEntryDef& e : d.items) {
            Entry en;
            en.action = e.action;
            en.name = e.name ? e.name : "";
            en.type = e.type; en.target = e.target;
            en.x = e.x; en.y = e.y; en.align = e.align;
            sc.items.push_back(en);
        }
    }
    // Ours: VSync under the frame limiter on Display Setup > Advanced. The
    // entries below it have no fixed position, so they shift down one row.
    {
        std::vector<Entry>& it = m_screens[27].items;
        Entry v;
        v.action = A_VSYNC; v.name = "VSYNC"; v.type = 12; v.target = 27; v.align = 1;
        it.insert(it.begin() + 2, v);
        if (it.size() > 12) it.resize(12);
    }

    // ---- Ours: extra screens and entries (index 42 is retail's "pause" alias) ----
    {
        Screen alias;
        m_screens.push_back(alias);   // 42
        // 43: Gameplay (rewrite master switch and its extras)
        Entry rw; rw.action = A_REWRITE; rw.name = "REWRITE MODE"; rw.type = 12; rw.align = 1; rw.live = true;
        AddScreen("GAMEPLAY", 33, {rw,
            MakeToggle("HIT MARKERS", &m_set.hitMarker, true),
            MakeToggle("DAMAGE FLASH", &m_set.damageFlash, true),
            MakeToggle("CROSSHAIR DOT", &m_set.crosshairDot, true),
            MakeToggle("AUTO RELOAD", &m_set.autoReload, true)});
        // 44: HUD and view (rewrite)
        AddScreen("HUD AND VIEW", 4, {
            MakeToggle("SHOW HUD", &m_set.showHud, false),
            MakeSlider("HUD SCALE", &m_set.hudScale, 0.6f, 1.6f, 0.05f, true),
            MakeSlider("FIELD OF VIEW", &m_set.fov, 50.0f, 110.0f, 2.0f, true)});
        // Options: Gameplay above Back. Pause root: quick rewrite switch above Quit.
        std::vector<Entry>& op = m_screens[33].items;
        op.insert(op.begin() + 4, MakeGoto("GAMEPLAY", 43));
        std::vector<Entry>& ds = m_screens[4].items;
        ds.insert(ds.begin() + 4, MakeGoto("HUD AND VIEW", 44));
        if (ds.size() > 12) ds.resize(12);
        std::vector<Entry>& pa = m_screens[41].items;
        Entry q = rw; q.type = 11; q.align = 3;
        pa.insert(pa.begin() + 6, q);
        // Audio screen sliders: music and sound effects volume.
        for (Entry& e : m_screens[3].items) {
            if (e.action == 28) { e.action = A_SLIDER; e.f = &m_set.musicVol; e.lo = 0; e.hi = 1; e.step = 0.0625f; }
            if (e.action == 29) { e.action = A_SLIDER; e.f = &m_set.sfxVol; e.lo = 0; e.hi = 1; e.step = 0.0625f; }
        }
    }
    for (Screen& sc : m_screens) {
        for (size_t i = 0; i < sc.items.size(); i++) {
            Entry& e = sc.items[i];
            // DrawStandardMenus: an entry without a position goes 30 units
            // (20 for type 9) under the previous one; the first entry (or the
            // second, after a text entry) defaults to (320,130).
            if ((e.x == 0 && e.y == 0) || e.type == 9) {
                if (i == 0 || (i == 1 && sc.items[0].action == A_TEXT)) {
                    e.x = 320; e.y = 130;
                } else {
                    e.x = sc.items[i - 1].x;
                    e.y = sc.items[i - 1].y + (e.type == 9 ? 20 : 30);
                }
            }
            switch (e.action) {
            case A_BACK: case A_RESUME: case A_NO: case A_YES_QUIT:
            case A_FRAME_LIMITER: case A_DRAW_DIST: case A_VSYNC:
            case A_TOGGLE: case A_CHOICE: case A_SLIDER: case A_REWRITE:
                e.live = true;
                break;
            case A_GOTO:
                e.live = ScreenShown(e.target);
                break;
            }
        }
    }
}

void FeMenu::Open(int screen) {
    if (screen < 0 || screen >= (int)m_screens.size() || !ScreenShown(screen)) screen = kPauseScreen;
    m_open = true;
    m_quit = false;
    m_mx = m_my = -1;
    SwitchTo(screen);
}

void FeMenu::Close() { m_open = false; }

void FeMenu::SwitchTo(int screen, int fromChild) {
    if (screen == 42) screen = kPauseScreen;   // retail: "the menu you came from"
    if (screen < 0 || screen >= (int)m_screens.size()) { Close(); return; }
    m_screen = screen;
    const std::vector<Entry>& it = m_screens[screen].items;
    m_sel = -1;
    // Coming back from a child: highlight the entry that leads to it
    // (retail stores that index as the child's "start" entry).
    if (fromChild >= 0) {
        for (size_t i = 0; i < it.size(); i++)
            if (Live(it[i]) && it[i].action == A_GOTO && it[i].target == fromChild) { m_sel = (int)i; break; }
        int st = m_screens[fromChild].start;
        if (m_sel < 0 && st >= 0 && st < (int)it.size() && Live(it[st])) m_sel = st;
    }
    if (m_sel < 0) {
        for (size_t i = 0; i < it.size(); i++)
            if (Live(it[i]) && Visible(it[i])) { m_sel = (int)i; break; }
    }
    if (m_sel < 0) m_sel = 0;
}

void FeMenu::Back() {
    m_uiEvent = 3;
    if (m_screen == kPauseScreen) { Close(); return; }
    int prev = m_screens[m_screen].prev;
    if (prev < 0) { Close(); return; }
    SwitchTo(prev, m_screen);
}

void FeMenu::Move(int dir) {
    const std::vector<Entry>& it = m_screens[m_screen].items;
    int n = (int)it.size();
    for (int step = 1; step <= n; step++) {
        int i = ((m_sel + dir * step) % n + n) % n;   // retail wraps around
        if (Live(it[i]) && Visible(it[i])) { if (m_sel != i) m_uiEvent = 1; m_sel = i; return; }
    }
}

void FeMenu::Activate(int dir) {
    std::vector<Entry>& it = m_screens[m_screen].items;
    if (m_sel < 0 || m_sel >= (int)it.size() || !Live(it[m_sel])) return;
    const Entry& e = it[m_sel];
    if (e.action != A_BACK && e.action != A_RESUME) m_uiEvent = (dir == 0) ? 2 : 1;
    switch (e.action) {
    case A_BACK:      if (dir == 0) Back(); break;
    case A_GOTO:      if (dir == 0) SwitchTo(e.target); break;
    case A_RESUME:    if (dir == 0) Close(); break;
    case A_NO:        if (dir == 0) SwitchTo(e.target, m_screen); break;
    case A_YES_QUIT:  if (dir == 0) m_quit = true; break;
    case A_FRAME_LIMITER: {
        const int n = (int)(sizeof(kFrameLimits) / sizeof(kFrameLimits[0]));
        int cur = 0;
        for (int i = 0; i < n; i++) if (kFrameLimits[i] == m_set.frameLimit) cur = i;
        cur = (cur + (dir < 0 ? n - 1 : 1)) % n;
        m_set.frameLimit = kFrameLimits[cur];
        m_changed = true;
        break;
    }
    case A_VSYNC:
        m_set.vsync = !m_set.vsync;
        m_changed = true;
        break;
    case A_TOGGLE:
        if (e.b) { *e.b = !*e.b; m_changed = true; }
        break;
    case A_CHOICE:
        if (e.i && !e.choices.empty()) {
            const int n = (int)e.choices.size();
            *e.i = (*e.i + (dir < 0 ? n - 1 : 1)) % n;
            m_changed = true;
        }
        break;
    case A_SLIDER:
        if (e.f && dir != 0) {
            *e.f = std::min(e.hi, std::max(e.lo, *e.f + dir * e.step * (e.hi - e.lo > 2.0f ? 1.0f : 1.0f)));
            m_changed = true;
        }
        break;
    case A_REWRITE:
        m_set.movementStyle = m_set.cameraStyle = m_set.movementStyle ? 0 : 1;
        m_changed = true;
        break;
    case A_DRAW_DIST:
        if (dir == 0) break;
        m_set.drawDist = std::min(kDrawDistMax, std::max(kDrawDistMin, m_set.drawDist + dir * kDrawDistStep));
        m_changed = true;
        break;
    }
}

void FeMenu::OnKey(int vk) {
    if (!m_open) return;
    switch (vk) {
    case VK_UP:     Move(-1); break;
    case VK_DOWN:   Move(+1); break;
    case VK_LEFT:   Activate(-1); break;
    case VK_RIGHT:  Activate(+1); break;
    case VK_RETURN: Activate(0); break;
    case VK_ESCAPE: case VK_BACK: Back(); break;
    case 'P':       Close(); break;   // dev habit: P toggles the pause menu
    }
}

void FeMenu::OnMouse(float x, float y, bool click, bool held) {
    if (!m_open) return;
    x -= m_ox; y -= m_oy;   // into the centred layout
    m_mouseMoved = (x != m_mx || y != m_my);
    m_mx = x; m_my = y;
    std::vector<Entry>& it = m_screens[m_screen].items;
    // Hover selects (retail does the same once the mouse moves).
    int hover = -1;
    for (size_t i = 0; i < it.size(); i++) {
        const Entry& e = it[i];
        if (Live(e) && Visible(e) && x >= e.hx0 && x <= e.hx1 && y >= e.hy0 && y <= e.hy1) hover = (int)i;
    }
    if (hover >= 0 && (m_mouseMoved || click)) { if (m_sel != hover) m_uiEvent = 1; m_sel = hover; }
    // Draw distance: dragging on the slider sets it directly.
    bool onSlider = x >= Sx(500) && x <= Sx(600) + Sx(3) && y >= Sy(120) && y <= Sy(150);
    if (held && onSlider && m_sel >= 0 && m_sel < (int)it.size() && it[m_sel].action == A_DRAW_DIST) {
        float v = std::min(1.0f, std::max(0.0f, (x - Sx(500)) / Sx(100)));
        float dd = kDrawDistMin + v * (kDrawDistMax - kDrawDistMin);
        if (dd != m_set.drawDist) { m_set.drawDist = dd; m_changed = true; }
    } else if (held && m_sel >= 0 && m_sel < (int)it.size() && it[m_sel].action == A_SLIDER && it[m_sel].f &&
               x >= Sx(500) && x <= Sx(603) && y >= it[m_sel].hy0 - Sy(6) && y <= it[m_sel].hy1 + Sy(6)) {
        Entry& se = it[m_sel];
        float v = std::min(1.0f, std::max(0.0f, (x - Sx(500)) / Sx(100)));
        float nv = se.lo + v * (se.hi - se.lo);
        if (nv != *se.f) { *se.f = nv; m_changed = true; }
    } else if (click && hover >= 0) {
        Activate(0);
    }
}

std::string FeMenu::ValueText(const Entry& e) const {
    switch (e.action) {
    case A_FRAME_LIMITER:
        if (m_set.frameLimit <= 0) return m_text.Get("FEM_OFF");
        return std::to_string(m_set.frameLimit) + " FPS";
    case A_SLIDER: return std::string();
    case A_TOGGLE:
        return e.b ? std::string(*e.b ? "ON" : "OFF") : std::string();
    case A_CHOICE:
        return (e.i && *e.i >= 0 && *e.i < (int)e.choices.size()) ? e.choices[*e.i] : std::string();
    case A_REWRITE:
        return m_set.movementStyle == 1 ? "ON" : "OFF";
    case A_VSYNC:
        return m_text.Get(m_set.vsync ? "FEM_ON" : "FEM_OFF");
    }
    return std::string();
}

void FeMenu::DrawSlider(Fe2d& g, float value, float y125) {
    // CMenuManager::DisplaySlider(StretchX(500), StretchY(125), StretchY(4),
    // StretchY(20), StretchX(100), value, StretchX(3)): 16 bars growing from
    // h1 to h2, lit while value is past the bar's midpoint, with a shadow.
    const float x = Sx(500), y = Sy(y125), h1 = Sy(4), h2 = Sy(20), len = Sx(100), w = Sx(3);
    const float maxH = std::max(h1, h2);
    for (int i = 0; i < 16; i++) {
        float bx = len * (float)i * 0.0625f + x;
        float bh = ((float)i * h2 + (float)(16 - i) * h1) * 0.0625f;
        uint32_t col = (value <= (float)i * 0.0625f + 0.03125f) ? kNormal : kSelected;
        g.DrawRect(bx + Sx(2), y + maxH - bh + Sy(2), bx + w + Sx(2), y + maxH + Sy(2), 0xC8000000);
        g.DrawRect(bx, y + maxH - bh, bx + w, y + maxH, col);
    }
}

void FeMenu::DrawDevPanel(Fe2d& g) {
    if (devKeys.empty()) return;
    // Not retail: the dev/test key list, under the pause artwork.
    const float x0 = m_w - Sx(215), y0 = Sy(212);
    m_font.SetStyle(FeFont::SUBTITLES);
    m_font.SetAlign(FeFont::LEFT);
    m_font.SetWrapWidth(0);
    m_font.SetScale(Sx(0.33f), Sy(0.72f));
    m_font.SetEdge(0);
    float lh = m_font.LineHeight();
    m_font.SetColor(kSelected);
    m_font.Print(g, x0, y0, "Dev keys");
    float y = y0 + lh * 1.3f;
    for (const auto& kv : devKeys) {
        m_font.SetColor(kSelected);
        m_font.Print(g, x0, y, kv.first);
        m_font.SetColor(0xFFC8C8C8);
        m_font.Print(g, x0 + Sx(42), y, kv.second);
        y += lh;
    }
}

void FeMenu::Draw(D3DRenderer& r) {
    if (!m_open || !r.GetDevice()) return;
    // The 640x448 layout keeps its proportions and sits centred: any
    // resolution or aspect ratio (4:3 to ultrawide) gets the same menu with
    // black side bars instead of a stretched one.
    const float sw = (float)r.Width(), sh = (float)r.Height();
    const float s = std::min(sw / 640.0f, sh / 448.0f);
    m_w = 640.0f * s;
    m_h = 448.0f * s;
    m_ox = std::floor((sw - m_w) * 0.5f);
    m_oy = std::floor((sh - m_h) * 0.5f);
    m_font.SetScreen(m_w, m_h);
    Fe2d& g = m_2d;
    g.Begin(r.GetDevice());

    // ---- CMenuManager::DrawBackground ----
    g.DrawRect(0, 0, sw, sh, kBlack);
    g.SetOrigin(m_ox, m_oy);
    int bg = -1;          // fronten2 back2..back8 -> 0..6
    float bw = 256, bh = 256;
    bool centred = false;
    switch (m_screen) {
    case 0: case 28: case 35:           bg = 2; break;   // back4
    case 26: case 33: case 34:          bg = 1; break;   // back3
    case 1: case 2: case 10: case 16:   bg = 0; break;   // back2
    case 3:                             bg = 3; centred = true; break;   // back5
    case 4: case 27:                    bg = 5; break;   // back7
    case 36: case 39:                   bg = 4; break;   // back6
    case 5: case 38:                    bg = -1; break;  // map / controls: none
    default:                            bg = 6; bw = 300; bh = 200; break;   // back8
    }
    if (bg >= 0 && m_back[bg]) {
        float x0 = centred ? m_w * 0.5f - Sx(128) : m_w - Sx(bw);
        float x1 = centred ? m_w * 0.5f + Sx(128) : m_w;
        g.DrawTex(m_back[bg], x0, 0, x1, Sy(bh), 0xFFFFFFFF);
    }
    if (!m_font.Ready()) { g.End(); return; }

    Screen& sc = m_screens[m_screen];
    // ---- Title: gothic, light blue, black edge ----
    if (!sc.title.empty()) {
        m_font.SetStyle(FeFont::GOTHIC);
        m_font.SetAlign(FeFont::LEFT);
        m_font.SetWrapWidth(0);
        m_font.SetScale(Sx(1.3f), Sy(2.1f));
        m_font.SetEdge(1);
        m_font.SetColor(kSelected);
        m_font.SetDropColor(kBlack);
        m_font.Print(g, Sx(40), Sy(28), m_text.Get(sc.title.c_str()));
    }
    // ---- Description (entry 0 with the text action) ----
    bool anyPorted = false;
    for (const Entry& e : sc.items)
        if (Live(e) && e.action != A_BACK && Visible(e)) anyPorted = true;
    std::string desc;
    if (!sc.items.empty() && sc.items[0].action == A_TEXT && !sc.items[0].name.empty())
        desc = m_text.Get(sc.items[0].name.c_str());
    else if (!anyPorted)
        desc = "This page is not ported yet.";
    if (!desc.empty()) {
        m_font.SetStyle(FeFont::SUBTITLES);
        m_font.SetAlign(FeFont::LEFT);
        m_font.SetWrapWidth(m_w - 40.0f - Sx(60));
        m_font.SetScale(Sx(0.49f), Sy(1.2f));
        m_font.SetEdge(2);
        m_font.SetDropColor(kBlack);
        m_font.SetColor(kNormal);
        m_font.Print(g, Sx(60), Sy(97), desc);
        m_font.SetWrapWidth(0);
    }

    // ---- Entries ----
    for (size_t i = 0; i < sc.items.size(); i++) {
        Entry& e = sc.items[i];
        e.hx0 = e.hy0 = e.hx1 = e.hy1 = 0;
        if (!Visible(e)) continue;
        bool sel = (int)i == m_sel;
        m_font.SetStyle(FeFont::MENU);
        m_font.SetScale(Sx(0.7f), Sy(1.0f));
        m_font.SetEdge(2);
        m_font.SetDropColor(kBlack);
        uint32_t col = Live(e) ? (sel ? kSelected : kNormal) : (e.needRewrite && e.live ? 0xFF3A4A5C : kDisabled);
        m_font.SetColor(col);
        FeFont::Align al = e.align == 1 ? FeFont::LEFT : e.align == 2 ? FeFont::RIGHT : FeFont::CENTER;
        m_font.SetAlign(al);
        std::string label = m_text.Get(e.name.c_str());
        std::string value = e.live ? ValueText(e) : std::string();
        if (!value.empty() && e.align == 3) {   // centred rows carry the value in the label
            label += ": " + value;
            value.clear();
        }
        float px = Sx((float)e.x), py = Sy((float)e.y);
        m_font.Print(g, px, py, label);
        float tw = m_font.StringWidth(label);        float lx = al == FeFont::LEFT ? px : al == FeFont::RIGHT ? px - tw : px - tw * 0.5f;
        e.hx0 = lx; e.hx1 = lx + tw;
        e.hy0 = py; e.hy1 = py + m_font.LineHeight();

        if (!value.empty()) {
            m_font.SetEdge(1);
            m_font.SetAlign(FeFont::RIGHT);
            m_font.Print(g, m_w - Sx(40), py, value);
            e.hx1 = m_w - Sx(40);    // the value is part of the row
        }
        if (Live(e) && e.action == A_SLIDER && e.f) {
            DrawSlider(g, (*e.f - e.lo) / (e.hi - e.lo), (float)e.y - 2.0f);
            e.hx1 = std::max(e.hx1, Sx(600));
        }
        if (Live(e) && e.action == A_DRAW_DIST) {
            DrawSlider(g, (m_set.drawDist - kDrawDistMin) * 1.1428572f, 125.0f);
            e.hx1 = std::max(e.hx1, Sx(600));
        }
        // Selection arrow (fronten1 "arrow"), 40 units left of the label.
        if (sel && m_arrow) {
            float ax = lx - Sx(40);
            g.DrawTex(m_arrow, ax, py - Sx(5), ax + Sx(32), py + Sx(47), 0xFFFFFFFF);
        }
    }

    if (m_screen == kPauseScreen) DrawDevPanel(g);

    // ---- Mouse pointer: grey shadow, then the pointer ----
    if (m_mouse && m_mx >= 0 && m_my >= 0) {
        g.DrawTex(m_mouse, m_mx + Sx(6), m_my + Sy(3), m_mx + Sx(24), m_my + Sy(21), 0x32646464);
        g.DrawTex(m_mouse, m_mx, m_my, m_mx + Sx(18), m_my + Sy(18), 0xFFFFFFFF);
    }
    g.End();
}
