// radio_player.hpp - concrete IRadioPlayer (private).
//
// Tracks station selection, context (vehicle / on-foot Walkman) and Bullet-Time
// dilation, independent of the vehicle system. The station list is the retail
// SA line-up; the actual audio decode/mix is the host backend's job (it resolves
// streamKey against the user's own audio files).

#pragma once

#include "gta_sa/radio_audio.hpp"

namespace gta {

class RadioPlayer final : public IRadioPlayer {
public:
    RadioPlayer();

    const std::vector<RadioStation>& Stations() const override { return m_stations; }

    void SetContext(RadioContext ctx) override { m_context = ctx; }
    RadioContext Context() const override { return m_context; }

    void Select(int32_t stationId) override;
    int32_t Current() const override { return m_current; }

    void SetTimeDilation(float scale) override { m_dilation = scale; }
    float TimeDilation() const override { return m_dilation; }

    void Update(float deltaTime) override;

private:
    std::vector<RadioStation> m_stations;
    RadioContext m_context = RadioContext::Off;
    int32_t m_current = -1;     // -1 = off
    float m_dilation = 1.0f;
    float m_trackTime = 0.0f;   // seconds into the current station's stream
};

}  // namespace gta
