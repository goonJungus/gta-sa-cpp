// gta_world.cpp - concrete IGtaWorld + module entry points.
//
// Owns the opened IMG archive, the parsed world (IDE objects + IPL placements),
// and the subsystem implementations. Brings everything up in init() from the
// user's own install and drives the subsystems in update().

#include "gta_sa/gta_sa_module.hpp"
#include "gta_sa/peds.hpp"           // CJStats
#include "world/img_archive.hpp"
#include "world/map_data.hpp"
#include "world/world_streaming_impl.hpp"
#include "peds/ped_manager.hpp"
#include "vehicles/vehicle_manager.hpp"
#include "audio/radio_player.hpp"
#include "scripts/script_machine.hpp"

#include <string>
#include <vector>

namespace gta {

const char* toString(Status s) {
    switch (s) {
        case Status::Ok:               return "Ok";
        case Status::NotConfigured:    return "NotConfigured";
        case Status::ImgMissing:       return "ImgMissing";
        case Status::DatMissing:       return "DatMissing";
        case Status::ReadFailed:       return "ReadFailed";
        case Status::ParseFailed:      return "ParseFailed";
        case Status::TooManyInstances: return "TooManyInstances";
    }
    return "Unknown";
}

namespace {

class GtaWorld final : public IGtaWorld {
public:
    ~GtaWorld() override { shutdown(); }

    Status init(const GtaConfig& config) override {
        using enum Status;   // NotConfigured / ImgMissing / ... unqualified below
        if (m_init) shutdown();
        m_config = config;
        if (m_config.gameDirectory.empty()) return NotConfigured;
        log("gta_sa: init from " + m_config.gameDirectory);

        // 1. Open the user's IMG archive.
        const std::string imgPath = m_config.gameDirectory + "/models/gta3.img";
        if (!m_img.Open(imgPath)) return ImgMissing;
        log("gta_sa: IMG opened, " + std::to_string(m_img.EntryCount()) + " entries");

        // 2. gta.dat lists the IDE/IPL files.
        std::vector<std::string> ideRel, iplRel;
        if (!ParseGtaDat(m_config.gameDirectory, ideRel, iplRel)) return DatMissing;

        // 3. Object definitions (IDE).
        std::vector<ObjectDef> objects;
        for (const auto& rel : ideRel)
            LoadIde(m_config.gameDirectory + "/" + rel, objects);

        // 4. Placements: text IPLs on disk...
        std::vector<Placement> placements;
        for (const auto& rel : iplRel)
            LoadIplText(m_config.gameDirectory + "/" + rel, placements);
        // ...plus the binary "stream" IPLs packed inside the IMG.
        for (std::size_t i = 0; i < m_img.EntryCount(); i++) {
            const std::string& en = m_img.EntryName(i);
            if (en.size() < 5 || en.compare(en.size() - 4, 4, ".ipl") != 0) continue;
            std::vector<uint8_t> raw = m_img.Extract(en);
            if (!raw.empty()) LoadIplBinary(raw.data(), raw.size(), placements);
        }

        if (m_config.maxInstances != 0 && placements.size() > m_config.maxInstances)
            return TooManyInstances;

        // 5. Hand the parsed world to the streaming subsystem.
        m_streaming.Build(&m_img, std::move(objects), std::move(placements),
                          m_config.streamRadius);
        log("gta_sa: " + std::to_string(m_streaming.ObjectCount()) + " objects, " +
            std::to_string(m_streaming.PlacementCount()) + " placements");

        // 6. Load the mission script image (non-fatal if absent).
        m_scripts.LoadScript(m_config.gameDirectory + "/data/script/main.scm");

        // 7. loadCollision/loadPaths: the .col archives live inside the IMG and
        //    are handed to the host via IWorldStreaming::ReadAsset; a dedicated
        //    collision/path query API is a later pass.
        m_init = true;
        return Ok;
    }

    void update(float dt) override {
        if (!m_init) return;
        m_vehicles.Update(dt);
        m_peds.Update(dt);
        m_radio.Update(dt);
        m_scripts.Update(dt);
    }

    void shutdown() override {
        if (!m_init) return;
        m_img.Close();
        m_init = false;
    }

    IWorldStreaming* streaming() override { return &m_streaming; }
    IPedManager*     peds()      override { return &m_peds; }
    IVehicleManager* vehicles()  override { return &m_vehicles; }
    IRadioPlayer*    radio()     override { return &m_radio; }
    IScriptMachine*  scripts()   override { return &m_scripts; }
    CJStats*         player()    override { return &m_player; }

private:
    void log(const std::string& m) const {
        if (m_config.logSink) m_config.logSink(m.c_str());
    }

    GtaConfig m_config;
    ImgArchive m_img;
    WorldStreaming m_streaming;
    PedManager m_peds;
    VehicleManager m_vehicles;
    RadioPlayer m_radio;
    ScriptMachine m_scripts;
    CJStats m_player;
    bool m_init = false;
};

}  // namespace

IGtaWorld* createGtaWorld() { return new GtaWorld(); }

void destroyGtaWorld(IGtaWorld* world) {
    if (!world) return;
    world->shutdown();
    delete world;
}

ModuleVersion moduleVersion() { return {0, 1, 0}; }

}  // namespace gta
