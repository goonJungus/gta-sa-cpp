// Collision audit (dev tool). See AuditCollision.h.

#ifndef NOMINMAX
#define NOMINMAX   // TxdLoader.h pulls in <windows.h>; keep std::min/std::max usable
#endif

#include "AuditCollision.h"
#include "ImgLoader.h"
#include "IdeLoader.h"
#include "DffLoader.h"
#include "TxdLoader.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>
#include <algorithm>
#include <fstream>

namespace {

struct Vec3 { float x = 0, y = 0, z = 0; };

struct ColInfo {
    Vec3 mn, mx;            // model-space collision AABB (COL3 TBounds)
    int numSpheres = 0, numBoxes = 0, numTris = 0;
    std::string arc;        // which .col archive it came from
};

struct ModelInfo {
    bool inIde = false;
    int flags = 0;
    float drawDist = 0;
    std::string txd;
};

inline uint16_t rdU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t rdU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline float rdF32(const uint8_t* p) { float f; std::memcpy(&f, p, 4); return f; }

std::string lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// COL3 fixed-size name field (22 bytes), stop at first NUL.
std::string colName(const uint8_t* field) {
    std::string s;
    for (int i = 0; i < 22; i++) {
        if (field[i] == '\0') break;
        s += (char)field[i];
    }
    return lower(s);
}

Vec3 center(const Vec3& a, const Vec3& b) {
    return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
}
Vec3 extent(const Vec3& mn, const Vec3& mx) {
    return {mx.x - mn.x, mx.y - mn.y, mx.z - mn.z};
}
float diag(const Vec3& e) { return std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z); }
float dist(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Parse every COL3 entry in one archive, recording its TBounds AABB + counts.
void scanColArchive(const std::vector<uint8_t>& d, const std::string& arcName,
                    std::unordered_map<std::string, ColInfo>& out) {
    size_t pos = 0;
    while (pos + 8 <= d.size()) {
        if (std::memcmp(d.data() + pos, "COL3", 4) != 0) { pos++; continue; }
        size_t E = pos;
        uint32_t size = rdU32(d.data() + E + 4);
        size_t end = E + 8 + size;
        if (size < 0x48 || end > d.size()) { pos++; continue; }
        // COL3 TBounds (per decomp CFileLoader::LoadCollisionModelVer3): the
        // data ptr is E+0x20, and it begins with bbox min(3f)+max(3f), THEN
        // center(3f)+radius(f). So min@E+0x20, max@E+0x2C (NOT the COL1 layout
        // radius/center/min/max). Counts: numSpheres@0x48/boxes@0x4A/tris@0x4C.
        const uint8_t* e = d.data() + E;
        ColInfo ci;
        ci.mn = {rdF32(e + 0x20), rdF32(e + 0x24), rdF32(e + 0x28)};
        ci.mx = {rdF32(e + 0x2C), rdF32(e + 0x30), rdF32(e + 0x34)};
        ci.numSpheres = rdU16(e + 0x48);
        ci.numBoxes = rdU16(e + 0x4A);
        ci.numTris = rdU16(e + 0x4C);
        ci.arc = arcName;
        std::string nm = colName(e + 8);
        if (!nm.empty()) out.emplace(nm, ci); // first occurrence wins
        pos = end;
    }
}

// DFF model-space AABB over all mesh vertices. ok=false if no vertices.
bool dffAabb(const std::vector<uint8_t>& dff, Vec3& mn, Vec3& mx) {
    DffModel dm = DffLoader::LoadFromMemory(dff.data(), dff.size());
    if (!dm.valid || dm.meshes.empty()) return false;
    mn = {1e30f, 1e30f, 1e30f};
    mx = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (auto& m : dm.meshes) {
        for (auto& v : m.vertices) {
            mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
            mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
            any = true;
        }
    }
    return any;
}

} // namespace

