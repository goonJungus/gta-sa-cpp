#pragma once
// Gamepad: XInput controllers, loaded at run time (xinput1_4 / 1_3 / 9_1_0) so the exe still starts
// where none is installed. Sticks come back dead-zoned and rescaled to -1..1.

#include <cstdint>

class Gamepad {
public:
    enum Button : uint16_t {
        DPAD_UP = 0x0001, DPAD_DOWN = 0x0002, DPAD_LEFT = 0x0004, DPAD_RIGHT = 0x0008,
        START = 0x0010, BACK = 0x0020, LTHUMB = 0x0040, RTHUMB = 0x0080,
        LB = 0x0100, RB = 0x0200, A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000,
    };
    // Reads controller 0..3 (first one connected). Cheap enough to call every frame.
    void Poll();
    bool Connected() const { return m_connected; }
    float LX() const { return m_lx; }
    float LY() const { return m_ly; }
    float RX() const { return m_rx; }
    float RY() const { return m_ry; }
    float LT() const { return m_lt; }
    float RT() const { return m_rt; }
    bool Down(uint16_t b) const { return (m_buttons & b) != 0; }
    bool Pressed(uint16_t b) const { return (m_buttons & b) != 0 && (m_prev & b) == 0; }
    // Any input at all this frame (used to show pad prompts).
    bool Active() const { return m_active; }

private:
    bool m_connected = false, m_active = false;
    float m_lx = 0, m_ly = 0, m_rx = 0, m_ry = 0, m_lt = 0, m_rt = 0;
    uint16_t m_buttons = 0, m_prev = 0;
    int m_user = 0;
    int m_retry = 0;   // frames until the next scan for a controller
};
