#include "Cheat.h"

#include <atomic>
#include <cstring>

namespace {
constexpr int kStringSize = 30;   // CCheat::CHEAT_STRING_SIZE
constexpr int kMinHash = 6;       // CCheat::CHEAT_MIN_HASH_SIZE

struct Entry { uint32_t hash; Cheat::Id id; };
// m_aCheatHashKeys rows that have an implementation here.
constexpr Entry kCheats[] = {
    {0xadfa640a, Cheat::Jetpack},           // CHEAT_GET_JETPACK
    {0xeeccea2b, Cheat::HealthArmorMoney},  // CHEAT_HEALTH_ARMOR_250K
    {0x8ca870dd, Cheat::Adrenaline},        // IAVENJQ typed (m_aCheatHashKeys labels around the wanted rows are shifted in the decomp)
};

char g_string[kStringSize] = {0};
std::atomic<int> g_hit{0};
uint32_t g_table[256];
bool g_megaPunch = false;
float g_adrenaline = 0.0f;   // seconds left
bool g_ready = false;
}  // namespace

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
        for (const Entry& e : kCheats)
            if (e.hash == hash) {
                g_hit = (int)e.id;
                g_string[0] = 0;
                return;
            }
    }
}

Cheat::Id Cheat::Take() {
    return (Id)g_hit.exchange(0);
}

bool Cheat::MegaPunchOn() { return g_megaPunch; }
void Cheat::ToggleMegaPunch() { g_megaPunch = !g_megaPunch; }
bool Cheat::AdrenalineOn() { return g_adrenaline > 0.0f; }
void Cheat::StartAdrenaline() { g_adrenaline = 20.0f; }
void Cheat::ClearAdrenaline() { g_adrenaline = 0.0f; }
void Cheat::Tick(float dt) { if (g_adrenaline > 0.0f) g_adrenaline = g_adrenaline > dt ? g_adrenaline - dt : 0.0f; }