int RunCollisionAudit(ImgLoader& img, const std::string& gameDir,
                      const std::string& outTsvPath) {
    std::fprintf(stderr, "[AUDIT] collision audit starting...\n");

    // 1. Every model from the gta.dat IDEs (whole map), with its flags.
    std::unordered_map<std::string, ModelInfo> models;
    {
        std::ifstream dat(gameDir + "\\data\\gta.dat");
        std::string line;
        int ideFiles = 0;
        while (std::getline(dat, line)) {
            // Lines look like: IDE DATA\MAPS\generic\vegepart.IDE
            size_t sp = line.find_first_not_of(" \t");
            if (sp == std::string::npos || line.compare(sp, 4, "IDE ") != 0) continue;
            std::string rel = line.substr(sp + 4);
            // trim trailing CR/space
            while (!rel.empty() && (rel.back() == '\r' || rel.back() == ' ' || rel.back() == '\t'))
                rel.pop_back();
            std::string path = gameDir + "\\" + rel;
            std::vector<IdeObject> objs;
            std::unordered_map<int, size_t> idx;
            if (!IdeLoader::Load(path, objs, idx)) continue;
            ideFiles++;
            for (auto& o : objs) {
                ModelInfo& mi = models[lower(o.modelName)];
                mi.inIde = true;
                mi.flags = o.flags;
                mi.drawDist = o.drawDistance;
                mi.txd = o.txdName;
            }
        }
        std::fprintf(stderr, "[AUDIT] gta.dat: %d IDE files, %zu models\n",
                     ideFiles, models.size());
    }

    // 2. Every COL3 entry across every .col archive in the IMG.
    std::unordered_map<std::string, ColInfo> cols;
    {
        int arcs = 0;
        for (size_t i = 0; i < img.EntryCount(); i++) {
            const std::string& en = img.GetEntryName(i);
            if (en.size() < 5 || en.compare(en.size() - 4, 4, ".col") != 0) continue;
            std::vector<uint8_t> d = img.Extract(en);
            if (d.empty()) continue;
            scanColArchive(d, en, cols);
            arcs++;
        }
        std::fprintf(stderr, "[AUDIT] %d col archives, %zu unique COL3 entries\n",
                     arcs, cols.size());
    }

    // 3. Compare, per model, over the union of IDE models and COL entries.
    std::vector<std::string> allNames;
    allNames.reserve(models.size() + cols.size());
    for (auto& kv : models) allNames.push_back(kv.first);
    for (auto& kv : cols) if (!models.count(kv.first)) allNames.push_back(kv.first);
    std::sort(allNames.begin(), allNames.end());

    FILE* tsv = std::fopen(outTsvPath.c_str(), "w");
    if (tsv)
        std::fprintf(tsv, "model\tstatus\tseverity\tcenter_off_m\tdff_diag_m\t"
                          "col_diag_m\tfrac_outside\tcol_off_xyz\tnumBox\tnumTri\t"
                          "flags\tcol_arc\tnote\n");

    int nOk = 0, nOffset = 0, nSize = 0, nMissing = 0, nExtra = 0, nNeither = 0, nLodNoCol = 0;

    for (auto& name : allNames) {
        bool isLod = name.find("lod") != std::string::npos;
        auto cit = cols.find(name);
        bool hasCol = cit != cols.end();
        bool hasDff = img.HasFile(name + ".dff");
        const ModelInfo* mi = models.count(name) ? &models[name] : nullptr;
        int flags = mi ? mi->flags : -1;

        Vec3 dmn, dmx, dc, de;
        float dDiag = 0;
        if (hasDff) {
            std::vector<uint8_t> dff = img.Extract(name + ".dff");
            if (!dff.empty() && dffAabb(dff, dmn, dmx)) {
                dc = center(dmn, dmx); de = extent(dmn, dmx); dDiag = diag(de);
            } else {
                hasDff = false; // couldn't parse -> treat as absent for geometry
            }
        }

        const char* status = "OK";
        const char* sev = "info";
        float centerOff = 0, sizeRatio = 0;
        Vec3 colOff{};
        std::string note;

        if (hasCol && hasDff) {
            const Vec3& cmn = cit->second.mn;
            const Vec3& cmx = cit->second.mx;
            Vec3 cc = center(cmn, cmx);
            Vec3 ce = extent(cmn, cmx);
            float cDiag = diag(ce);
            centerOff = dist(cc, dc);
            colOff = {cc.x - dc.x, cc.y - dc.y, cc.z - dc.z};
            sizeRatio = dDiag > 0.01f ? cDiag / dDiag : 0;
            // Collision is routinely a SIMPLIFIED hull: smaller than the art and
            // not perfectly concentric (it omits overhangs, antennae, etc.). So
            // a raw centre-offset flags almost everything. The real defect is
            // collision that sits OFF the model -- i.e. a large fraction of the
            // COL box lies OUTSIDE the DFF box. Measure that via AABB overlap.
            auto ov = [](float amn, float amx, float bmn, float bmx) {
                float lo = std::max(amn, bmn), hi = std::min(amx, bmx);
                return hi > lo ? hi - lo : 0.0f;
            };
            float ix = ov(cmn.x, cmx.x, dmn.x, dmx.x);
            float iy = ov(cmn.y, cmx.y, dmn.y, dmx.y);
            float iz = ov(cmn.z, cmx.z, dmn.z, dmx.z);
            float interVol = ix * iy * iz;
            float colVol = std::max(0.0f, ce.x) * std::max(0.0f, ce.y) * std::max(0.0f, ce.z);
            float outsideFrac = colVol > 1e-6f ? 1.0f - interVol / colVol : 0.0f;
            sizeRatio = outsideFrac; // repurpose TSV column as "frac outside"
            // Flat objects (alpha cards, windows, thin decals) have a near-zero
            // extent, so the volume-overlap fraction is degenerate (a few cm of
            // Z offset reads as "100% outside"). The real defect is a COLLISION
            // whose CENTRE is genuinely displaced from the mesh centre, so flag
            // on the actual centre offset, ignoring flat models.
            float dMinExt = std::min({de.x, de.y, de.z});
            float offThresh = std::max(0.5f, 0.2f * dDiag);
            if (dDiag > 1.0f && dMinExt > 0.5f && centerOff > offThresh) {
                status = "OFFSET"; sev = "warn"; nOffset++;     // collision displaced from the model
            } else if (dDiag > 1.0f && cDiag > 4.0f * dDiag && dMinExt > 0.5f) {
                status = "SIZE_MISMATCH"; sev = "warn"; nSize++; // collision hugely oversized
            } else {
                nOk++;
            }
        } else if (hasDff && !hasCol) {
            if (isLod) {
                status = "LOD_NO_COL"; sev = "info"; note = "LOD; collision from HD";
                nLodNoCol++;
            } else if (dDiag > 5.0f) {
                status = "MISSING_COL"; sev = "warn";
                note = "big model, DFF but no COL"; nMissing++;
            } else {
                status = "no_col_small"; sev = "info"; nOk++;
            }
        } else if (hasCol && !hasDff) {
            status = "EXTRA_COL"; sev = "warn";
            note = "COL but no DFF"; nExtra++;
        } else {
            status = "NO_DFF_NO_COL"; sev = "info"; nNeither++;
        }

        if (tsv) {
            std::fprintf(tsv,
                "%s\t%s\t%s\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f,%.2f,%.2f\t%d\t%d\t%d\t%s\t%s\n",
                name.c_str(), status, sev, centerOff, dDiag,
                hasCol ? diag(extent(cit->second.mn, cit->second.mx)) : 0.0f,
                sizeRatio, colOff.x, colOff.y, colOff.z,
                hasCol ? cit->second.numBoxes : -1,
                hasCol ? cit->second.numTris : -1,
                flags, hasCol ? cit->second.arc.c_str() : "-",
                note.c_str());
        }
    }
    if (tsv) std::fclose(tsv);

    std::fprintf(stderr,
        "[AUDIT] done. OK=%d OFFSET=%d SIZE_MISMATCH=%d MISSING_COL=%d "
        "EXTRA_COL=%d LOD_no_col=%d neither=%d\n",
        nOk, nOffset, nSize, nMissing, nExtra, nLodNoCol, nNeither);
    std::fprintf(stderr, "[AUDIT] TSV: %s\n", outTsvPath.c_str());
    return nOffset + nSize + nMissing + nExtra;
}

