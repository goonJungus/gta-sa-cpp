#include "Markers3D.h"

#include "D3DRenderer.h"
#include "DffLoader.h"
#include "ImgLoader.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {
constexpr float kDeg = 0.017453292f;

// CHudColours: RED GREEN BLUEDARK BLUELIGHT LIGHTGRAY BLACK GOLD PURPLE DARKGRAY REDDARK GREENDARK CREAM NIGHTBLUE
// BLUE YELLOW
const uint8_t kHudColours[15][3] = {
    {0xb4, 0x19, 0x1d}, {0x36, 0x68, 0x2c}, {0x32, 0x3c, 0x7f}, {0xac, 0xcb, 0xf1}, {0xe1, 0xe1, 0xe1},
    {0x00, 0x00, 0x00}, {0x90, 0x62, 0x10}, {0xa8, 0x6e, 0xfc}, {0x96, 0x96, 0x96}, {0x68, 0x0f, 0x11},
    {0x26, 0x47, 0x1f}, {0xe2, 0xc0, 0x63}, {0x4a, 0x5a, 0x6b}, {0x14, 0x19, 0xc8}, {0xff, 0xff, 0x00},
};

bool LoadModel(D3DRenderer& r, ImgLoader& img, const char* name, const std::vector<TxdTexture>& txd, Markers3D::Model& out) {
    const std::vector<uint8_t> data = img.Extract(std::string(name) + ".dff");
    if (data.empty()) return false;
    DffModel m = DffLoader::LoadFromMemory(data.data(), data.size());
    float rad = 0.0f;
    for (DffMesh& dm : m.meshes) {
        if (dm.vertices.empty() || dm.indices.empty()) continue;
        for (const DffVertex& v : dm.vertices) rad = std::max(rad, std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
        D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                           dm.indices.data(), (uint32_t)dm.indices.size());
        if (!mesh) continue;
        for (const TxdTexture& t : txd) {   // the markers' textures are in particle.txd
            if (TxdLoader::KeyOf(t.name) != TxdLoader::KeyOf(dm.textureName)) continue;
            IDirect3DTexture9* tex = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
            mesh->texture = tex;
            out.textures.push_back(tex);
            break;
        }
        out.meshes.push_back(mesh);
    }
    if (rad > 0.0f) out.radius = rad;
    return !out.meshes.empty();
}

void Normalise(float v[3]) {
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0.0f) { v[0] /= l; v[1] /= l; v[2] /= l; } else { v[0] = 1.0f; v[1] = v[2] = 0.0f; }
}

uint8_t ToByte(float v) { return (uint8_t)std::clamp(v, 0.0f, 255.0f); }
}

