// img_archive.hpp - IMG v2 archive reader (private implementation).
//
// GTA:SA packs .dff/.txd/.col/.ipl into models/gta3.img (IMG version 2). This
// reads the directory and extracts entries on demand. No assets are stored in
// the repo; everything is read from the user's own gta3.img at runtime.
// Cross-platform: std::ifstream only.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace gta {

class ImgArchive {
public:
    bool Open(const std::string& path);   // false if missing / not VER2
    void Close();
    bool IsOpen() const { return m_open; }

    bool HasFile(const std::string& name) const;              // case-insensitive
    std::vector<uint8_t> Extract(const std::string& name) const;  // empty on fail

    std::size_t EntryCount() const { return m_entries.size(); }
    const std::string& EntryName(std::size_t i) const { return m_entries[i].name; }

private:
    struct Entry {
        std::string name;        // lowercase
        uint32_t offsetSectors;  // 2048-byte sectors
        uint32_t sizeSectors;
    };
    static std::string ToLower(const std::string& s);

    std::string m_path;
    std::vector<Entry> m_entries;
    std::unordered_map<std::string, std::size_t> m_index;
    bool m_open = false;
};

}  // namespace gta