// ------------------------------------------------------------- sign audit

namespace {

bool IsSignModel(const std::string& n) {
    // billboards + signs + hoardings + adverts + posters + neon signs.
    static const char* pats[] = {
        "billbd", "billbrd", "billboard", "bboard", "bilbrd", "hoard",
        "advert", "roadsign", "sign_", "_sign", "signage", "sinage",
        "poster", "neon"
    };
    for (auto p : pats) if (n.find(p) != std::string::npos) return true;
    return false;
}

// Lazy txd -> set of lowercased texture keys (parsed once).
struct TxdCache {
    ImgLoader* img;
    std::unordered_map<std::string, std::shared_ptr<std::unordered_set<std::string>>> cache;
    const std::unordered_set<std::string>* get(const std::string& txd) {
        auto it = cache.find(txd);
        if (it != cache.end()) return it->second.get();
        auto set = std::make_shared<std::unordered_set<std::string>>();
        if (img->HasFile(txd + ".txd")) {
            std::vector<uint8_t> raw = img->Extract(txd + ".txd");
            if (!raw.empty()) {
                auto texs = TxdLoader::LoadFromMemory(raw.data(), raw.size());
                for (auto& t : texs) set->insert(TxdLoader::KeyOf(t.name));
            }
        }
        cache[txd] = set;
        return set.get();
    }
};

} // namespace