bool Markers3D::Load(D3DRenderer& r, const std::string& gameDir) {
    ImgLoader gta3;
    if (!gta3.Open(gameDir + "\\models\\gta3.img")) return false;
    std::vector<TxdTexture> txd;
    std::ifstream f(gameDir + "\\models\\particle.txd", std::ios::binary);
    const std::vector<uint8_t> td((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!td.empty()) txd = TxdLoader::LoadFromMemory(td.data(), td.size());
    for (const TxdTexture& t : txd)   // CSpecialFX::Init
        if (TxdLoader::KeyOf(t.name) == TxdLoader::KeyOf("finishFlag"))
            m_finishFlag = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
    // C3dMarkers::Init: cylinder for the cylinder and tube, hoop for the torus, diamond_3 for the rest
    const bool ok = LoadModel(r, gta3, "cylinder", txd, m_cylinder) & LoadModel(r, gta3, "hoop", txd, m_hoop) &
                    LoadModel(r, gta3, "diamond_3", txd, m_diamond);
    std::fprintf(stderr, "[MARKERS] models %s\n", ok ? "loaded" : "missing");
    m_loaded = ok;
    return ok;
}

void Markers3D::Shutdown(D3DRenderer& r) {
    for (Model* m : {&m_cylinder, &m_hoop, &m_diamond}) {
        for (D3DRenderMesh* me : m->meshes) r.DestroyMesh(me);
        for (IDirect3DTexture9* t : m->textures) if (t) r.DestroyTexture(t);
        *m = Model();
    }
    if (m_finishFlag) r.DestroyTexture(m_finishFlag);
    m_finishFlag = nullptr;
    m_loaded = false;
}

const Markers3D::Model* Markers3D::ModelOf(int type) const {   // C3dMarker::AddMarker
    switch (type) {
    case CYLINDER: case TUBE: return &m_cylinder;
    case TORUS: return &m_hoop;
    default: return &m_diamond;
    }
}

void Markers3D::Update(float timeStep, const float centre[3], const float cam[3], const float player[3], uint32_t nowMs) {
    m_timeStep = timeStep;
    for (int k = 0; k < 3; k++) { m_centre[k] = centre[k]; m_cam[k] = cam[k]; m_player[k] = player[k]; }
    m_now = nowMs;
    m_angleDiamond += timeStep * 5.0f;   // C3dMarkers::Update
    if (m_angleDiamond > 360.0f) m_angleDiamond -= 360.0f;

    // CCheckpoints::Update: the moving kinds
    for (Checkpoint& c : m_cp) {
        if (!c.used) continue;
        if (c.type == CP_TORUSROT) {   // the hoop's facing wobbles by a tenth each frame
            if (c.multiSize > 0.5f && c.rotFlag) c.rotFlag = false;
            if (c.multiSize < -0.5f) c.rotFlag = true;
            c.multiSize = c.rotFlag ? 0.1f : -0.1f;
            for (float& v : c.fwd) v += c.multiSize;
            Normalise(c.fwd);
        }
        if (c.type == CP_TORUSTHROUGH) {   // flown through: grows a little, then shrinks away
            if (c.multiSize > 0.75f && c.rotFlag) c.rotFlag = false;
            if (c.multiSize < 0.0f) c.rotFlag = true;
            c.multiSize += c.rotFlag ? 0.01f : -0.01f;
            if (c.multiSize < 0.0f) { c.type = 0x101; c.used = false; }
        }
        const float dx = std::fabs(player[0] - c.pos[0]), dy = std::fabs(player[1] - c.pos[1]);
        const bool away = dx * dx + dy * dy > c.size * c.size;
        if (c.type == CP_TORUS_UPDOWN) {   // bobs up and down until the player is under it, then sinks
            if (away) {
                if (c.multiSize > 0.2f && c.rotFlag) c.rotFlag = false;
                if (c.multiSize < -0.2f) c.rotFlag = true;
                c.multiSize += c.rotFlag ? 0.02f : -0.02f;
                c.pos[2] += c.multiSize;
            } else { c.type = CP_TORUS_DOWN; c.multiSize = -0.1f; }
        }
        if (c.type == CP_TORUS_DOWN) {
            if (away) { c.type = CP_TORUS_UPDOWN; c.rotFlag = true; c.multiSize = 0.02f; }
            if (c.pos[2] > player[2] - 0.75f) c.pos[2] += c.multiSize;
        }
    }
}

// C3dMarker::UpdateZCoordinate: once per spot, the ground under the marker (within 100 m of the player).
void Markers3D::UpdateZ(Marker& m, const float point[3], float zDistance) {
    if (m.mapReadX == (int)m.pos[0] && m.mapReadY == (int)m.pos[1]) return;
    const float dx = point[0] - m.pos[0], dy = point[1] - m.pos[1];
    if (dx * dx + dy * dy >= 10000.0f || !hooks.ground) return;
    const float g = hooks.ground(m.pos[0], m.pos[1], m.pos[2] + 1.0f);
    if (g > -1e5f) m.pos[2] = g - zDistance * 0.05f;
    m.mapReadX = (int)m.pos[0];
    m.mapReadY = (int)m.pos[1];
}

void Markers3D::PlaceMarker(uint32_t id, int type, const float posIn[3], float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                            uint16_t, float pulseFraction, int rotateRate, const float dir[3], bool zCheck) {
    if (!m_loaded) return;
    float pos[3] = {posIn[0], posIn[1], posIn[2]};
    float dist = std::sqrt((pos[0] - m_centre[0]) * (pos[0] - m_centre[0]) + (pos[1] - m_centre[1]) * (pos[1] - m_centre[1]));
    const bool coneLike = type == ARROW || type == ARROW2 || type == CONE || type == CONE_NO_COLLISION;
    if (type == TUBE) dist *= 0.25f;
    else if (!coneLike && type != CYLINDER && type != TORUS) return;

    // the marker with this id, a free one, or (cones and arrows) the farthest cone or arrow beyond this one
    Marker* m = nullptr;
    for (Marker& x : m_m) if (x.type != NA && x.id == id) { m = &x; break; }
    if (!m) for (Marker& x : m_m) if (x.type == NA) { m = &x; break; }
    if (!m) {
        if (!coneLike) return;
        for (Marker& x : m_m) {
            const bool xCone = x.type == ARROW || x.type == ARROW2 || x.type == CONE || x.type == CONE_NO_COLLISION;
            if (xCone && dist < x.dist2D && (!m || m->dist2D < x.dist2D)) m = &x;
        }
        if (!m) return;
        m->type = NA;
    }
    m->dist2D = dist;
    auto stdSize = [&](float s) {   // cones and arrows shrink by 30% as the player comes within 5 m
        if (!coneLike) return;
        if (dist >= 25.0f) m->stdSize = s;
        else if (dist <= 5.0f) m->stdSize = s - s * 0.3f;
        else m->stdSize = s - (25.0f - dist) * s * 0.015f;
    };
    auto fadeAlpha = [&](float s) {   // full beyond 12 m outside the marker, 65% within a metre of it (gta_sa.exe 007256cd)
        if (type == ARROW2) return;
        if (dist >= s + 12.0f) m->col[3] = a;
        else if (dist <= s + 1.0f) m->col[3] = ToByte(a * 0.65f);
        else m->col[3] = ToByte(a * (1.0f - (s + 12.0f - dist) * 0.031818185f));
    };

    if (m->id != id || m->type != type) {   // C3dMarker::AddMarker
        *m = Marker();
        m->id = id;
        m->type = type;
        m->dist2D = dist;
        m->size = m->stdSize = size;
        m->col[0] = r; m->col[1] = g; m->col[2] = b; m->col[3] = a;
        m->pulseFraction = pulseFraction;
        m->rotateRate = rotateRate;
        m->onScreenTest = m_now;
        if (type == CONE || type == CONE_NO_COLLISION) pos[2] += std::sin(m_angleDiamond * kDeg) * 0.3f;
        for (int k = 0; k < 3; k++) m->pos[k] = pos[k];
        if (type == CYLINDER) {
            float wz;
            if (hooks.water && hooks.water(m->pos[0], m->pos[1], &wz) && wz >= m->pos[2]) m->pos[2] = wz;
            else UpdateZ(*m, m_centre, size);
        }
        stdSize(size);
        fadeAlpha(size);
        for (int k = 0; k < 3; k++) m->normal[k] = dir[k];
        m->inUse = true;
        return;
    }

    if (type == ARROW || type == CONE) {   // a ceiling just under the cone (a 1.5 m ray up to it, every 2 s) halves it
        const bool fresh = m->lastPos[0] * m->lastPos[0] + m->lastPos[1] * m->lastPos[1] + m->lastPos[2] * m->lastPos[2] < 0.0001f;
        if ((fresh || m_now - m->onScreenTest > 1999) &&
            (m->onScreenTest = m_now, m->lastPos[0] != pos[0] || m->lastPos[1] != pos[1] || m->lastPos[2] != pos[2])) {
            for (int k = 0; k < 3; k++) m->lastPos[k] = pos[k];
            const float from[3] = {pos[0], pos[1], pos[2] - 1.5f};
            float hz;
            m->roof = hooks.buildingLos && hooks.buildingLos(from, pos, &hz) ? hz : 65535.0f;
        }
        if (m->roof < 65535.0f) {
            size *= 0.5f;
            pos[2] = m->roof - ModelOf(type)->radius * 0.1f;
        }
    }
    if (type == CONE || type == CONE_NO_COLLISION) pos[2] += std::sin(m_angleDiamond * kDeg) * (m->roof < 65535.0f ? 0.15f : 0.3f);
    stdSize(size);
    fadeAlpha(size);
    m->size = m->stdSize;   // the pulse term is sin(0): no pulse
    const bool sameSpot = m->mapReadX == (int)m->pos[0] && m->mapReadY == (int)m->pos[1];
    if (m->rotateRate != 0) {   // CMatrix::RotateZ by rate degrees a step
        const float t = (float)m->rotateRate * m_timeStep * kDeg, c = std::cos(t), s = std::sin(t);
        for (auto& row : m->rot) {
            const float x = row[0], y = row[1];
            row[0] = c * x - s * y;
            row[1] = s * x + c * y;
        }
    }
    if (type == ARROW || type == CYLINDER || type == CONE || type == CONE_NO_COLLISION) {
        m->pos[0] = pos[0]; m->pos[1] = pos[1];
        if (!sameSpot) m->pos[2] = pos[2];
    }
    if (type == TORUS || (type == TUBE && !zCheck)) {
        m->pos[0] = pos[0]; m->pos[1] = pos[1]; m->pos[2] = pos[2];
    } else if (type == TUBE) {
        m->pos[0] = pos[0]; m->pos[1] = pos[1];
        if (!sameSpot) m->pos[2] = pos[2];
        UpdateZ(*m, pos, 10.0f);
    } else if (type == CYLINDER) {
        float wz;
        if (hooks.water && hooks.water(m->pos[0], m->pos[1], &wz) && m->pos[2] <= wz) m->pos[2] = wz;
        else UpdateZ(*m, m_centre, size);
    }
    for (int k = 0; k < 3; k++) m->normal[k] = dir[k];
    m->inUse = true;
}

void Markers3D::PlaceMarkerSet(uint32_t id, int type, const float pos[3], float size, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                               uint16_t pulsePeriod, float pulseFraction) {
    // C3dMarkers::PlaceMarkerSet: spins one degree a step; the alpha is scaled by the period's phase in retail,
    // which comes out at the full alpha for the ring and cones it is used for
    const float zero[3] = {0, 0, 0};
    PlaceMarker(id, type, pos, size, r, g, b, a, pulsePeriod, pulseFraction, 1, zero, false);
}

void Markers3D::PlaceMarkerCone(uint32_t id, const float pos[3], float size, uint8_t r, uint8_t g, uint8_t b, uint16_t pulsePeriod,
                                float pulseFraction, int, bool collision) {
    const float dx = pos[0] - m_cam[0], dy = pos[1] - m_cam[1], dz = pos[2] - m_cam[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 1.6f) return;   // not right in front of the camera
    const float zero[3] = {0, 0, 0};
    // m_colDiamond is the alpha; Render draws cones opaque anyway. Retail passes no spin here.
    PlaceMarker(id, collision ? CONE : CONE_NO_COLLISION, pos, size, r, g, b, 255, pulsePeriod, pulseFraction, 0, zero, false);
}

void Markers3D::HighlightArea(uint32_t id, float x1, float y1, float x2, float y2, float z) {
    float c[3] = {(x1 + x2) * 0.5f, (y1 + y2) * 0.5f, z};
    if (z <= -100.0f) c[2] = hooks.ground ? hooks.ground(c[0], c[1], 1000.0f) : 0.0f;
    const float size = std::max(std::fabs(x1 - x2), std::fabs(y1 - y2)) * 0.5f;   // the ring spans the box
    PlaceMarkerSet(id, CYLINDER, c, size, 255, 0, 0, 228, 0x800, 0.1f);
}

int Markers3D::UserMarkerSet(float x, float y, float z, int colour) {
    for (int i = 0; i < 5; i++) {
        UserMarker& u = m_user[i];
        if (u.used) continue;
        u.used = true;
        u.pos[0] = x; u.pos[1] = y; u.pos[2] = z;
        const uint8_t* c = kHudColours[colour >= 0 && colour < 15 ? colour : 0];
        u.rgb[0] = c[0]; u.rgb[1] = c[1]; u.rgb[2] = c[2];
        return i;
    }
    return -1;
}

void Markers3D::UserMarkerDelete(int i) {
    if (i >= 0 && i < 5) m_user[i].used = false;
}

uint32_t Markers3D::AddScriptCheckpoint(int type, const float at[3], const float pointTo[3], float size) {
    Checkpoint* c = nullptr;
    for (Checkpoint& x : m_cp) if (!x.used) { c = &x; break; }
    if (!c) return 0;
    *c = Checkpoint();
    c->used = true;
    c->id = m_nextCpId++;
    c->type = type;
    c->col[0] = 255; c->col[1] = 0; c->col[2] = 0;
    c->col[3] = type <= CP_EMPTYTUBE ? 0x20 : 0x60;
    c->pulsePeriod = 0x400;
    c->size = size;
    for (int k = 0; k < 3; k++) { c->pos[k] = at[k]; c->fwd[k] = pointTo[k] - at[k]; }
    Normalise(c->fwd);
    return c->id;
}

void Markers3D::DeleteCheckpoint(uint32_t id) {
    for (Checkpoint& c : m_cp) if (c.used && c.id == id) { c.used = false; c.type = 0x101; c.id = 0; }
}

void Markers3D::CheckpointPos(uint32_t id, const float pos[3]) {
    for (Checkpoint& c : m_cp) {
        if (!c.used || c.id != id) continue;
        c.pos[0] = pos[0]; c.pos[1] = pos[1];
        if (c.type == CP_TORUS_UPDOWN) c.pos[2] = c.multiSize + pos[2];
        if (c.type != CP_TORUS_DOWN) c.pos[2] = pos[2];
    }
}

void Markers3D::CheckpointHeading(uint32_t id, float headingDeg) {
    for (Checkpoint& c : m_cp) {
        if (!c.used || c.id != id) continue;
        c.fwd[0] = std::cos(headingDeg * kDeg);
        c.fwd[1] = std::sin(headingDeg * kDeg);
        Normalise(c.fwd);
    }
}

// CCheckpoint::Render: the checkpoint as markers of its own id
void Markers3D::CheckpointPlace(Checkpoint& c) {
    const float zero[3] = {0, 0, 0};
    switch (c.type) {
    case CP_TUBE: case CP_ENDTUBE: case CP_EMPTYTUBE: {
        bool zCheck = true;   // on water the column stands on the surface
        float wz;
        if (hooks.water && hooks.water(c.pos[0], c.pos[1], &wz) && c.pos[2] <= wz) { c.pos[2] = wz; zCheck = false; }
        if (c.type == CP_ENDTUBE) {   // the chequered flag three metres up, the checkpoint's size across
            m_flags.push_back({{c.pos[0], c.pos[1], c.pos[2] + 3.0f}, c.size, {c.col[0], c.col[1], c.col[2], c.col[3]}});
        }
        PlaceMarker(c.id, TUBE, c.pos, c.size * 1.5f, c.col[0], c.col[1], c.col[2], 0x80, c.pulsePeriod, c.pulseFraction, 1, zero, zCheck);
        if (c.type == CP_TUBE && m_arrows.size() < 5)   // C3dMarkers::DirectionArrowSet: toward the next checkpoint
            m_arrows.push_back({{c.pos[0], c.pos[1], c.pos[2] + 3.0f}, c.size * 0.625f, {-c.fwd[0], -c.fwd[1], -c.fwd[2]}, {255, 64, 64, 255}});
        break;
    }
    case CP_TORUS: {   // fades out by vanishing once the player is within twice its size
        const float dx = m_player[0] - c.pos[0], dy = m_player[1] - c.pos[1], dz = m_player[2] - c.pos[2];
        if (std::sqrt(dx * dx + dy * dy + dz * dz) > c.size * 2.0f)
            PlaceMarker(c.id, TORUS, c.pos, c.size, c.col[0], c.col[1], c.col[2], c.col[3], c.pulsePeriod, c.pulseFraction, c.rotateRate, c.fwd, false);
        break;
    }
    case CP_TORUS_NOFADE: case CP_TORUSROT: case CP_TORUSTHROUGH: case CP_TORUS_UPDOWN: case CP_TORUS_DOWN:
        // TORUSTHROUGH places three trailing hoops first under the same id; the last placement is the one drawn
        PlaceMarker(c.id, TORUS, c.pos, c.size, c.col[0], c.col[1], c.col[2], c.col[3], c.pulsePeriod, c.pulseFraction, c.rotateRate, c.fwd, false);
        break;
    default: break;
    }
}

void Markers3D::Render(D3DRenderer& r, const float cam[3]) {
    if (!m_loaded) return;
    for (Checkpoint& c : m_cp) if (c.used) CheckpointPlace(c);   // CCheckpoints::Render
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    auto draw = [&](const Model& mo, const float w[4][4], uint32_t argb, bool cullNone) {
        D3DMATRIX out;
        float* o = &out._11;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) o[i * 4 + j] = w[i][j];
        dev->SetTransform(D3DTS_WORLD, &out);
        for (D3DRenderMesh* me : mo.meshes) r.DrawMeshTinted(me, argb, cullNone);
    };

    // C3dMarkers::User3dMarkersDraw: the diamond bobbing a quarter metre, opaque, both faces
    for (const UserMarker& u : m_user) {
        if (!u.used) continue;
        const float w[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0},
                               {u.pos[0], u.pos[1], u.pos[2] + std::sin(m_angleDiamond * kDeg) * 0.25f, 1}};
        draw(m_diamond, w, 0xFF000000u | (uint32_t)u.rgb[0] << 16 | (uint32_t)u.rgb[1] << 8 | u.rgb[2], true);
    }

    for (Marker& m : m_m) {
        if (m.type == NA) continue;
        if (!m.inUse) { m.type = NA; m.id = 0; continue; }   // not placed this frame: gone
        m.inUse = false;
        if (m.dist2D >= 150.0f && m.type != TORUS) continue;
        const float dx = m.pos[0] - cam[0], dy = m.pos[1] - cam[1], dz = m.pos[2] - cam[2];
        if (dx * dx + dy * dy + dz * dz > 600.0f * 600.0f) continue;
        // C3dMarker::Render: the spin, turned to the normal (torus, arrow2), scaled; the tube is 20 tall
        float rm[3][3];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) rm[i][j] = m.rot[i][j];
        if ((m.type == TORUS || m.type == ARROW2) && (m.normal[0] != 0.0f || m.normal[1] != 0.0f || m.normal[2] != 1.0f)) {
            const float* n = m.normal;
            float right[3] = {-n[1], n[0], 0.0f};   // up x normal
            Normalise(right);
            float fwd[3] = {n[1] * right[2] - n[2] * right[1], n[2] * right[0] - n[0] * right[2], n[0] * right[1] - n[1] * right[0]};
            Normalise(fwd);
            const float f[3][3] = {{right[0], right[1], right[2]}, {fwd[0], fwd[1], fwd[2]}, {n[0], n[1], n[2]}};
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) rm[i][j] = m.rot[i][0] * f[0][j] + m.rot[i][1] * f[1][j] + m.rot[i][2] * f[2][j];
        }
        float w[4][4] = {};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) w[i][j] = rm[i][j] * m.size;
        if (m.type == TUBE) for (int j = 0; j < 3; j++) w[2][j] *= 20.0f;
        w[3][0] = m.pos[0]; w[3][1] = m.pos[1]; w[3][2] = m.pos[2]; w[3][3] = 1.0f;
        uint8_t alpha = m.col[3];
        if (m.type == ARROW || m.type == CONE || m.type == CONE_NO_COLLISION) alpha = 255;
        const uint32_t argb = (uint32_t)alpha << 24 | (uint32_t)m.col[0] << 16 | (uint32_t)m.col[1] << 8 | m.col[2];
        draw(*ModelOf(m.type), w, argb, m.type == CYLINDER || m.type == ARROW2);
    }
    DrawArrowsAndFlags(r);
    const D3DMATRIX ident = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    dev->SetTransform(D3DTS_WORLD, &ident);
}

