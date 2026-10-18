#pragma once
// FeText - GXT text table (port of CText::Load / CText::Get + CKeyGen).
//
// SA's american.gxt: 4-byte header (version, char bits), then chunks
// {char tag[4]; uint32 size;}: TABL (table directory), then the MAIN table's
// TKEY (sorted {uint32 dataOffset, uint32 keyHash}) and TDAT (8-bit strings).
// Lookup hashes the upper-cased key (CKeyGen::GetUppercaseKey: CRC32 table,
// init 0xFFFFFFFF, no final xor) and binary-searches TKEY.

#include <cstdint>
#include <string>
#include <vector>

class FeText {
public:
    // Load <gameDir>\text\american.gxt (main table only).
    bool Load(const std::string& gameDir, const char* file = "american.gxt");
    bool Loaded() const { return !m_keys.empty(); }

    // Text for a GXT key. Missing keys return the key itself (retail shows
    // an error string) so an unported label is obvious, not blank.
    std::string Get(const char* key) const;

    static uint32_t UppercaseKey(const char* str);  // CKeyGen::GetUppercaseKey

private:
    std::string Extra(const char* key) const;
    int m_lang = 0;               // 0 en, 1 fr, 2 de, 3 it, 4 es
    struct Key { uint32_t offset, hash; };
    std::vector<Key> m_keys;      // sorted by hash (as stored in the file)
    std::vector<char> m_data;     // TDAT
};
