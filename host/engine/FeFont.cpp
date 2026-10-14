// FeFont.cpp - see FeFont.h. Behaviour follows CFont::PrintChar,
// FindSubFontCharacter, LoadFontValues and the edge/shadow passes in
// CFont's string buffer (uses_ParseToken @ 0x719b40).

#include "FeFont.h"

#include "D3DRenderer.h"
#include "TxdLoader.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// CFont::FindSubFontCharacter (letterId = char - 0x20).
int FindSubFontCharacter(int id, int style) {
    if (style == 1) {   // pricedown
        if (id == 0x1a) return 0x9a;
        if (id > 7 && id < 10) return id + 'V';
        if (id == 0x04) return ']';
        if (id == 0x07) return 0xce;
        if (id == 0x0e) return 0xcf;
        if (id == 0x01) return 0xd0;
    }
    if (id == 0x8f) return 0xcd;
    if (id == 0x1f) return '[';
    if (id == 0x06) return '\n';
    if (id == '>') return ' ';
    if (id > 0x0f && id < 0x1a) return id + 0x80;
    if (id > 0x20 && id < 0x3b) return id + 'z';
    if (id > 0x40 && id < 0x5b) return id + 'Z';
    if (id > 0x5f && id < 0x77) return id + 'U';
    if (id > 0x76 && id < 0x8d) return id + '>';
    if (id > 0x8c && id < 0x8f) return 0xcc;
    return id;
}

// Split on ~n~ and drop other ~x~ formatting tokens (colour codes etc.).
std::vector<std::string> SplitTokens(const std::string& s) {
    std::vector<std::string> lines(1);
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '~') {
            size_t j = s.find('~', i + 1);
            if (j == std::string::npos) break;
            if (j == i + 2 && (s[i + 1] == 'n' || s[i + 1] == 'N')) lines.emplace_back();
            i = j;
            continue;
        }
        lines.back() += s[i];
    }
    return lines;
}

}  // namespace

bool FeFont::Init(D3DRenderer& r, const std::string& gameDir) {
    m_r = &r;
    // Glyph sheets (CFont::Initialise: font2 -> sprite 0, font1 -> sprite 1).
    std::vector<TxdTexture> tex = TxdLoader::Load(gameDir + "\\models\\fonts.txd");
    for (auto& t : tex) {
        std::string k = TxdLoader::KeyOf(t.name);
        int slot = (k == "font2") ? 0 : (k == "font1") ? 1 : -1;
        if (slot < 0) continue;
        m_tex[slot] = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(),
                                      (uint32_t)t.mip0.size(), &t.extraMips);
    }
    // Proportional widths (CFont::LoadFontValues).
    FILE* f = std::fopen((gameDir + "\\data\\fonts.dat").c_str(), "r");
    if (f) {
        char line[256];
        int font = 0;
        auto next = [&](char* out) -> bool {
            while (std::fgets(out, 256, f)) {
                const char* p = out;
                while (*p == ' ' || *p == '\t') p++;
                if (*p && *p != '#' && *p != '\n' && *p != '\r') return true;
            }
            return false;
        };
        while (next(line)) {
            char tag[64] = {};
            std::sscanf(line, "%63s", tag);
            if (!std::strcmp(tag, "[FONT_ID]")) {
                if (next(line)) std::sscanf(line, "%d", &font);
                if (font < 0 || font > 1) font = 0;
            } else if (!std::strcmp(tag, "[PROP]")) {
                for (int row = 0; row < 26 && next(line); row++) {
                    int v[8] = {};
                    std::sscanf(line, "%d %d %d %d %d %d %d %d",
                                &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
                    for (int k = 0; k < 8; k++) m_prop[font][row * 8 + k] = (uint8_t)v[k];
                }
            } else if (!std::strcmp(tag, "[UNPROP]")) {
                int v = 27;
                if (next(line)) std::sscanf(line, "%d", &v);
                m_unprop[font] = (uint8_t)v;
            }
        }
        std::fclose(f);
    }
    return Ready();
}

void FeFont::Shutdown() {
    for (auto*& t : m_tex)
        if (t) { m_r->DestroyTexture(t); t = nullptr; }
}

