// Vehicle upgrades: CVehicle::AddVehicleUpgrade / AddUpgrade / CreateUpgradeAtomic for the add-on components
// (bonnet scoops, spoilers, wings, bull bars, lights, roof scoops, nitro bottles). A component's DFF holds one
// atomic; it is placed on the car at the ug_* frame of its slot, under the frame's parent (a door or bumper hinge
// so it moves and flies off with it, else fixed on the body). See Vehicle.h.

#include "Vehicle.h"
#include "PhysicsCore.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "DffLoader.h"
#include "ImgLoader.h"
#include "TxdLoader.h"

using namespace physics;

namespace {

std::string LowerStr(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// CAtomicModelInfo::SetupVehicleUpgradeFlags (aChassisComps): the slot a component name belongs to, by prefix.
// The replacement parts (CAtomicModelInfo's aDummyComps: wheel_, exh_, fbmp_, rbmp_) take the slots 100 .. 103 here;
// -1 for anything else (chassis and misc replacements are not done).
int SlotOfComponent(const std::string& name) {
    static const struct { const char* prefix; int slot; } kPrefix[] = {
        {"bnt_", 0}, {"bntl_", 1}, {"bntr_", 2}, {"spl_", 6}, {"wg_l_", 8}, {"wg_r", 9}, {"fbb_", 10}, {"bbb_", 11},
        {"lgt_", 12}, {"rf_", 14}, {"nto_", 15}};
    const std::string n = LowerStr(name);
    for (const auto& k : kPrefix)
        if (n.compare(0, std::strlen(k.prefix), k.prefix) == 0) return k.slot;
    if (n.compare(0, 6, "wheel_") == 0) return 100;
    if (n.compare(0, 4, "exh_") == 0) return 101;
    if (n.compare(0, 5, "fbmp_") == 0) return 102;
    if (n.compare(0, 5, "rbmp_") == 0) return 103;
    return -1;
}

}  // namespace

std::string Vehicles::GetUpgrade(size_t ci, int slot) const {
    if (ci >= m_cars.size()) return std::string();
    for (const VehicleInstance::Upgrade& u : m_cars[ci].upgrades) if (u.slot == slot) return u.name;
    return std::string();
}

void Vehicles::RemoveUpgrade(D3DRenderer&, size_t ci, int slot) {
    if (ci >= m_cars.size()) return;
    auto& ups = m_cars[ci].upgrades;
    ups.erase(std::remove_if(ups.begin(), ups.end(), [&](const VehicleInstance::Upgrade& u) { return u.slot == slot; }), ups.end());
    if (slot == 15) InstallNitro(ci, -1);   // the bottle goes, the nitro with it
}

bool Vehicles::AddUpgrade(D3DRenderer& r, size_t ci, const std::string& component) {
    if (ci >= m_cars.size() || !m_ready) return false;
    VehicleInstance& c = m_cars[ci];
    auto mi = m_models.find(c.model);
    if (mi == m_models.end()) return false;
    VehicleModel& vm = mi->second;
    const int slot = SlotOfComponent(component);
    if (slot < 0 || (slot < 16 && !vm.upgradePos[slot].found)) return false;

    // CVehicleModelInfo::ms_linkedUpgrades (carmods.dat "link"): a left wing / bonnet half brings its right twin.
    if (m_upgradeLinks.empty()) {
        std::ifstream f(m_dir + "/data/carmods.dat");
        std::string line;
        bool inLink = false;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            if (!inLink) { inLink = line.compare(0, 4, "link") == 0; if (!inLink && (line.compare(0, 4, "mods") == 0 || line.compare(0, 5, "wheel") == 0)) break; continue; }
            if (line.compare(0, 3, "end") == 0) break;
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ls(line);
            std::string a, b;
            if (ls >> a >> b) { m_upgradeLinks[LowerStr(a)] = LowerStr(b); m_upgradeLinks[LowerStr(b)] = LowerStr(a); }
        }
        if (m_upgradeLinks.empty()) m_upgradeLinks["-"] = "-";
    }

    // the component's atomic
    ImgLoader img;
    if (!img.Open(m_dir + "/models/gta3.img")) return false;
    const std::string cname = LowerStr(component);
    const std::vector<uint8_t> dffData = img.Extract(cname + ".dff");
    if (dffData.empty()) return false;
    DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
    if (dff.meshes.empty()) return false;
    // its textures: the car's own dictionary, then the shared vehicle one
    std::vector<TxdTexture> own, shared;
    const std::vector<uint8_t> txdData = img.Extract(vm.info.txd + ".txd");
    if (!txdData.empty()) own = TxdLoader::LoadFromMemory(txdData.data(), txdData.size());
    if (!m_sharedTxd.empty()) shared = TxdLoader::LoadFromMemory(m_sharedTxd.data(), m_sharedTxd.size());
    auto texture = [&](const std::string& name) -> IDirect3DTexture9* {
        const std::string key = TxdLoader::KeyOf(name);
        for (const std::vector<TxdTexture>* set : {&own, &shared})
            for (const TxdTexture& x : *set) {
                if (TxdLoader::KeyOf(x.name) != key) continue;
                IDirect3DTexture9* t = r.CreateTexture(x.width, x.height, x.d3dFormat, x.mip0.data(), (uint32_t)x.mip0.size(), &x.extraMips);
                if (t) vm.textures.push_back(t);
                return t;
            }
        return nullptr;
    };

