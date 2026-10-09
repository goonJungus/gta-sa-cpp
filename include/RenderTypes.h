// RenderTypes - adapted from gta-reversed for clean-room C++ build
// Minimal faithful stand-ins for the RenderWare SDK types the render-subsystem
// headers need but which belong to the (not yet converted) RenderWare layer.
// Follows the ColTypes.h precedent: faithful data layout, methods omitted until
// the owning subsystem is converted. Enum VALUES are per RW SDK 3.7 (rwcore.h)
// — verify against the real SDK headers when the render layer is implemented.
// TODO: replace with the real RenderWare conversion, then delete this file.

#pragma once

#include "RenderWare.h" // RwRGBA, RwTextureFilterMode (deduped 2026-10-09)

#include <cstdint>

// ---- opaque RenderWare handles (pointer use only) ----
struct RwTexture;
struct RwRaster;
struct RwTexDictionary;
struct RwStream;
struct RwD3D9Vertex;

// ---- RenderWare integer/bool aliases ----
using RwUInt8  = uint8_t;
using RwUInt32 = uint32_t;
using RwBool   = int32_t; // RW SDK: RwBool is a 32-bit int

// ---- color / rect ----
// RwRGBA: canonical definition in RenderWare.h (deduped 2026-10-09; removed here).

struct RwRGBAReal {
    float red{}, green{}, blue{}, alpha{};
};

struct RwRect {
    int32_t x{}, y{}, w{}, h{};
};

// ---- immediate-mode 2D vertex (RW SDK 3.7 batypes.h layout, 0x1C) ----
struct RwIm2DVertex {
    float  x{}, y{};     // screen coords
    float  z{};          // camera Z
    float  rhw{};        // reciprocal homogeneous W
    RwRGBA emissiveColor{};
    float  u{}, v{};     // texture coords
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(RwIm2DVertex) == 0x1C, "RwIm2DVertex layout changed");
#endif

using RwImVertexIndex = uint16_t; // RW SDK: index into an immediate-mode vertex buffer

// ---- enums (values per RW SDK 3.7; verify on render-layer conversion) ----
enum RwBlendFunction {
    rwBLENDNABLEND = 0,
    rwBLENDZERO,
    rwBLENDONE,
    rwBLENDSRCCOLOR,
    rwBLENDINVSRCCOLOR,
    rwBLENDSRCALPHA,
    rwBLENDINVSRCALPHA,
    rwBLENDDESTALPHA,
    rwBLENDINVDESTALPHA,
    rwBLENDDESTCOLOR,
    rwBLENDINVDESTCOLOR,
    rwBLENDSRCALPHASAT
};

enum RwCullMode {
    rwCULLMODECULLNONE = 1,
    rwCULLMODECULLBACK,
    rwCULLMODECULLFRONT
};

enum RwShadeMode {
    rwSHADEMODEFLAT = 1,
    rwSHADEMODEGOURAUD
};

enum RwTextureAddressMode {
    rwTEXTUREADDRESSNATEXTUREADDRESS = 0,
    rwTEXTUREADDRESSWRAP,
    rwTEXTUREADDRESSMIRROR,
    rwTEXTUREADDRESSCLAMP,
    rwTEXTUREADDRESSBORDER
};

// RwTextureFilterMode: canonical definition in RenderWare.h (deduped 2026-10-09; removed here).

// ---- interim CRGBA (full conversion belongs to the graphics-core subsystem) ----
// Faithful 4-byte r/g/b/a layout from gta-reversed RGBA.h; ctors/Set/operators
// per gta-reversed (added 2026-10-09: CVehicleModelInfo needs CRGBA(r,g,b,a),
// CRGBA(RwRGBA), Set(), operator==).
struct CRGBA {
    uint8_t r{}, g{}, b{}, a{};

    CRGBA() = default;
    constexpr CRGBA(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a) : r(_r), g(_g), b(_b), a(_a) {}
    CRGBA(const RwRGBA& c) : r(c.red), g(c.green), b(c.blue), a(c.alpha) {}

    void Set(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a) { r = _r; g = _g; b = _b; a = _a; }

    bool operator==(const CRGBA& rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a; }
    bool operator!=(const CRGBA& rhs) const { return !(*this == rhs); }

    // gta-reversed RGBA.h (added 2026-10-09 for CAutomobile).
    constexpr CRGBA operator*(float m) const {
        return CRGBA(
            static_cast<uint8_t>(r * m),
            static_cast<uint8_t>(g * m),
            static_cast<uint8_t>(b * m),
            static_cast<uint8_t>(a * m)
        );
    }
    constexpr RwRGBA ToRwRGBA() const {
        return RwRGBA{ r, g, b, a };
    }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CRGBA) == 4, "CRGBA layout changed");
#endif

// ---- GXT text char (gta-reversed GxtChar.h: 8-bit GXT character) ----
using GxtChar = uint8_t;
