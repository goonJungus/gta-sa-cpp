#include "Cheat.h"

#include <atomic>
#include <cstring>

namespace {
constexpr int kStringSize = 30;   // CCheat::CHEAT_STRING_SIZE
constexpr int kMinHash = 6;       // CCheat::CHEAT_MIN_HASH_SIZE

// CCheat::m_aCheatHashKeys (gta_sa.exe 0x8a5b78..): row = eCheats. A zero row has no typable code.
constexpr uint32_t kHash[Cheat::kCount] = {
    0xde4b237d, 0xb22a28d1, 0x5a783fae, 0xeeccea2b,
    0x42af1e28, 0x555fc201, 0x2a845345, 0xe1ef01ea,
    0x771b83fc, 0x5bf12848, 0x44453a17, 0xfcff1d08,
    0xb69e8532, 0x8b828076, 0xdd6ed9e9, 0xa290fd8c,
    0x3484b5a7, 0x43db914e, 0xdbc0dd65, 0x00000000,
    0xd08a30fe, 0x37bf1b4e, 0xb5d40866, 0xe63b0d99,
    0x675b8945, 0x4987d5ee, 0x2e8f84e8, 0x1a9aa3d6,
    0xe842f3bc, 0x0d5c6a4e, 0x74d4fcb1, 0xb01d13b8,
    0x66516ebc, 0x4b137e45, 0x00000000, 0x78520e33,
    0x3a577325, 0xd4966d59, 0x5fd1b49d, 0xa7613f99,
    0x1792d871, 0xcbc579df, 0x4fedccff, 0x44b34866,
    0x2ef877db, 0x2781e797, 0x2bc1a045, 0xb2afe368,
    0xfa8dd45b, 0x8ded75bd, 0x1a5526bc, 0xa48a770b,
    0xb07d3b32, 0x80c1e54b, 0x5dad0087, 0x7f80b950,
    0x6c0fa650, 0xf46f2fa4, 0x70164385, 0x00000000,
    0x885d0b50, 0x151bdcb3, 0xadfa640a, 0xe57f96ce,
    0x040cf761, 0xe1b33eb9, 0xfeda77f7, 0x8ca870dd,
    0x9a629401, 0xf53ef5a5, 0xf2aa0c1d, 0xf36345a8,
    0x8990d5e1, 0xb7013b1b, 0xcaec94ee, 0x31f0c3cc,
    0xb3b3e72a, 0xc25cdbff, 0xd5cf4eff, 0x680416b1,
    0xcf5fda18, 0xf01286e9, 0xa841cc0a, 0x31ea09cf,
    0xe958788a, 0x02c83a7c, 0xe49c3ed4, 0x171ba8cc,
    0x86988dae, 0x2bdd2fa1, 0x00000000, 0x00000000,
};

char g_string[kStringSize] = {0};
std::atomic<int> g_hit{-1};
uint32_t g_table[256];
float g_adrenaline = 0.0f;   // seconds left
bool g_ready = false;
}  // namespace

bool Cheat::s_active[Cheat::kCount] = {};
bool Cheat::s_cheated = false;

uint32_t Cheat::Key(const char* s, int n) {
    if (!g_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
            g_table[i] = c;
        }
        g_ready = true;
    }
    uint32_t key = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        key = g_table[(key ^ (uint8_t)c) & 0xFF] ^ (key >> 8);
    }
    return key;
}

void Cheat::Push(char c) {
    for (int i = kStringSize - 2; i >= 1; i--) g_string[i] = g_string[i - 1];
    g_string[0] = c;
    g_string[kStringSize - 1] = 0;
    if ((int)std::strlen(g_string) < kMinHash) return;
    int size = kMinHash;
    for (int i = kMinHash; i < kStringSize; i++, size++) {
        const uint32_t hash = Key(g_string, size);
        for (int k = 0; k < kCount; k++)
            if (kHash[k] && kHash[k] == hash) {
                g_hit = k;
                g_string[0] = 0;
                return;
            }
    }
}

int Cheat::Take() { return g_hit.exchange(-1); }
void Cheat::Trigger(int i) { if (i >= 0 && i < kCount) g_hit = i; }

bool Cheat::MegaPunchOn() { return s_active[MegaPunch]; }
void Cheat::ToggleMegaPunch() { s_active[MegaPunch] = !s_active[MegaPunch]; }
bool Cheat::AdrenalineOn() { return g_adrenaline > 0.0f; }
void Cheat::StartAdrenaline() { g_adrenaline = 20.0f; }
void Cheat::ClearAdrenaline() { g_adrenaline = 0.0f; }
void Cheat::Tick(float dt) { if (g_adrenaline > 0.0f) g_adrenaline = g_adrenaline > dt ? g_adrenaline - dt : 0.0f; }
