// radio_player.cpp - see radio_player.hpp.

#include "audio/radio_player.hpp"

namespace gta {

RadioPlayer::RadioPlayer() {
    // Retail GTA:SA station line-up. streamKey is the host-resolved name under
    // the user's audio/streams folder (no audio ships in the repo).
    const char* names[][2] = {
        {"Playback FM",            "PLAYBACK"},
        {"K-Rose",                 "KROSE"},
        {"K-DST",                  "KDST"},
        {"Bounce FM",              "BOUNCE"},
        {"SF-UR",                  "SFUR"},
        {"Radio Los Santos",       "RADIO_LOS_SANTOS"},
        {"Radio X",                "RADIO_X"},
        {"CSR 103.9",              "CSR"},
        {"K-Jah West",             "KJAH"},
        {"Master Sounds 98.3",     "MASTERSOUNDS"},
        {"WCTR",                   "WCTR"},
    };
    int32_t id = 0;
    for (auto& n : names)
        m_stations.push_back({id++, n[0], n[1]});
}

void RadioPlayer::Select(int32_t stationId) {
    if (stationId < 0) { m_current = -1; return; }
    if (stationId >= (int32_t)m_stations.size()) return;
    if (stationId != m_current) { m_current = stationId; m_trackTime = 0.0f; }
}

void RadioPlayer::Update(float deltaTime) {
    if (m_current < 0 || m_context == RadioContext::Off) return;
    // Advance the playback clock scaled by Bullet-Time dilation so the host can
    // keep a vehicle/Walkman stream in sync and pitch-shift it.
    m_trackTime += deltaTime * (m_dilation > 0.0f ? m_dilation : 0.0f);
}

}  // namespace gta
