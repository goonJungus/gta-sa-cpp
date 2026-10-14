// map_data.hpp - IDE object definitions + IPL placements (private).
//
// Parses the map-definition text/binary formats into the public ObjectDef /
// Placement types. gta.dat names which IDE/IPL files to load; text IPLs live on
// disk; binary "stream" IPLs live inside gta3.img. Standard library only.

#pragma once

#include "gta_sa/world_streaming.hpp"  // ObjectDef, Placement

#include <cstdint>
#include <string>
#include <vector>

namespace gta {

// Read data/gta.dat, collecting the IDE and IPL file paths it lists (relative
// to the game directory). Returns false if gta.dat can't be opened.
bool ParseGtaDat(const std::string& gameDir,
                 std::vector<std::string>& ideRel,
                 std::vector<std::string>& iplRel);

// Parse one IDE file's "objs"/"tobj" sections, appending ObjectDefs. false on
// open failure.
bool LoadIde(const std::string& path, std::vector<ObjectDef>& out);

// Parse one text IPL file's "inst" section, appending Placements. false on
// open failure.
bool LoadIplText(const std::string& path, std::vector<Placement>& out);

// Parse a binary ("bnry") IPL from memory, appending Placements. false on bad
// magic / truncation.
bool LoadIplBinary(const uint8_t* data, std::size_t size,
                   std::vector<Placement>& out);

}  // namespace gta
