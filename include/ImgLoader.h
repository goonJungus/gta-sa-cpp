#pragma once
// IMG v2 archive parser for gtasa_cpp.exe (Milestone 4).
// GTA SA uses IMG version 2: "VER2" magic, then a directory of entries.
// Each entry: u32 offset (in 2048-byte sectors), u32 size (in sectors),
// char name[24] (null-padded). File data follows the directory.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

struct ImgEntry {
    std::string name;       // lowercase filename (e.g. "lae2sf1.dff")
    uint32_t offsetSectors; // offset in 2048-byte sectors
    uint32_t sizeSectors;   // size in 2048-byte sectors
};

class ImgLoader {
public:
    // Open an IMG archive. Returns false on failure.
    bool Open(const std::string& path);

    // Check if a file exists in the archive (case-insensitive).
    bool HasFile(const std::string& name) const;

    // Extract a file's raw bytes. Returns empty vector on failure.
    std::vector<uint8_t> Extract(const std::string& name) const;

    // Number of entries in the archive.
    size_t EntryCount() const { return m_entries.size(); }

    void Close();

private:
    static std::string ToLower(const std::string& s);

    std::string m_path;
    std::vector<ImgEntry> m_entries;
    std::unordered_map<std::string, size_t> m_index; // lowercase name -> entry idx
    bool m_open = false;
};