// C3dMarkers::DirectionArrowsDraw: the diamond turned to the arrow's direction, once black at its size and once in
// its colour at 0.8 of it (a dark rim), both faces, opaque. Then the finish flags: camera-facing sprites.
void Markers3D::DrawArrowsAndFlags(D3DRenderer& r) {
    IDirect3DDevice9* dev = r.GetDevice();
    for (const Arrow& a : m_arrows) {
        float f[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        const float* n = a.dir;
        if (n[0] != 0.0f || n[1] != 0.0f || n[2] != 1.0f) {
            float right[3] = {-n[1], n[0], 0.0f};   // (0,0,1) x normal
            Normalise(right);
            float fwd[3] = {n[1] * right[2] - n[2] * right[1], n[2] * right[0] - n[0] * right[2], n[0] * right[1] - n[1] * right[0]};
            Normalise(fwd);
            for (int j = 0; j < 3; j++) { f[0][j] = right[j]; f[1][j] = fwd[j]; f[2][j] = n[j]; }
        }
        for (int pass = 0; pass < 2; pass++) {
            const float sc = a.size * (pass ? 0.8f : 1.0f);
            D3DMATRIX out = {};
            float* o = &out._11;
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) o[i * 4 + j] = f[i][j] * sc;
            out._41 = a.pos[0]; out._42 = a.pos[1]; out._43 = a.pos[2]; out._44 = 1.0f;
            dev->SetTransform(D3DTS_WORLD, &out);
            const uint32_t argb = pass ? (0xFF000000u | (uint32_t)a.rgba[0] << 16 | (uint32_t)a.rgba[1] << 8 | a.rgba[2]) : 0xFF000000u;
            for (D3DRenderMesh* me : m_diamond.meshes) r.DrawMeshTinted(me, argb, true);
        }
    }
    m_arrows.clear();
    if (!m_flags.empty() && m_finishFlag) {
        const D3DMATRIX ident = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        dev->SetTransform(D3DTS_WORLD, &ident);
        const D3DMATRIX& v = r.GetViewMatrix();
        const float right[3] = {v._11, v._21, v._31}, up[3] = {v._12, v._22, v._32};
        std::vector<D3DRenderer::ColorTexVertex> verts;
        std::vector<uint16_t> idx;
        for (const Flag& fl : m_flags) {
            const float h = fl.size * 0.5f;   // RenderOneXLUSprite's half size: the checkpoint's size across
            const uint32_t col = (uint32_t)fl.rgba[3] << 24 | (uint32_t)fl.rgba[0] << 16 | (uint32_t)fl.rgba[1] << 8 | fl.rgba[2];
            const uint16_t b = (uint16_t)verts.size();
            const float sx[4] = {-1, 1, 1, -1}, sy[4] = {1, 1, -1, -1}, tu[4] = {0, 1, 1, 0}, tv[4] = {0, 0, 1, 1};
            for (int k = 0; k < 4; k++)
                verts.push_back({fl.pos[0] + (right[0] * sx[k] + up[0] * sy[k]) * h, fl.pos[1] + (right[1] * sx[k] + up[1] * sy[k]) * h,
                                 fl.pos[2] + (right[2] * sx[k] + up[2] * sy[k]) * h, col, tu[k], tv[k]});
            for (uint16_t k : {0, 1, 2, 0, 2, 3}) idx.push_back((uint16_t)(b + k));
        }
        r.DrawBlendedTriangles(verts.data(), (uint32_t)verts.size(), idx.data(), (uint32_t)idx.size(), m_finishFlag);
    }
    m_flags.clear();
}
