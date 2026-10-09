// CSurfaceInfos - minimal stand-in for the surface database
// (gta-reversed/source/game_sa/SurfaceInfos_c.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts needed by the collision subsystem are defined here: the
// per-surface flag bits read by IsSeeThrough/IsShootThrough. The full table is
// populated from data/surface.dat by the surface subsystem, which is not yet
// converted - see TODO below.

#pragma once

#include "ColTypes.h" // eColSurfaceType, CColPoint

#include <cstdint>

// eAdhesionGroup from gta-reversed SurfaceInfo_c.h (values verified 2026-10-09).
// Delete this stand-in when the surface subsystem is converted.
enum eAdhesionGroup : int32_t {
    ADHESION_GROUP_RUBBER = 0,
    ADHESION_GROUP_HARD,
    ADHESION_GROUP_ROAD,
    ADHESION_GROUP_LOOSE,
    ADHESION_GROUP_SAND,
    ADHESION_GROUP_WET,
};

class CSurfaceInfos {
public:
    // Faithful entry stride: 12 bytes per surface. The flag dword lives at
    // entry+4: bit 12 = see-through, bit 13 = shoot-through.
    // Verified against decomp src/SurfaceInfos_c/IsSeeThrough_0055e6b0.c and
    // src/SurfaceInfos_c/IsShootThrough_0055e6d0.c.
    struct SurfaceEntry {
        uint32_t data[3]{};
    };

    // gta-reversed: m_surfaces[id < 195]
    static constexpr uint32_t kNumSurfaces = 195;

    bool IsSeeThrough(uint32_t id) const {
        return id < kNumSurfaces && ((m_surfaces[id].data[1] >> 12) & 1u) != 0;
    }
    // TODO(port): stubs added 2026-10-09 for CPed
    bool IsStairs(uint32_t surfaceId) { (void)surfaceId; return false; }
    bool LeavesFootsteps(uint32_t surfaceId) { (void)surfaceId; return false; }
    bool IsShallowWater(uint32_t surfaceId) { (void)surfaceId; return false; }

    bool IsShootThrough(uint32_t id) const {
        return id < kNumSurfaces && ((m_surfaces[id].data[1] >> 13) & 1u) != 0;
    }

    // TODO: populate the table from data/surface.dat when the surface
    // subsystem is converted. Until then every surface reports not
    // see-through / not shoot-through, which makes doSeeThroughCheck=true
    // skip all primitives (matching "no data", not the shipped game).

    // TODO(port): real table lookups from data/surface.dat. Stubs added
    // 2026-10-09 for CAutomobile (skidmarks, steering adhesive, roughness).
    uint32_t GetSkidmarkType(eColSurfaceType surfaceId) const { (void)surfaceId; return 0; }
    uint32_t GetRoughness(eColSurfaceType surfaceId) const { (void)surfaceId; return 0; }
    float GetAdhesiveLimit(CColPoint* colPoint) const { (void)colPoint; return 0.0f; }
    // TODO(port): real lookup in the surface table (gta-reversed SurfaceInfos_c.h:27
    // takes SurfaceId; this stand-in takes eColSurfaceType to match the port's
    // ColTypes convention). Added 2026-10-09 for CAutomobile.
    eAdhesionGroup GetAdhesionGroup(eColSurfaceType surfaceId) const { (void)surfaceId; return ADHESION_GROUP_ROAD; }
    // TODO(port): real wet multiplier lookup (gta-reversed SurfaceInfos_c::GetWetMultiplier).
    // Added 2026-10-09 for CAutomobile.
    float GetWetMultiplier(eColSurfaceType surfaceId) const { (void)surfaceId; return 1.0f; }

    // TODO(port): real IsWater check (gta-reversed SurfaceInfos_c::IsWater).
    // Added 2026-10-09 for CAutomobile.
    bool IsWater(eColSurfaceType surfaceId) const { (void)surfaceId; return false; }

    // TODO(port): real IsSand check (gta-reversed SurfaceInfos_c::IsSand).
    // Added 2026-10-09 for CAutomobile.
    bool IsSand(eColSurfaceType surfaceId) const { (void)surfaceId; return GetAdhesionGroup(surfaceId) == ADHESION_GROUP_SAND; }

    // TODO(port): real IsSteepSlope check (gta-reversed SurfaceInfos_c::IsSteepSlope).
    // Added 2026-10-09 for CPed.
    bool IsSteepSlope(uint32_t surfaceId) const { (void)surfaceId; return false; }

    // TODO(port): real IsSoftLanding check (gta-reversed SurfaceInfos_c::IsSoftLanding).
    // Added 2026-10-09 for CPed.
    bool IsSoftLanding(uint32_t surfaceId) const { (void)surfaceId; return false; }

private:
    SurfaceEntry m_surfaces[kNumSurfaces]{};
};

extern CSurfaceInfos g_surfaceInfos;
