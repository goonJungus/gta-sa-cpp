// CMBlur - minimal stub for the clean-room C++ build
// Source: gta-reversed/source/game_sa/MBlur.h
//
// III/VC leftover motion-blur. Only the drunk-blur entry points used by
// converted TUs are declared here; bodies are no-ops until the render
// backend is ported. Full port needs RwCamera/RwRaster (RenderWare SDK).

#pragma once

class CMBlur {
public:
    static void SetDrunkBlur(float drunkness) { (void)drunkness; } // TODO(render): real motion blur
    static void ClearDrunkBlur() {}                               // TODO(render): real motion blur
};
