#pragma once
// Delirium - optional active-ragdoll plugin, loaded at run time.
//
// The physics library is a separate, separately licensed DLL (delirium_physics.dll). Nothing of it
// is built into or shipped with this program: if the DLL (and the character data it reads) is
// not found, every call here is a no-op and the engine's own ragdoll is used.
//
// Settings ([Delirium] in gtasa_cpp.ini): Dll = path to delirium_physics.dll,
// Data = path to the character file / game data the library needs. Rewrite style only.

#include <cstdint>
#include <string>

struct NmWorld;
struct NmBuild;
struct NmAgent;

class Delirium {
public:
    // Load the DLL and its character data; false (with Error() set) if anything is missing.
    bool Load(const std::string& dllPath, const std::string& dataPath);
    void Unload();
    bool Ready() const { return m_ready; }
    const std::string& Error() const { return m_err; }
    int Version() const { return m_ready ? m_version() : 0; }

    NmWorld* WorldCreate() const { return m_ready ? m_worldCreate() : nullptr; }
    void WorldDestroy(NmWorld* w) const { if (m_ready && w) m_worldDestroy(w); }
    int WorldAddFloor(NmWorld* w, float x, float y, float z, float half) const {
        return (m_ready && w) ? m_worldAddFloor(w, x, y, z, half) : -1;
    }
    NmAgent* AgentCreate(const float pos[3], float yaw, float seed) const {
        return m_ready ? m_agentCreate(m_build, pos, yaw, seed) : nullptr;
    }
    void AgentDestroy(NmAgent* a) const { if (m_ready && a) m_agentDestroy(a); }
    int AgentStep(NmAgent* a, const NmWorld* w, float dt) const {
        return (m_ready && a) ? m_agentStep(a, w, dt) : -1;
    }
    int PartCount(const NmAgent* a) const { return (m_ready && a) ? m_partCount(a) : 0; }
    // pos[3] + rot[4] per part
    int PartTransforms(const NmAgent* a, float* out, int parts) const {
        return (m_ready && a) ? m_partTransforms(a, out, parts) : -1;
    }
    int Impulse(NmAgent* a, int part, const float at[3], const float impulse[3]) const {
        return (m_ready && a) ? m_impulse(a, part, at, impulse) : -1;
    }
    // A shot: the part hit, where, the bullet direction, the ped's health.
    int TaskShot(NmAgent* a, const NmWorld* w, int part, const float at[3], const float dir[3], float health) const {
        return (m_ready && a) ? m_taskShot(a, w, "*", part, at, dir, health) : -1;
    }

private:
    void* m_lib = nullptr;
    bool m_ready = false;
    std::string m_err;
    NmBuild* m_build = nullptr;
    int32_t (*m_version)() = nullptr;
    NmWorld* (*m_worldCreate)() = nullptr;
    void (*m_worldDestroy)(NmWorld*) = nullptr;
    int32_t (*m_worldAddFloor)(NmWorld*, float, float, float, float) = nullptr;
    NmAgent* (*m_agentCreate)(const NmBuild*, const float*, float, float) = nullptr;
    void (*m_agentDestroy)(NmAgent*) = nullptr;
    int32_t (*m_agentStep)(NmAgent*, const NmWorld*, float) = nullptr;
    int32_t (*m_partCount)(const NmAgent*) = nullptr;
    int32_t (*m_partTransforms)(const NmAgent*, float*, int32_t) = nullptr;
    int32_t (*m_impulse)(NmAgent*, int32_t, const float*, const float*) = nullptr;
    int32_t (*m_taskShot)(NmAgent*, const NmWorld*, const char*, int32_t, const float*, const float*, float) = nullptr;
};