int RunSignAudit(ImgLoader& img, const std::string& gameDir,
                 const std::string& outTsvPath) {
    std::fprintf(stderr, "[SIGNS] billboard/sign audit starting...\n");

    // models + txds + txd parents from the gta.dat IDEs.
    std::unordered_map<std::string, ModelInfo> models;   // name -> info (has txd)
    std::unordered_map<std::string, std::string> txdParent; // child -> parent
    {
        std::ifstream dat(gameDir + "\\data\\gta.dat");
        std::string line;
        while (std::getline(dat, line)) {
            size_t sp = line.find_first_not_of(" \t");
            if (sp == std::string::npos || line.compare(sp, 4, "IDE ") != 0) continue;
            std::string rel = line.substr(sp + 4);
            while (!rel.empty() && (rel.back()=='\r'||rel.back()==' '||rel.back()=='\t')) rel.pop_back();
            std::vector<IdeObject> objs; std::unordered_map<int,size_t> idx;
            std::vector<TxdParent> parents;
            if (!IdeLoader::Load(gameDir + "\\" + rel, objs, idx, &parents)) continue;
            for (auto& o : objs) { auto& mi = models[lower(o.modelName)]; mi.inIde=true; mi.txd=o.txdName; }
            for (auto& tp : parents) if (!tp.child.empty()&&!tp.parent.empty()) txdParent.emplace(tp.child, tp.parent);
        }
        // dedicated parent files
        for (const char* rel : {"data\\maps\\txd.ide","data\\txdcut.ide"}) {
            std::vector<IdeObject> o; std::unordered_map<int,size_t> i; std::vector<TxdParent> ps;
            if (IdeLoader::Load(gameDir + "\\" + rel, o, i, &ps))
                for (auto& tp : ps) if(!tp.child.empty()&&!tp.parent.empty()) txdParent.emplace(tp.child,tp.parent);
        }
    }

    // COL bounds across all archives (reuse).
    std::unordered_map<std::string, ColInfo> cols;
    for (size_t i = 0; i < img.EntryCount(); i++) {
        const std::string& en = img.GetEntryName(i);
        if (en.size() < 5 || en.compare(en.size()-4,4,".col") != 0) continue;
        std::vector<uint8_t> d = img.Extract(en);
        if (!d.empty()) scanColArchive(d, en, cols);
    }

    std::fprintf(stderr, "[SIGNS] gta.dat models=%zu, txd parents=%zu, col entries=%zu\n",
                 models.size(), txdParent.size(), cols.size());
    TxdCache tc{&img, {}};
    // boot/always-resident txds a texture may legitimately come from.
    static const char* bootTxds[] = {"generic","particle","hud","fonts","misc","grass",
                                     "gb_global","gb_la","gb_vegas","gb_sf","gb_country","gb_generic"};

    // resolve a texture: own txd, then txdp parent chain, then boot txds.
    auto resolves = [&](const std::string& txd, const std::string& texKey) -> bool {
        std::string cur = txd;
        for (int depth = 0; depth < 16; depth++) {
            auto* s = tc.get(cur);
            if (s->count(texKey)) return true;
            auto pit = txdParent.find(cur);
            if (pit == txdParent.end() || pit->second == cur) break;
            cur = pit->second;
        }
        for (auto b : bootTxds) { auto* s = tc.get(b); if (s->count(texKey)) return true; }
        return false;
    };

    FILE* tsv = std::fopen(outTsvPath.c_str(), "w");
    if (tsv) std::fprintf(tsv, "model\ttxd\tstatus\tn_tex\tn_missing\tmissing_tex\t"
                               "col\tcol_diag\tdff_diag\tcol_note\n");

    int nModels=0, nNoDff=0, nTexMiss=0, nColBad=0, nOk=0;
    std::vector<std::string> names;
    for (auto& kv : models) if (IsSignModel(kv.first)) names.push_back(kv.first);
    std::sort(names.begin(), names.end());

    for (auto& name : names) {
        nModels++;
        const std::string& txd = models[name].txd;
        bool hasDff = img.HasFile(name + ".dff");
        if (!hasDff) {
            nNoDff++;
            if (tsv) std::fprintf(tsv, "%s\t%s\tNO_DFF\t0\t0\t-\t-\t0\t0\t-\n", name.c_str(), txd.c_str());
            continue;
        }
        std::vector<uint8_t> dff = img.Extract(name + ".dff");
        DffModel dm = DffLoader::LoadFromMemory(dff.data(), dff.size());
        // unique texture keys this model needs
        std::set<std::string> texs;
        for (auto& m : dm.meshes) if (!m.textureName.empty()) texs.insert(TxdLoader::KeyOf(m.textureName));
        std::vector<std::string> missing;
        for (auto& t : texs) if (!resolves(txd, t)) missing.push_back(t);

        // collision vs DFF bounds
        Vec3 dmn, dmx; bool dok = dffAabb(dff, dmn, dmx);
        float dDiag = dok ? diag(extent(dmn,dmx)) : 0;
        auto cit = cols.find(name);
        const char* col = "no_col"; float cDiag = 0; std::string colNote;
        if (cit != cols.end()) {
            cDiag = diag(extent(cit->second.mn, cit->second.mx));
            if (dok) {
                Vec3 cc = center(cit->second.mn, cit->second.mx), dc = center(dmn, dmx);
                float off = dist(cc, dc);
                // flag collision that is tiny vs the sign, or offset, or oversized
                if (cDiag < 0.25f * dDiag && dDiag > 2.0f) { col="COL_TOO_SMALL"; colNote="collision much smaller than sign"; }
                else if (off > std::max(1.0f, 0.3f*dDiag)) { col="COL_OFFSET"; char b[64]; snprintf(b,sizeof(b),"centre off %.1fm",off); colNote=b; }
                else if (cDiag > 3.0f*dDiag) { col="COL_TOO_BIG"; colNote="collision much larger than sign"; }
                else col="ok";
            } else col="ok";
        }

        bool texBad = !missing.empty();
        bool colBad = (col[0]=='C'); // COL_* flags start with C
        const char* status = texBad && colBad ? "TEX+COL" : texBad ? "TEX_MISSING"
                           : colBad ? "COL_BAD" : "ok";
        if (texBad) nTexMiss++;
        if (colBad) nColBad++;
        if (!texBad && !colBad) nOk++;

        if (tsv) {
            std::string miss;
            for (size_t i=0;i<missing.size();i++){ if(i)miss+=","; miss+=missing[i]; }
            if (miss.empty()) miss="-";
            std::fprintf(tsv, "%s\t%s\t%s\t%zu\t%zu\t%s\t%s\t%.1f\t%.1f\t%s\n",
                name.c_str(), txd.c_str(), status, texs.size(), missing.size(),
                miss.c_str(), col, cDiag, dDiag, colNote.empty()?"-":colNote.c_str());
        }
    }
    if (tsv) std::fclose(tsv);
    std::fprintf(stderr, "[SIGNS] %d sign/billboard models: ok=%d TEX_MISSING=%d COL_BAD=%d NO_DFF=%d\n",
                 nModels, nOk, nTexMiss, nColBad, nNoDff);
    std::fprintf(stderr, "[SIGNS] TSV: %s\n", outTsvPath.c_str());
    return nTexMiss + nColBad;
}
