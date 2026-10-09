// GrTypes.h - adapted from gta-reversed for clean-room C++ build
// Base.h (plugin-sdk) integer typedef replacements, shared by all converted headers.
// NOTE: CColStore.h, CLoadedCarGroup.h, CStreaming.h, CStreamingInfo.h predate this
//   file and inline their own copies - do not include this header alongside them
//   in one TU (alias redefinition). Future conversions should use this header.
//
#pragma once

#include <cstdint>

// Base.h (plugin-sdk) replacements - gta-reversed integer typedefs
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