void FeFont::SetStyle(Style s) {
    // CFont::SetFontStyle: menu draws from texture 0 with sub-font 2;
    // pricedown from texture 1 with sub-font 1; the others are plain.
    if (s == MENU)           { m_texId = 0; m_style = 2; }
    else if (s == PRICEDOWN) { m_texId = 1; m_style = 1; }
    else                     { m_texId = (int)s; m_style = 0; }
}

int FeFont::GlyphId(unsigned char c) const {
    int id = (uint8_t)(c - 0x20);
    if (m_style != 0) id = FindSubFontCharacter(id, m_style);
    return id;
}

float FeFont::Advance(int id) const {
    // CFont::GetCharacterSize: (prop width + outline size) * scale.x
    int w = (id >= 0 && id < 210) ? m_prop[m_texId][id] : m_unprop[m_texId];
    return (float)(w + m_edge) * m_scaleX;
}

float FeFont::StringWidth(const std::string& text) const {
    // Keep the lines alive: ranging over SplitTokens(text)[0] would iterate a
    // destroyed temporary (no lifetime extension through operator[]).
    const std::vector<std::string> lines = SplitTokens(text);
    float w = 0;
    for (unsigned char c : lines[0]) w += Advance(GlyphId(c));
    return w;
}

void FeFont::DrawRun(Fe2d& g, float x, float y, const std::string& line, uint32_t argb) {
    for (unsigned char c : line) {
        int id = GlyphId(c);
        // CFont::PrintChar skips id 0 (space) and '?' (0x3f); it still advances.
        if (id != 0 && id != '?') {
            float u0 = (float)(id & 0xf) * 0.0625f;
            float v0 = (float)(id >> 4) * 0.078125f;
            float x1 = x + m_scaleX * 32.0f;
            if (id < 0xc0) {
                g.QueueTex(x, y, x1, y + m_scaleY * 20.0f, argb,
                           u0, v0 + 0.0021f, u0 + 0.0625f - 0.001f, v0 + 0.078125f - 0.0021f);
            } else {
                g.QueueTex(x, y, x1, y + m_scaleY * 16.0f, argb,
                           u0, v0 + 0.0021f, u0 + 0.0625f - 0.001f, v0 + 0.078125f - 0.016f);
            }
        }
        x += Advance(id);
    }
}

int FeFont::Print(Fe2d& g, float x, float y, const std::string& text) {
    if (!Ready() || text.empty() || text[0] == '*') return 0;
    // Word-wrap each ~n~ line to m_wrap pixels.
    std::vector<std::string> lines;
    for (const std::string& raw : SplitTokens(text)) {
        if (m_wrap <= 0) { lines.push_back(raw); continue; }
        std::string cur, word;
        auto flushWord = [&]() {
            std::string trial = cur.empty() ? word : cur + " " + word;
            if (!cur.empty() && StringWidth(trial) > m_wrap) { lines.push_back(cur); cur = word; }
            else cur = trial;
            word.clear();
        };
        for (char ch : raw) { if (ch == ' ') flushWord(); else word += ch; }
        flushWord();
        lines.push_back(cur);
    }
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& ln = lines[i];
        float lx = x;
        if (m_align == CENTER) lx -= StringWidth(ln) * 0.5f;
        else if (m_align == RIGHT) lx -= StringWidth(ln);
        float ly = y + LineHeight() * (float)i;
        if (m_edge > 0) {
            float ex = Sx((float)m_edge), ey = Sy((float)m_edge);
            const float off[8][2] = {{ex, -ey}, {-ex, -ey}, {ex, ey}, {-ex, ey},
                                     {ex, 0}, {-ex, 0}, {0, ey}, {0, -ey}};
            for (auto& o : off) DrawRun(g, lx + o[0], ly + o[1], ln, m_dropColor);
        } else if (m_shadow > 0) {
            DrawRun(g, lx + Sx((float)m_shadow), ly + Sy((float)m_shadow), ln, m_dropColor);
        }
        DrawRun(g, lx, ly, ln, m_color);
        g.Flush(m_tex[m_texId]);
    }
    return (int)lines.size();
}
