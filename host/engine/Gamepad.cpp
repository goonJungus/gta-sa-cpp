// Gamepad: see Gamepad.h.

#define NOMINMAX
#include "Gamepad.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

namespace {

struct XState {
    DWORD packet;
    WORD buttons;
    BYTE lt, rt;
    SHORT lx, ly, rx, ry;
};
typedef DWORD(WINAPI* GetStateFn)(DWORD, XState*);

GetStateFn Load() {
    static GetStateFn fn = nullptr;
    static bool tried = false;
    if (tried) return fn;
    tried = true;
    for (const char* dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"}) {
        if (HMODULE m = LoadLibraryA(dll)) {
            fn = (GetStateFn)GetProcAddress(m, "XInputGetState");
            if (fn) break;
        }
    }
    return fn;
}

float Dead(float v, float dead) {   // radial dead zone is applied by the caller on the pair
    return std::fabs(v) < dead ? 0.0f : v;
}

void Stick(SHORT x, SHORT y, float dead, float& ox, float& oy) {
    float fx = (float)x / 32767.0f, fy = (float)y / 32767.0f;
    const float m = std::sqrt(fx * fx + fy * fy);
    if (m < dead) { ox = oy = 0.0f; return; }
    const float s = std::min(1.0f, (m - dead) / (1.0f - dead)) / m;
    ox = fx * s; oy = fy * s;
    (void)Dead;
}

}  // namespace

void Gamepad::Poll() {
    m_prev = m_buttons;
    GetStateFn fn = Load();
    if (!fn) { m_connected = false; return; }
    XState st = {};
    bool ok = false;
    if (m_connected) {
        ok = fn((DWORD)m_user, &st) == 0;
    } else if (--m_retry <= 0) {   // a scan of the four slots is slowish: do it twice a second
        m_retry = 30;
        for (int u = 0; u < 4 && !ok; u++)
            if (fn((DWORD)u, &st) == 0) { ok = true; m_user = u; }
    }
    if (!ok) {
        m_connected = false;
        m_buttons = 0; m_lx = m_ly = m_rx = m_ry = m_lt = m_rt = 0.0f; m_active = false;
        return;
    }
    m_connected = true;
    Stick(st.lx, st.ly, 0.22f, m_lx, m_ly);
    Stick(st.rx, st.ry, 0.20f, m_rx, m_ry);
    m_lt = st.lt > 30 ? (float)(st.lt - 30) / 225.0f : 0.0f;
    m_rt = st.rt > 30 ? (float)(st.rt - 30) / 225.0f : 0.0f;
    m_buttons = st.buttons;
    m_active = m_buttons != 0 || m_lx != 0.0f || m_ly != 0.0f || m_rx != 0.0f || m_ry != 0.0f || m_lt > 0.0f || m_rt > 0.0f;
}
