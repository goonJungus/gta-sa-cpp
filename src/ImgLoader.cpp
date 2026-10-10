// IMG v2 archive parser for gtasa_cpp.exe (Milestone 4).
// See ImgLoader.h for format details.

#include "ImgLoader.h"
#include <cstdio>
#include <cctype>
#include <cstring>

static const uint32_t SECTOR_SIZE = 2048;

std::string ImgLoader::ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = (char)tolower((unsigned char)c);
    return out;
}

bool ImgLoader::Open(const std::string& path) {
    Close();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        fclose(f);
        return false;
    }
    if (memcmp(magic, "VER2", 4) != 0) {
        fclose(f);
        return false;
    }

    uint32_t numEntries = 0;
    if (fread(&numEntries, 4, 1, f) != 1) {
        fclose(f);
        return false;
    }
    // Sanity cap: gta3.img has ~16k entries; reject absurd counts.
    if (numEntries == 0 || numEntries > 100000) {
        fclose(f);
        return false;
    }

    m_entries.reserve(numEntries);
    for (uint32_t i = 0; i < numEntries; i++) {
        uint32_t offset = 0, size = 0;
        char nameBuf[24];
        if (fread(&offset, 4, 1, f) != 1 ||
            fread(&size, 4, 1, f) != 1 ||
            fread(nameBuf, 1, 24, f) != 24) {
            fclose(f);
            Close();
            return false;
        }
        // Name is null-padded; trim at first NUL.
        size_t nameLen = 0;
        while (nameLen < 24 && nameBuf[nameLen] != '\0')
            nameLen++;
        std::string name(nameBuf, nameLen);
        std::string key = ToLower(name);
        m_index[key] = m_entries.size();
        m_entries.push_back({name, offset, size});
    }

    fclose(f);
    m_path = path;
    m_open = true;
    return true;
}

bool ImgLoader::HasFile(const std::string& name) const {
    if (!m_open)
        return false;
    return m_index.find(ToLower(name)) != m_index.end();
}

std::vector<uint8_t> ImgLoader::Extract(const std::string& name) const {
    std::vector<uint8_t> out;
    if (!m_open)
        return out;
    auto it = m_index.find(ToLower(name));
    if (it == m_index.end())
        return out;
    const ImgEntry& e = m_entries[it->second];

    FILE* f = fopen(m_path.c_str(), "rb");
    if (!f)
        return out;

    uint64_t byteOffset = (uint64_t)e.offsetSectors * SECTOR_SIZE;
    uint64_t byteSize = (uint64_t)e.sizeSectors * SECTOR_SIZE;
    // SA pads entries to full sectors, so reading the full sector range
    // is correct.
#ifdef _WIN32
    if (_fseeki64(f, (int64_t)byteOffset, SEEK_SET) != 0) {
        fclose(f);
        return out;
    }
#else
    if (fseeko(f, (off_t)byteOffset, SEEK_SET) != 0) {
        fclose(f);
        return out;
    }
#endif

    out.resize((size_t)byteSize);
    size_t got = fread(out.data(), 1, (size_t)byteSize, f);
    fclose(f);
    if (got != (size_t)byteSize) {
        out.clear();
        return out;
    }
    return out;
}

void ImgLoader::Close() {
    m_path.clear();
    m_entries.clear();
    m_index.clear();
    m_open = false;
}
