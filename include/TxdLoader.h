#pragma once
// RenderWare TXD (texture dictionary) loader for gtasa_cpp.exe
// Milestone 3: parses SA-era D3D8/D3D9 native textures out of TXD files.
// Supported raster formats: DXT1, DXT3, 8888 (RGBA swizzled to A8R8G8B8).
// Palettized and other formats are skipped. No D3DX dependency; the raw
// level-0 mip data is handed to D3DRenderer::CreateTexture.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <string>
#include <vector>

struct TxdTexture {
    std::string name;           // as stored in the TXD (original case)
    uint16_t width = 0;
    uint16_t height = 0;
    D3DFORMAT d3dFormat = D3DFMT_UNKNOWN;  // resolved D3D9 format
    std::vector<uint8_t> mip0;  // level-0 data, already in D3D byte order
};

class TxdLoader {
public:
    // Load all supported textures from a TXD file. Unsupported entries are
    // skipped; returns an empty vector on failure.
    static std::vector<TxdTexture> Load(const std::string& path);

    // Lowercase lookup key (GTA texture names are case-insensitive).
    static std::string KeyOf(const std::string& name);
};