    RemoveUpgrade(r, ci, slot);
    VehicleInstance::Upgrade up;
    up.name = cname;
    up.slot = slot;
    // CreateUpgradeAtomic at the slot, and again at the damaged twin's frame when the model has one
    static const int kTwin[16] = {3, 4, 5, -1, -1, -1, 7, -1, -1, -1, -1, -1, 13, -1, -1, -1};
    VehicleModel::UpgradePos replacePos;   // where a replacement takes its parent frame's place
    float exhaustFrame[16];
    bool haveExhaust = false;
    if (slot == 101) for (const VehiclePart& vp : vm.parts) if (vp.exhaustOk) { std::memcpy(exhaustFrame, vp.m, sizeof(exhaustFrame)); haveExhaust = true; }
    if ((slot == 101 && !haveExhaust) || (slot >= 102 && !vm.bumpers[slot - 102].found)) return false;
    if (slot >= 100) {
        replacePos.found = true;
        float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        std::memcpy(replacePos.local, ident, sizeof(ident));
        std::memcpy(replacePos.full, ident, sizeof(ident));
        if (slot == 101) std::memcpy(replacePos.full, exhaustFrame, sizeof(replacePos.full));
        if (slot >= 102) {
            const VehicleModel::Bumper& bm = vm.bumpers[slot - 102];
            Mul4(bm.local, bm.base, replacePos.full);
            replacePos.bumper = slot - 102;
        }
    }
    for (const int s : {slot, slot < 16 ? kTwin[slot] : -1}) {
        if (s < 0 || (s < 16 && !vm.upgradePos[s].found)) continue;
        const VehicleModel::UpgradePos& pos = s >= 100 ? replacePos : vm.upgradePos[s];
        const bool damaged = s != slot;
        for (DffMesh& dm : dff.meshes) {
            if (dm.vertices.empty() || dm.indices.empty()) continue;
            // the car's paint on the body-colour materials (green, pink, cyan, magenta), as ApplyPaint does
            const uint32_t mc = dm.materialColor & 0x00FFFFFFu;
            int paintSlot = -1;
            if (mc == 0x003CFF00u) paintSlot = 0; else if (mc == 0x00FF00AFu) paintSlot = 1; else if (mc == 0x0000FFFFu) paintSlot = 2; else if (mc == 0x00FF00FFu) paintSlot = 3;
            uint32_t tint = !dm.textureName.empty() ? 0xFFFFFFFFu : (0xFF000000u | mc);
            if (paintSlot >= 0) {
                const int idx = c.paint[paintSlot];
                const std::array<uint8_t, 3> rgb = idx >= 0 && (size_t)idx < m_colours.size() ? m_colours[(size_t)idx] : std::array<uint8_t, 3>{200, 200, 200};
                tint = 0xFF000000u | ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2];
            }
            const uint32_t matAlpha = dm.materialColor >> 24;   // translucent glass blends
            if (matAlpha < 0xFFu) tint = (matAlpha << 24) | (tint & 0x00FFFFFFu);
            for (DffVertex& v : dm.vertices) v.color = tint;
            D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                               dm.indices.data(), (uint32_t)dm.indices.size());
            if (!mesh) continue;
            mesh->texture = texture(dm.textureName);
            mesh->doubleSided = true;
            mesh->alphaBlend = matAlpha < 0xFFu;
            vm.upgradeMeshes.push_back(mesh);
            VehiclePart p;
            p.mesh = mesh;
            std::memcpy(p.m, pos.full, sizeof(p.m));
            if (pos.door >= 0) {   // it rides the bonnet / boot / door it hangs under
                p.door = pos.door;
                std::memcpy(p.doorLocal, pos.local, sizeof(p.doorLocal));
                p.comp = pos.door == 4 ? 0 : pos.door == 5 ? 1 : pos.door + 2;
                p.damagedMesh = damaged;
            } else if (pos.bumper >= 0) {
                p.bumper = pos.bumper;
                std::memcpy(p.bumperLocal, pos.local, sizeof(p.bumperLocal));
            }
            up.parts.push_back(p);
        }
    }
    if (up.parts.empty()) return false;
    if (slot == 100) {   // fit the new wheel to the car's own tyre size
        float lo = 1e9f, hi = -1e9f;
        for (const DffMesh& dm : dff.meshes) for (const DffVertex& v : dm.vertices) { lo = std::min(lo, v.z); hi = std::max(hi, v.z); }
        if (hi > lo && vm.wheelRadius > 0.0f) up.scale = vm.wheelRadius / (0.5f * (hi - lo));
    }
    c.upgrades.push_back(std::move(up));

    if (slot == 15) {   // SetVehicleUpgradeFlags case 15: the bottle's size sets the boosts (large 5, double 10, small 2)
        InstallNitro(ci, cname == "nto_b_l" ? 5 : cname == "nto_b_tw" ? 10 : 2);
    }
    auto link = m_upgradeLinks.find(cname);
    if (link != m_upgradeLinks.end() && link->second != "-") {
        const std::string other = link->second;
        if (GetUpgrade(ci, SlotOfComponent(other)) != other) AddUpgrade(r, ci, other);
    }
    return true;
}
