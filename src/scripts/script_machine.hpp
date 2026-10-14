// script_machine.hpp - concrete IScriptMachine (private).
//
// Foundation stage: loads the compiled SCM image from disk and exposes the
// global-variable space the host bridges mission state through. Opcode
// execution (thread scheduling, the main.scm mission VM) is a later pass; the
// interface and the global store are final.

#pragma once

#include "gta_sa/scripts.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace gta {

class ScriptMachine final : public IScriptMachine {
public:
    bool LoadScript(const std::string& scmPath) override;
    void Update(float deltaTime) override;

    int32_t GetGlobal(int32_t index) const override;
    void SetGlobal(int32_t index, int32_t value) override;

    bool IsLoaded() const override { return m_loaded; }
    std::size_t ThreadCount() const override { return m_threads; }

private:
    std::vector<uint8_t> m_image;         // raw SCM bytes
    std::array<int32_t, 1024> m_globals{}; // SA keeps a few hundred; 1024 headroom
    bool m_loaded = false;
    std::size_t m_threads = 0;
};

}  // namespace gta
