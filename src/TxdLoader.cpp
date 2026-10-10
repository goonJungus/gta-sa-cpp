#include "TxdLoader.h"
#include <fstream>
#include <cstring>
#include <cctype>

// RenderWare section types
enum : uint32_t {
    TXD_RW_STRUCT         = 0x01,
    TXD_RW_STRING         = 0x02,
    TXD_RW_EXTENSION      = 0x03,
    TXD_RW_TEXTURENATIVE  = 0x15,
    TXD_RW_TEXDICTIONARY  = 0x16,
};

// D3D8 raster-format flags seen in SA TXDs
enum : uint32_t {
    RWF_DXT1 = 0x200,
    RWF_DXT3 = 0x400,
    RWF_1555 = 0x1000,
    RWF_4444 = 0x2000,
    RWF_8888 = 0x4000,
};

// Sanity caps
static const uint32_t kMaxDimension = 2048;
static const uint32_t kMaxMipBytes  = 16 * 1024 * 1024;

namespace {

struct SectionHeader {
    uint32_t type;
    uint32_t size;
    uint32_t version;
};

struct Reader {
    const uint8_t* data;
    size_t size;
    size_t pos;

    bool Read(void* out, size_t n) {
        if (pos + n > size)
            return false;
        memcpy(out, data + pos, n);
        pos += n;
        return true;
    }
    bool Skip(size_t n) {
        if (pos + n > size)
            return false;
        pos += n;
        return true;
    }
    template<typename T> bool ReadT(T& out) { return Read(&out, sizeof(T)); }
    bool ReadHeader(SectionHeader& h) { return ReadT(h); }
};

// 88-byte D3D8/D3D9 native-texture header inside the RW_STRUCT
struct NativeHeader {
    uint32_t platformId;   // 8 = D3D8, 9 = D3D9
    uint32_t filterMode;   // u8 filter, u8 wrapU, u8 wrapV, u8 pad
    char textureName[32];
    char maskName[32];
    uint32_t rasterFormat;
    uint32_t d3dFormat;    // D3DFORMAT value (often 0 in SA TXDs)
    uint16_t width;
    uint16_t height;
    uint8_t depth;
    uint8_t numMipmaps;
    uint8_t rasterType;
    uint8_t compression;   // 1 = DXT1, 2 = DXT2, 3 = DXT3
};
static_assert(sizeof(NativeHeader) == 88, "NativeHeader must be 88 bytes");

std::string NameOf(const char* s, size_t n) {
    size_t len = 0;
    while (len < n && s[len] != '\0')
        len++;
    return std::string(s, len);
}

}  // namespace

std::string TxdLoader::KeyOf(const std::string& name) {
    std::string k;
    k.reserve(name.size());
    for (char c : name)
        k.push_back((char)tolower((unsigned char)c));
    return k;
}

