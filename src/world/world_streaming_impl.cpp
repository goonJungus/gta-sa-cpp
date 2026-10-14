// world_streaming_impl.cpp - see world_streaming_impl.hpp.

#include "world/world_streaming_impl.hpp"

namespace gta {

void WorldStreaming::Build(const ImgArchive* img, std::vector<ObjectDef> objects,
                           std::vector<Placement> placements,
                           float streamRadius) {
    m_img = img;
    m_objects = std::move(objects);
    m_placements = std::move(placements);
    if (streamRadius > 0.0f) m_radius = streamRadius;

    m_byId.clear();
    m_byName.clear();
    m_byId.reserve(m_objects.size());
    m_byName.reserve(m_objects.size());
    for (std::size_t i = 0; i < m_objects.size(); i++) {
        // First definition wins (matches retail IDE merge order).
        m_byId.emplace(m_objects[i].id, i);
        m_byName.emplace(m_objects[i].modelName, i);
    }
}

const ObjectDef* WorldStreaming::FindObject(int32_t modelId) const {
    auto it = m_byId.find(modelId);
    return it == m_byId.end() ? nullptr : &m_objects[it->second];
}

const ObjectDef* WorldStreaming::FindObjectByName(const std::string& modelName) const {
    auto it = m_byName.find(modelName);
    return it == m_byName.end() ? nullptr : &m_objects[it->second];
}

std::vector<uint8_t> WorldStreaming::ReadAsset(const std::string& fileName) const {
    if (!m_img) return {};
    return m_img->Extract(fileName);
}

std::vector<std::string> WorldStreaming::ListAssets(const std::string& ext) const {
    std::vector<std::string> out;
    if (!m_img) return out;
    for (std::size_t i = 0; i < m_img->EntryCount(); i++) {
        const std::string& n = m_img->EntryName(i);
        if (n.size() >= ext.size() &&
            n.compare(n.size() - ext.size(), ext.size(), ext) == 0)
            out.push_back(n);
    }
    return out;
}

std::size_t WorldStreaming::QueryNear(std::vector<std::size_t>& out) const {
    const float r2 = m_radius * m_radius;
    std::size_t n = 0;
    for (std::size_t i = 0; i < m_placements.size(); i++) {
        const Vec3& p = m_placements[i].position;
        const float dx = p.x - m_focus.x, dy = p.y - m_focus.y;
        if (dx * dx + dy * dy <= r2) { out.push_back(i); n++; }
    }
    return n;
}

}  // namespace gta
