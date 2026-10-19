#include "Delirium.h"

#include <windows.h>

namespace {
template <class F> bool Bind(HMODULE lib, const char* name, F& fn, std::string& err) {
    fn = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(lib, name)));
    if (!fn) err = std::string("missing export ") + name;
    return fn != nullptr;
}
}

bool Delirium::Load(const std::string& dllPath, const std::string& dataPath) {
    Unload();
    if (dllPath.empty()) { m_err = "no Dll set"; return false; }
    HMODULE lib = LoadLibraryA(dllPath.c_str());
    if (!lib) { m_err = "cannot load " + dllPath; return false; }

    int32_t (*init)(const char*) = nullptr;
    int32_t (*initChar)(const char*) = nullptr;
    NmBuild* (*buildFrom)(const char*) = nullptr;
    bool ok = Bind(lib, "nm_version", m_version, m_err)
        && Bind(lib, "nm_init", init, m_err)
        && Bind(lib, "nm_world_create", m_worldCreate, m_err)
        && Bind(lib, "nm_world_destroy", m_worldDestroy, m_err)
        && Bind(lib, "nm_world_add_floor", m_worldAddFloor, m_err)
        && Bind(lib, "nm_agent_create", m_agentCreate, m_err)
        && Bind(lib, "nm_agent_destroy", m_agentDestroy, m_err)
        && Bind(lib, "nm_agent_step", m_agentStep, m_err)
        && Bind(lib, "nm_agent_part_count", m_partCount, m_err)
        && Bind(lib, "nm_agent_get_part_transforms", m_partTransforms, m_err)
        && Bind(lib, "nm_agent_apply_impulse", m_impulse, m_err)
        && Bind(lib, "nm_agent_task_shot", m_taskShot, m_err);
    (void)initChar; (void)buildFrom;
    if (!ok) { FreeLibrary(lib); return false; }
    if (init(dataPath.c_str()) < 0) {
        m_err = "nm_init failed for " + dataPath;
        FreeLibrary(lib);
        return false;
    }
    m_lib = lib;
    m_ready = true;
    return true;
}

void Delirium::Unload() {
    if (m_lib) FreeLibrary(static_cast<HMODULE>(m_lib));
    m_lib = nullptr;
    m_ready = false;
    m_build = nullptr;
}
