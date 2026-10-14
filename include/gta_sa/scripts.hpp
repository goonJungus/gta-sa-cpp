// scripts.hpp - SCM script VM & mission triggers.
//
// Runs the compiled mission script (main.scm) that drives missions, triggers,
// timers and world state. The host reads/writes script globals to bridge
// mission state into the unified game; opcodes whose side effects the host
// doesn't consume still advance the VM.

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace gta {

class IScriptMachine {
public:
    virtual ~IScriptMachine() = default;

    // Load a compiled SCM image (usually data/script/main.scm under the game
    // dir). Returns false if it cannot be read or is not a valid SCM.
    virtual bool LoadScript(const std::string& scmPath) = 0;

    // Advance all running script threads by dt seconds.
    virtual void Update(float deltaTime) = 0;

    // Global script variables, for host-side mission-state bridging.
    virtual int32_t GetGlobal(int32_t index) const = 0;
    virtual void SetGlobal(int32_t index, int32_t value) = 0;

    virtual bool IsLoaded() const = 0;
    virtual std::size_t ThreadCount() const = 0;
};

}  // namespace gta