std::vector<TxdTexture> TxdLoader::Load(const std::string& path) {
    std::vector<TxdTexture> out;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return out;
    size_t fileSize = (size_t)f.tellg();
    if (fileSize < 12)
        return out;
    f.seekg(0);
    std::vector<uint8_t> buf(fileSize);
    if (!f.read((char*)buf.data(), fileSize))
        return out;

    Reader r{ buf.data(), buf.size(), 0 };
    SectionHeader h;
    if (!r.ReadHeader(h) || h.type != TXD_RW_TEXDICTIONARY)
        return out;
    size_t dictEnd = r.pos + h.size;
    if (dictEnd > r.size)
        return out;

    // TexDictionary struct: u16 numTextures (+ u16 pad/device id)
    if (!r.ReadHeader(h) || h.type != TXD_RW_STRUCT)
        return out;
    size_t dictStructEnd = r.pos + h.size;
    uint16_t numTextures = 0;
    if (!r.ReadT(numTextures))
        return out;
    r.pos = dictStructEnd;

    for (uint16_t i = 0; i < numTextures; i++) {
        if (r.pos + 12 > dictEnd || r.pos + 12 > r.size)
            break;
        if (!r.ReadHeader(h))
            break;
        size_t nativeEnd = r.pos + h.size;
        if (h.type != TXD_RW_TEXTURENATIVE || nativeEnd > r.size) {
            r.pos = nativeEnd;
            continue;
        }

        // RW_STRUCT: native header, then palette (palettized only),
        // then per-mipmap: u32 dataSize + data.
        TxdTexture tex;
        bool haveStruct = false;
        if (r.ReadHeader(h) && h.type == TXD_RW_STRUCT) {
            size_t structEnd = r.pos + h.size;
            if (structEnd <= r.size && h.size >= sizeof(NativeHeader)) {
                NativeHeader nh;
                if (r.Read(&nh, sizeof(nh))) {
                    haveStruct = true;
                    tex.name = NameOf(nh.textureName, sizeof(nh.textureName));
                    tex.width = nh.width;
                    tex.height = nh.height;

                    bool isDXT1 = (nh.compression == 1) || (nh.rasterFormat & RWF_DXT1);
                    bool isDXT3 = (nh.compression == 3) || (nh.rasterFormat & RWF_DXT3);
                    bool is8888 = (nh.rasterFormat & RWF_8888) != 0;

                    uint32_t w = nh.width, hh = nh.height;
                    if (w == 0 || hh == 0 || w > kMaxDimension || hh > kMaxDimension) {
                        // bogus dimensions: skip entry
                    } else if (isDXT1 || isDXT3) {
                        tex.d3dFormat = isDXT1 ? D3DFMT_DXT1 : D3DFMT_DXT3;
                        uint32_t blockBytes = isDXT1 ? 8 : 16;
                        uint32_t expect = ((w + 3) / 4) * ((hh + 3) / 4) * blockBytes;
                        int levels = nh.numMipmaps > 0 ? nh.numMipmaps : 1;
                        bool ok = false;
                        for (int m = 0; m < levels; m++) {
                            uint32_t ds = 0;
                            if (!r.ReadT(ds) || ds > kMaxMipBytes)
                                break;
                            if (m == 0 && ds >= expect && ds <= kMaxMipBytes) {
                                // SA TXDs store exactly the DXT blocks; take them.
                                tex.mip0.resize(ds);
                                if (r.Read(tex.mip0.data(), ds))
                                    ok = true;
                                else
                                    tex.mip0.clear();
                            } else {
                                if (!r.Skip(ds))
                                    break;
                            }
                            if (m == 0 && !ok)
                                break;
                        }
                        if (!ok)
                            tex.mip0.clear();
                    } else if (is8888) {
                        tex.d3dFormat = D3DFMT_A8R8G8B8;
                        uint32_t expect = w * hh * 4;
                        int levels = nh.numMipmaps > 0 ? nh.numMipmaps : 1;
                        bool ok = false;
                        for (int m = 0; m < levels; m++) {
                            uint32_t ds = 0;
                            if (!r.ReadT(ds) || ds > kMaxMipBytes)
                                break;
                            if (m == 0 && ds >= expect && ds <= kMaxMipBytes) {
                                tex.mip0.resize(expect);
                                std::vector<uint8_t> raw(ds);
                                if (r.Read(raw.data(), ds)) {
                                    // RW stores RGBA byte order; D3D A8R8G8B8
                                    // wants B,G,R,A. Swizzle in place.
                                    for (uint32_t p = 0; p < expect; p += 4) {
                                        tex.mip0[p + 0] = raw[p + 2];
                                        tex.mip0[p + 1] = raw[p + 1];
                                        tex.mip0[p + 2] = raw[p + 0];
                                        tex.mip0[p + 3] = raw[p + 3];
                                    }
                                    ok = true;
                                } else {
                                    tex.mip0.clear();
                                }
                            } else {
                                if (!r.Skip(ds))
                                    break;
                            }
                            if (m == 0 && !ok)
                                break;
                        }
                        if (!ok)
                            tex.mip0.clear();
                    }
                    // else: unsupported raster format (palettized, 1555, 4444...)
                    // -> tex.mip0 stays empty, entry skipped below.
                }
            }
            r.pos = structEnd;
        }
        // RW_EXTENSION (usually empty)
        if (r.ReadHeader(h) && h.type == TXD_RW_EXTENSION)
            r.pos += h.size;

        if (haveStruct && !tex.mip0.empty() && tex.d3dFormat != D3DFMT_UNKNOWN)
            out.push_back(std::move(tex));

        r.pos = nativeEnd;
        if (r.pos > r.size)
            break;
    }
    return out;
}
