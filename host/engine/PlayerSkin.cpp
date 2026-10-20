// CPlayerSkin::GetSkinTexture / CPlayerInfo::SetPlayerSkin: a custom skin picture (skins\<name>.bmp, or
// models\generic\player.bmp for the empty name) that CVisibilityPlugins::RenderPlayerCB lays over every material
// of the player model. No retail install ships a skin picture, so by default nothing is replaced.

#define NOMINMAX
#include "PlayerPed.h"

#include <cstdio>
#include <cstring>

namespace {
bool ReadBmp(const std::string& path, uint32_t& w, uint32_t& h, std::vector<uint8_t>& bgra) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> b;
    uint8_t chunk[4096];
    for (size_t n; (n = std::fread(chunk, 1, sizeof chunk, f)) > 0;) b.insert(b.end(), chunk, chunk + n);
    std::fclose(f);
    auto u32 = [&](size_t o) { return (uint32_t)b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; };
    auto u16 = [&](size_t o) { return (uint32_t)b[o] | b[o + 1] << 8; };
    if (b.size() < 54 || b[0] != 'B' || b[1] != 'M') return false;
    const uint32_t dataOff = u32(10), hdr = u32(14), bpp = u16(28), comp = u32(30);
    const int32_t sw = (int32_t)u32(18), sh = (int32_t)u32(22);
    if (comp != 0 || sw <= 0 || sh == 0 || (bpp != 8 && bpp != 24 && bpp != 32)) return false;
    w = (uint32_t)sw;
    h = (uint32_t)(sh < 0 ? -sh : sh);
    const bool topDown = sh < 0;
    const size_t stride = ((size_t)w * bpp / 8 + 3) & ~(size_t)3;
    if (dataOff + stride * h > b.size()) return false;
    const size_t palOff = 14 + hdr;
    bgra.assign((size_t)w * h * 4, 0xFF);
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t* row = &b[dataOff + stride * (topDown ? y : h - 1 - y)];
        uint8_t* o = &bgra[(size_t)y * w * 4];
        for (uint32_t x = 0; x < w; x++, o += 4) {
            if (bpp == 8) {
                if (palOff + row[x] * 4 + 3 > b.size()) return false;
                std::memcpy(o, &b[palOff + row[x] * 4], 3);
            } else {
                std::memcpy(o, row + x * (bpp / 8), 3);
            }
        }
    }
    return true;
}
}  // namespace

// CPlayerInfo::SetPlayerSkin: "" = models\generic\player.bmp; a name with no picture leaves the model alone.
void PlayerPed::SetSkin(const char* name) {
    if (m_skinTex && m_r) m_r->DestroyTexture(m_skinTex);
    m_skinTex = nullptr;
    m_skinName = name ? name : "";
    if (!m_r) return;
    const bool plain = m_skinName.empty() || m_skinName == "$$\"\"";
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> px;
    if (ReadBmp(m_gameDir + (plain ? "\\models\\generic\\player.bmp" : "\\skins\\" + m_skinName + ".bmp"), w, h, px))
        m_skinTex = m_r->CreateTexture(w, h, D3DFMT_A8R8G8B8, px.data(), (uint32_t)px.size());
    for (Part& p : m_parts)
        if (p.mesh && m_skinTex) p.mesh->texture = m_skinTex;
    if (!m_skinTex && m_ready) BuildOutfit(m_outfit);   // the picture is gone: the clothes texture the parts again
}
