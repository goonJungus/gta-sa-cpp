// CPointLights - minimal clean-room header for clean-room C++ build
// Adapted from gta-reversed/source/game_sa/PointLights.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// MINIMAL PORT: only the lighting API used by CPed::SetupLighting/RemoveLighting.
// TODO: port the full point-light system when the renderer batch lands.
// Added 2026-10-09 for CPed compile errors.

#pragma once

#include "CVector.h" // CVector

#include <cstdint>

class CEntity;

class CPointLights {
public:
    // gta-reversed PointLights.h: static void RemoveLightsAffectingObject();
    // TODO(port): real implementation (gta-reversed PointLights.cpp).
    static void RemoveLightsAffectingObject();
};

// TODO(port): these are free functions in gta-reversed (declared in
//   source/app/app.h, defined in the app's lighting code). Declared here so
//   CPed::SetupLighting/RemoveLighting compile; real implementations land
//   with the renderer batch.
void ActivateDirectional();
void DeActivateDirectional();
void SetAmbientColours();
