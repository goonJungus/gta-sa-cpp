// world_streaming.hpp - IPL/IDE placements, object definitions, zone streaming.
//
// Engine-agnostic view of the streamed world. The host asks which placements
// are near the focus and loads their models/textures/collision itself through
// the asset loaders; this module never renders. Also defines the shared math
// types (Vec3/Quat) used by the other subsystem headers.

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace gta {

// GTA world space: metres, +Z up. Quaternions are stored ready to apply as the
// placement rotation (the IPL loader normalizes handedness at load time).
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct Quat { float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f; };

// One IDE object definition (a modelled object: building, prop, vegetation...).
struct ObjectDef {
    int32_t id = -1;
    std::string modelName;   // .dff base name, lowercase
    std::string txdName;     // .txd base name, lowercase
    float drawDistance = 0.0f;
    uint32_t flags = 0;
    bool isTimed = false;    // came from a "tobj" (timed object) section
};

// One world placement (an IPL instance of an object).
struct Placement {
    int32_t modelId = -1;
    Vec3 position;
    Quat rotation;
    int32_t interior = 0;
    int32_t lod = -1;        // index into Placements() of the LOD, or -1
};

class IWorldStreaming {
public:
    virtual ~IWorldStreaming() = default;

    // Move the streaming focus (usually CJ / the active camera). Placements
    // within the configured radius are reported "needed" by QueryNear.
    virtual void SetFocus(const Vec3& worldPos) = 0;
    virtual Vec3 Focus() const = 0;

    // Object-definition lookup (from the IDEs). Null if unknown.
    virtual const ObjectDef* FindObject(int32_t modelId) const = 0;
    virtual const ObjectDef* FindObjectByName(const std::string& modelName) const = 0;

    // Every parsed placement. Stable for the world's lifetime.
    virtual const std::vector<Placement>& Placements() const = 0;

    // Append the indices (into Placements()) within the stream radius of the
    // focus to 'out'; returns how many were appended. Clear 'out' beforehand.
    virtual std::size_t QueryNear(std::vector<std::size_t>& out) const = 0;

    // Read a raw asset (model "<name>.dff", texture "<name>.txd", collision
    // "<name>.col") from the user's game archive, by file name. Empty if
    // absent. This is how the host obtains geometry/textures/collision to
    // decode and render -- the module ships no assets and does no rendering.
    virtual std::vector<uint8_t> ReadAsset(const std::string& fileName) const = 0;

    // Enumerate archive entries whose name ends with 'extension' (e.g. ".col").
    // Lets the host load the collision archives (which bundle many models' COL
    // data) to build its own physics. Returns the matching file names.
    virtual std::vector<std::string> ListAssets(const std::string& extension) const = 0;

    virtual std::size_t ObjectCount() const = 0;
    virtual std::size_t PlacementCount() const = 0;
};

}  // namespace gta
