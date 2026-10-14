// gta_sa_module.hpp - Primary master header & initialization API.
//
// PUBLIC API of the GTA: San Andreas engine module. This is the only header
// the integrator needs to include to drive a GTA world; it hands out the
// subsystem interfaces (world streaming, peds, vehicles, radio, scripts)
// declared in the sibling headers.
//
// Design rules (see DIRECTIVE.md / ../COORDINATION.md):
//   * No main(). The module is a library; a host links it and drives
//     IGtaWorld::init/update/shutdown.
//   * No game assets in the source tree. Everything is read at runtime from
//     the user's own installation, located via GtaConfig::gameDirectory.
//   * Cross-platform (Windows/macOS/Linux), 64-bit, C++20, no compiler
//     extensions, standard library only. No rendering API in the module: the
//     host owns the renderer and consumes engine-agnostic data.

#pragma once

#include <cstdint>
#include <string>

namespace gta {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Where the user's own game files live, and how much of them to load.
//
// Layout expected under gameDirectory (a retail GTA:SA 1.0 install):
//     <gameDirectory>/models/gta3.img          (archive: .dff/.txd/.col/.ipl)
//     <gameDirectory>/data/gta.dat             (which IDE/IPL files to load)
//     <gameDirectory>/data/maps/.../*.ide/*.ipl
//
// gameDirectory is passed in rather than auto-detected so the module has no
// platform-specific registry/Steam-path knowledge. Hosts that want autodetect
// resolve it themselves and pass the result here.
struct GtaConfig {
    // Folder containing models/ and data/. Empty = not configured.
    std::string gameDirectory;

    // Parse collision (.col) archives for the physics/host side. Off makes a
    // lighter world for tooling that only needs placements.
    bool loadCollision = true;

    // Parse the AI path nodes (nodes*.dat) for traffic/ped routing. Reserved;
    // ignored until the path subsystem lands.
    bool loadPaths = false;

    // Streaming budget. Upper bound on world placements held at once; guards a
    // corrupt file from exhausting memory. 0 = module default.
    uint64_t maxInstances = 0;

    // Radius in metres around the stream focus kept resident (see
    // IWorldStreaming::SetFocus). 0 = module default.
    float streamRadius = 0.0f;

    // Optional log sink. When null the module writes nothing; a host supplies
    // its own logging. Messages are already formatted, no trailing newline.
    void (*logSink)(const char* message) = nullptr;
};

// ---------------------------------------------------------------------------
// Status reporting
// ---------------------------------------------------------------------------

enum class Status {
    Ok = 0,
    NotConfigured,   // gameDirectory empty or does not exist
    ImgMissing,      // models/gta3.img not found
    DatMissing,      // data/gta.dat not found
    ReadFailed,      // I/O error reading an archive/definition file
    ParseFailed,     // structurally invalid IMG/IDE/IPL data
    TooManyInstances // exceeded GtaConfig::maxInstances
};

const char* toString(Status s);

// ---------------------------------------------------------------------------
// Subsystem interfaces (defined in the sibling public headers)
// ---------------------------------------------------------------------------

class IWorldStreaming;   // world_streaming.hpp
class IPedManager;       // peds.hpp
class IVehicleManager;   // vehicles.hpp
class IRadioPlayer;      // radio_audio.hpp
class IScriptMachine;    // scripts.hpp
struct CJStats;          // peds.hpp

// ---------------------------------------------------------------------------
// IGtaWorld - engine-facing lifecycle
// ---------------------------------------------------------------------------

class IGtaWorld {
public:
    virtual ~IGtaWorld() = default;

    // Load configuration and bring up the world: open the IMG, parse the IDE
    // object definitions and IPL placements named by gta.dat, and (optionally)
    // index collision. Calling init() on an already-initialized world shuts it
    // down first.
    virtual Status init(const GtaConfig& config) = 0;

    // Advance simulation by deltaTime seconds: streaming around the current
    // focus, ped/vehicle/script/radio updates. Safe before init() (no-op) and
    // after shutdown() (no-op).
    virtual void update(float deltaTime) = 0;

    // Release all resources. Idempotent.
    virtual void shutdown() = 0;

    // Subsystem accessors. Non-null once init() returns Ok; each is owned by
    // the world and lives until shutdown(). The host drives rendering/physics
    // by reading these; the module never renders.
    virtual IWorldStreaming* streaming() = 0;
    virtual IPedManager*     peds()      = 0;
    virtual IVehicleManager* vehicles()  = 0;
    virtual IRadioPlayer*    radio()     = 0;
    virtual IScriptMachine*  scripts()   = 0;

    // CJ's physical stats (Muscle/Fat/Stamina/LungCapacity/DrivingSkill). The
    // host may read and write these directly (gym/running progression lives
    // host-side or in the unified ECS). Non-null once initialized.
    virtual CJStats* player() = 0;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

// Create a world instance. Never returns null. The caller owns the result and
// destroys it with destroyGtaWorld().
IGtaWorld* createGtaWorld();

// Destroy a world created by createGtaWorld(). Passing null is a no-op. Calls
// shutdown() first if the world is still initialized.
void destroyGtaWorld(IGtaWorld* world);

// Library version, for host-side feature gating.
struct ModuleVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};
ModuleVersion moduleVersion();

}  // namespace gta
