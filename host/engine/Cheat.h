#pragma once
// CCheat::AddToCheatString: the last 30 typed letters, newest first. After every key the buffer's
// first 6..29 letters are hashed with CKeyGen::GetKey (CRC32 of the upper-cased text, no final
// inversion) and looked up in m_aCheatHashKeys. A hit applies the cheat and clears the buffer.

#include <cstdint>

class Cheat {
public:
    enum Id { None = 0, Jetpack, HealthArmorMoney };
    // A typed letter A..Z (the WndProc calls this on every key-down that is not an auto-repeat).
    static void Push(char c);
    // The cheat a key completed since the last call (None if no cheat).
    static Id Take();
    static uint32_t Key(const char* s, int n);
};
