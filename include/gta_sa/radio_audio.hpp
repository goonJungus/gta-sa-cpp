// radio_audio.hpp - Radio station stream controllers & SFX.
//
// The radio player is updated INDEPENDENTLY of vehicles (Architectural Rule 4)
// so on-foot Walkman playback works anywhere. The module tracks station
// selection and playback state and resolves station stream keys; the host's
// audio backend does the actual decode/mix and applies Bullet-Time dilation.

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace gta {

// A radio station. Audio data is streamed from the user's own files by the
// host backend; this module only carries the metadata and the stream key.
struct RadioStation {
    int32_t id = -1;
    std::string name;        // "Radio Los Santos", "K-DST", "SF-UR", ...
    std::string streamKey;   // key/path the host audio backend resolves
};

// Where the radio is heard, so the host can route/attenuate it.
enum class RadioContext {
    Off,
    InVehicle,   // playing from the current vehicle's head unit
    Walkman      // on foot (dungeons/ruins/anywhere) - vehicle-independent
};

class IRadioPlayer {
public:
    virtual ~IRadioPlayer() = default;

    virtual const std::vector<RadioStation>& Stations() const = 0;

    virtual void SetContext(RadioContext ctx) = 0;
    virtual RadioContext Context() const = 0;

    virtual void Select(int32_t stationId) = 0;   // -1 = radio off
    virtual int32_t Current() const = 0;

    // 1.0 = normal; <1 slows + pitches down (Bullet Time). Host applies to mix.
    virtual void SetTimeDilation(float scale) = 0;
    virtual float TimeDilation() const = 0;

    // Advance the playback clock (track position / station switching) by dt.
    // Called from IGtaWorld::update, independent of the vehicle system.
    virtual void Update(float deltaTime) = 0;
};

}  // namespace gta
