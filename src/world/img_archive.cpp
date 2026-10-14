// img_archive.cpp - see img_archive.hpp.

#include "world/img_archive.hpp"

#include <fstream>
#include <cctype>
#include <cstring>

namespace gta {

static constexpr uint32_t kSectorSize = 2048;

std::string ImgArchive::ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

bool ImgArchive::Open(const std::string& path) {
    Close();
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[4];
    if (!f.read(magic, 4) || std::memcmp(magic, "VER2", 4) != 0) return false;

    uint32_t numEntries = 0;
    if (!f.read(reinterpret_cast<char*>(&numEntries), 4)) return false;
    if (numEntries == 0 || numEntries > 100000) return false;  // sanity

    m_entries.reserve(numEntries);
    for (uint32_t i = 0; i < numEntries; i++) {
        uint32_t offset = 0, size = 0;
        char nameBuf[24];
        if (!f.read(reinterpret_cast<char*>(&offset), 4) ||
            !f.read(reinterpret_cast<char*>(&size), 4) ||
            !f.read(nameBuf, 24)) {
            Close();
            return false;
        }
        std::size_t nameLen = 0;
        while (nameLen < 24 && nameBuf[nameLen] != '\0') nameLen++;
        std::string name = ToLower(std::string(nameBuf, nameLen));
        m_index[name] = m_entries.size();
        m_entries.push_back({name, offset, size});
    }
    m_path = path;
    m_open = true;
    return true;
}

bool ImgArchive::HasFile(const std::string& name) const {
    return m_open && m_index.find(ToLower(name)) != m_index.end();
}

std::vector<uint8_t> ImgArchive::Extract(const std::string& name) const {
    std::vector<uint8_t> out;
    if (!m_open) return out;
    auto it = m_index.find(ToLower(name));
    if (it == m_index.end()) return out;
    const Entry& e = m_entries[it->second];

    std::ifstream f(m_path, std::ios::binary);
    if (!f) return out;

    // IMG entries are padded to whole 2048-byte sectors.
    const std::streamoff byteOffset =
        static_cast<std::streamoff>(e.offsetSectors) * kSectorSize;
    const std::size_t byteSize =
        static_cast<std::size_t>(e.sizeSectors) * kSectorSize;

    f.seekg(byteOffset, std::ios::beg);
    if (!f) return out;
    out.resize(byteSize);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(byteSize));
    if (static_cast<std::size_t>(f.gcount()) != byteSize) out.clear();
    return out;
}

void ImgArchive::Close() {
    m_path.clear();
    m_entries.clear();
    m_index.clear();
    m_open = false;
}

}  // namespace gta
