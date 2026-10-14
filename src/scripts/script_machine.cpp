// script_machine.cpp - see script_machine.hpp.

#include "scripts/script_machine.hpp"

#include <fstream>

namespace gta {

bool ScriptMachine::LoadScript(const std::string& scmPath) {
    std::ifstream f(scmPath, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamoff size = f.tellg();
    if (size <= 0) return false;
    f.seekg(0, std::ios::beg);
    m_image.resize((std::size_t)size);
    if (!f.read(reinterpret_cast<char*>(m_image.data()), size)) {
        m_image.clear();
        return false;
    }
    m_loaded = true;
    m_threads = 0;   // VM not yet started; thread scheduling is a later pass
    return true;
}

void ScriptMachine::Update(float /*deltaTime*/) {
    // Foundation: the SCM image is loaded but not yet executed. Opcode dispatch
    // and thread scheduling land in a later pass.
}

int32_t ScriptMachine::GetGlobal(int32_t index) const {
    if (index < 0 || index >= (int32_t)m_globals.size()) return 0;
    return m_globals[(std::size_t)index];
}

void ScriptMachine::SetGlobal(int32_t index, int32_t value) {
    if (index < 0 || index >= (int32_t)m_globals.size()) return;
    m_globals[(std::size_t)index] = value;
}

}  // namespace gta
