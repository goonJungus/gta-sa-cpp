// world_streaming_impl.hpp - concrete IWorldStreaming (private).

#pragma once

#include "gta_sa/world_streaming.hpp"
#include "world/img_archive.hpp"

#include <unordered_map>

namespace gta {

class WorldStreaming final : public IWorldStreaming {
public:
    // Take ownership of the parsed world and build lookup indices. 'img' is the
    // opened game archive, borrowed (owned by GtaWorld) for ReadAsset.
    void Build(const ImgArchive* img, std::vector<ObjectDef> objects,
               std::vector<Placement> placements, float streamRadius);

    void SetFocus(const Vec3& worldPos) override { m_focus = worldPos; }
    Vec3 Focus() const override { return m_focus; }

    const ObjectDef* FindObject(int32_t modelId) const override;
    const ObjectDef* FindObjectByName(const std::string& modelName) const override;

    const std::vector<Placement>& Placements() const override { return m_placements; }
    std::size_t QueryNear(std::vector<std::size_t>& out) const override;
    std::vector<uint8_t> ReadAsset(const std::string& fileName) const override;
    std::vector<std::string> ListAssets(const std::string& extension) const override;

    std::size_t ObjectCount() const override { return m_objects.size(); }
    std::size_t PlacementCount() const override { return m_placements.size(); }

private:
    const ImgArchive* m_img = nullptr;
    std::vector<ObjectDef> m_objects;
    std::vector<Placement> m_placements;
    std::unordered_map<int32_t, std::size_t> m_byId;
    std::unordered_map<std::string, std::size_t> m_byName;
    Vec3 m_focus;
    float m_radius = 300.0f;
};

}  // namespace gta
