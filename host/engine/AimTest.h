#pragma once
// AimTest - a shooting range for the --aimtest flag: dummy peds at set distances (some strafing),
// hit tests for bullets, damage numbers and a score board.

#include <string>
#include <vector>

struct AimDummy {
    float baseX = 0, baseY = 0, baseZ = 0;     // spawn point (feet)
    float x = 0, y = 0, z = 0;                 // feet now
    float strafe = 0;                          // metres each side, 0 = standing still
    float phase = 0;
    float health = 100.0f, maxHealth = 100.0f;
    float respawn = 0.0f;                      // > 0: dead, seconds until it returns
    bool alive() const { return respawn <= 0.0f; }
};

struct AimFloat {                              // a rising damage number
    float x, y, z;
    float age = 0.0f;
    int amount = 0;
    bool head = false;
};

class AimTest {
public:
    // Dummies in a fan in front of (x, y) looking along (dirX, dirY); z comes from groundAt.
    template <class GroundFn>
    void Setup(float x, float y, float dirX, float dirY, GroundFn groundAt) {
        m_dummies.clear();
        const float rx = dirY, ry = -dirX;
        struct P { float fwd, side, strafe; };
        const P spots[] = {{5, -2.5f, 0}, {8, 3.0f, 0}, {12, -5.0f, 0}, {16, 6.0f, 0}, {22, -8.0f, 0},
                           {10, 0.0f, 4.0f}, {16, 0.0f, 6.0f}, {26, 0.0f, 8.0f}};
        for (const P& p : spots) {
            AimDummy d;
            d.baseX = x + dirX * p.fwd + rx * p.side;
            d.baseY = y + dirY * p.fwd + ry * p.side;
            d.baseZ = groundAt(d.baseX, d.baseY);
            d.strafe = p.strafe;
            d.phase = p.fwd * 0.7f;
            d.x = d.baseX; d.y = d.baseY; d.z = d.baseZ;
            m_dummies.push_back(d);
        }
        m_rx = rx; m_ry = ry;
    }
    void Update(float dt);
    // Nearest dummy on the segment a -> b (b = a + dir * maxLen). Returns its index or -1.
    int Trace(const float a[3], const float dir[3], float maxLen, float* tHit, bool* head) const;
    // Apply a hit; true when it killed the dummy.
    bool Damage(int index, float amount, bool head, const float at[3]);

    std::vector<AimDummy>& Dummies() { return m_dummies; }
    const std::vector<AimFloat>& Floats() const { return m_floats; }
    int shots = 0, hits = 0, headshots = 0, kills = 0;

    static constexpr float kRadius = 0.38f, kHeight = 1.8f, kHeadZ = 1.45f;

private:
    std::vector<AimDummy> m_dummies;
    std::vector<AimFloat> m_floats;
    float m_rx = 1, m_ry = 0, m_time = 0;
};
