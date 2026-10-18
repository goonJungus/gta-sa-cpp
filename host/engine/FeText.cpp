// FeText.cpp - see FeText.h.

#include "FeText.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

uint32_t FeText::UppercaseKey(const char* str) {
    // Standard reflected CRC-32 table (CKeyGen::keyTable), built once.
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (; *str; str++) {
        uint8_t ch = (uint8_t)std::toupper((unsigned char)*str);
        crc = (crc >> 8) ^ table[(ch ^ crc) & 0xFF];
    }
    return crc;   // no final xor (retail)
}

bool FeText::Load(const std::string& gameDir, const char* file) {
    m_keys.clear();
    m_data.clear();
    m_lang = 0;
    {
        std::string fn = file ? file : "";
        if (fn.rfind("french", 0) == 0) m_lang = 1;
        else if (fn.rfind("german", 0) == 0) m_lang = 2;
        else if (fn.rfind("italian", 0) == 0) m_lang = 3;
        else if (fn.rfind("spanish", 0) == 0) m_lang = 4;
    }
    std::string path = gameDir + "\\text\\" + file;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(size > 0 ? (size_t)size : 0);
    size_t got = buf.empty() ? 0 : std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (got != buf.size() || buf.size() < 4) return false;

    // CText::Load walks chunks after the 4-byte header and keeps the first
    // TKEY / TDAT it meets (the MAIN table follows the TABL directory).
    size_t pos = 4;
    bool haveKeys = false, haveData = false;
    while (pos + 8 <= buf.size() && !(haveKeys && haveData)) {
        char tag[5] = {};
        std::memcpy(tag, &buf[pos], 4);
        uint32_t len;
        std::memcpy(&len, &buf[pos + 4], 4);
        pos += 8;
        if (pos + len > buf.size()) break;
        if (!std::strcmp(tag, "TKEY") && !haveKeys) {
            size_t n = len / 8;
            m_keys.resize(n);
            std::memcpy(m_keys.data(), &buf[pos], n * 8);
            haveKeys = true;
        } else if (!std::strcmp(tag, "TDAT") && !haveData) {
            m_data.assign((const char*)&buf[pos], (const char*)&buf[pos] + len);
            haveData = true;
        }
        pos += len;   // TABL and anything else: skip by size
    }
    if (!haveKeys || !haveData) { m_keys.clear(); m_data.clear(); return false; }
    // Retail binary-searches; keep that guarantee even if a file isn't sorted.
    std::sort(m_keys.begin(), m_keys.end(),
              [](const Key& a, const Key& b) { return a.hash < b.hash; });
    return true;
}

std::string FeText::Get(const char* key) const {
    if (!key || !*key || *key == ' ') return std::string();
    uint32_t h = UppercaseKey(key);
    auto it = std::lower_bound(m_keys.begin(), m_keys.end(), h,
                               [](const Key& k, uint32_t v) { return k.hash < v; });
    if (it == m_keys.end() || it->hash != h || it->offset >= m_data.size())
        return Extra(key);
    return std::string(&m_data[it->offset]);   // NUL-terminated in TDAT
}

// Labels this port adds on top of the retail table. English text is the key; the rest follow the
// language order of kLanguageFiles (English, French, German, Italian, Spanish).
std::string FeText::Extra(const char* key) const {
    static const char* const kTable[][5] = {
        {"REWRITE MODE", "MODE REMASTERISE", "REWRITE-MODUS", "MODALITA REWRITE", "MODO REWRITE"},
        {"GAMEPLAY", "JEU", "SPIELWEISE", "GIOCO", "JUGABILIDAD"},
        {"HUD AND VIEW", "ATH ET VUE", "HUD UND ANSICHT", "HUD E VISTA", "HUD Y VISTA"},
        {"HIT MARKERS", "MARQUEURS DE TOUCHE", "TREFFERMARKER", "SEGNALI DI COLPO", "MARCAS DE IMPACTO"},
        {"DAMAGE FLASH", "FLASH DE DEGATS", "SCHADENSBLITZ", "LAMPO DANNI", "DESTELLO DE DANO"},
        {"CROSSHAIR DOT", "POINT DE VISEE", "ZIELPUNKT", "PUNTO DI MIRA", "PUNTO DE MIRA"},
        {"AUTO RELOAD", "RECHARGE AUTO", "AUTO-NACHLADEN", "RICARICA AUTO", "RECARGA AUTOMATICA"},
        {"SHOW HUD", "AFFICHER ATH", "HUD ANZEIGEN", "MOSTRA HUD", "MOSTRAR HUD"},
        {"HUD SCALE", "TAILLE DE L ATH", "HUD-GROESSE", "SCALA HUD", "ESCALA DEL HUD"},
        {"FIELD OF VIEW", "CHAMP DE VISION", "SICHTFELD", "CAMPO VISIVO", "CAMPO DE VISION"},
        {"DISPLAY MODE", "MODE D AFFICHAGE", "ANZEIGEMODUS", "MODALITA SCHERMO", "MODO DE PANTALLA"},
        {"WINDOW SIZE", "TAILLE DE FENETRE", "FENSTERGROESSE", "DIMENSIONE FINESTRA", "TAMANO DE VENTANA"},
        {"BORDERLESS", "SANS BORDURE", "RANDLOS", "SENZA BORDI", "SIN BORDES"},
        {"WINDOWED", "FENETRE", "FENSTER", "IN FINESTRA", "VENTANA"},
        {"MAP AND BLIPS", "CARTE ET BLIPS", "KARTE UND BLIPS", "MAPPA E BLIP", "MAPA Y MARCADORES"},
        {"VSYNC", "VSYNC", "VSYNC", "VSYNC", "VSYNC"},
        {"ON", "OUI", "AN", "SI", "SI"},
        {"OFF", "NON", "AUS", "NO", "NO"},
        {"FEL_FRE", "FRANCAIS", "FRANCAIS", "FRANCAIS", "FRANCAIS"},
        {"FEL_GER", "DEUTSCH", "DEUTSCH", "DEUTSCH", "DEUTSCH"},
        {"FEL_ITA", "ITALIANO", "ITALIANO", "ITALIANO", "ITALIANO"},
    };
    for (const auto& row : kTable)
        if (std::strcmp(row[0], key) == 0) return row[m_lang];
    return std::string(key);
}
