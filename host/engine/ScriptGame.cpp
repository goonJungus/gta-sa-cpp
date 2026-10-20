// ScriptGame: see ScriptGame.h. Command numbers and argument orders follow the retail script
// command table (and the gta-reversed handlers); each case names its command.

#include "ScriptGame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Peds.h"
#include "Pickups.h"
#include "EntryExit.h"
#include "Stats.h"
#include "Vehicle.h"

namespace {
float Dist3(float ax, float ay, float az, float bx, float by, float bz) {
    const float dx = ax - bx, dy = ay - by, dz = az - bz;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
constexpr int kPlayer = 1;   // the player's ped handle
const bool g_dbg = std::getenv("GTA_SCMGAME") != nullptr;   // dev: log every handled command
}

ScriptTextItem ScriptGame::DefaultStyle() const {
    ScriptTextItem s;   // CFont defaults after SetScale(0.48, 1.12) at the start of a text frame
    return s;
}

std::string ScriptGame::Text(const std::string& key) {
    return hud ? hud->Text().Get(key.c_str()) : key;
}

int ScriptGame::CharIndex(int uid) const { return peds && uid != kPlayer ? peds->FindUid(uid) : -1; }

bool ScriptGame::CharPos(int uid, float& x, float& y, float& z) const {
    if (uid == kPlayer) {
        if (!scm || !scm->host.playerPos) return false;
        scm->host.playerPos(x, y, z);
        return true;
    }
    const int i = CharIndex(uid);
    if (i < 0) return false;
    x = peds->X((size_t)i); y = peds->Y((size_t)i); z = peds->Z((size_t)i);
    return true;
}

void ScriptGame::AddText(float x, float y, const std::string& text) {
    ScriptTextItem it = m_style;
    it.x = x; it.y = y; it.text = text;
    m_queue.push_back(it);
    m_style = DefaultStyle();
}

static std::string WithNumbers(std::string s, const int* n, int count) {
    for (int i = 0; i < count; i++) {   // "~1~" is the first number, "~1~"... in GXT; the script numbers are substituted in order
        const std::string key = "~1~";
        const size_t p = s.find(key);
        if (p == std::string::npos) break;
        s.replace(p, key.size(), std::to_string(n[i]));
    }
    return s;
}

bool ScriptGame::Handle(ScmMachine::Call& c) {
    auto car = [&](int h) -> VehicleInstance* { return cars && h >= 1 && (size_t)h <= cars->Count() ? &cars->Car((size_t)h - 1) : nullptr; };
    auto carAlive = [&](int h) -> VehicleInstance* { VehicleInstance* v = car(h); return v && !v->removed ? v : nullptr; };
    if (g_dbg) std::fprintf(stderr, "[SCMGAME] %04X\n", c.op);
    switch (c.op) {
    // ---- fade, widescreen, control -------------------------------------------------------
    case 0x0169: m_fadeRgb[0] = (float)c.I(0); m_fadeRgb[1] = (float)c.I(1); m_fadeRgb[2] = (float)c.I(2); return true;   // SET_FADING_COLOUR
    case 0x016A: {   // DO_FADE(ms, direction): 1 fades the picture in, 0 fades it out
        m_fadeFrom = fade;
        m_fadeTo = c.I(1) ? 0.0f : 1.0f;
        m_fadeDur = std::max(0.0f, c.I(0) / 1000.0f);
        m_fadeT = 0;
        m_fading = m_fadeDur > 0.0f;
        if (!m_fading) fade = m_fadeTo;
        return true;
    }
    case 0x016B: c.result = m_fading; return true;   // GET_FADING_STATUS
    case 0x02A3: wideTarget = c.I(0) != 0; return true;   // SWITCH_WIDESCREEN
    case 0x01B4: playerControl = c.I(1) != 0; return true;   // SET_PLAYER_CONTROL(player, on)
    case 0x09E7: c.result = playerControl; return true;      // IS_PLAYER_CONTROL_ON
    case 0x09C8: c.result = true; return true;               // ARE_SUBTITLES_SWITCHED_ON
    // ---- text ---------------------------------------------------------------------------
    case 0x03F0: return true;                                 // USE_TEXT_COMMANDS
    case 0x033F: m_style.sx = c.F(0); m_style.sy = c.F(1); return true;   // SET_TEXT_SCALE
    case 0x0340: m_style.colour = ((uint32_t)std::clamp(c.I(3), 0, 255) << 24) | ((uint32_t)std::clamp(c.I(0), 0, 255) << 16) |
                                  ((uint32_t)std::clamp(c.I(1), 0, 255) << 8) | (uint32_t)std::clamp(c.I(2), 0, 255); return true;   // SET_TEXT_COLOUR
    case 0x0341: m_style.justify = c.I(0) != 0; m_style.right = false; return true;   // SET_TEXT_JUSTIFY
    case 0x0342: m_style.centre = c.I(0) != 0; return true;                           // SET_TEXT_CENTRE
    case 0x0343: m_style.wrap = c.F(0); return true;                                  // SET_TEXT_WRAPX
    case 0x0344: return true;                                                          // SET_TEXT_CENTRE_SIZE
    case 0x0345: case 0x0346: case 0x0347: case 0x0348: return true;                  // background, proportional
    case 0x0349: m_style.font = c.I(0); return true;                                  // SET_TEXT_FONT
    case 0x03E0: m_style.beforeFade = c.I(0) != 0; return true;                       // SET_TEXT_DRAW_BEFORE_FADE
    case 0x03E3: return true;                                                          // SET_SPRITES_DRAW_BEFORE_FADE
    case 0x03E4: m_style.right = c.I(0) != 0; return true;                            // SET_TEXT_RIGHT_JUSTIFY
    case 0x060D:                                                                       // SET_TEXT_DROPSHADOW(size, r, g, b, a)
        m_style.shadow = c.I(0);
        m_style.shadowColour = ((uint32_t)std::clamp(c.I(4), 0, 255) << 24) | ((uint32_t)std::clamp(c.I(1), 0, 255) << 16) |
                               ((uint32_t)std::clamp(c.I(2), 0, 255) << 8) | (uint32_t)std::clamp(c.I(3), 0, 255);
        return true;
    case 0x081C: return true;                                                          // SET_TEXT_EDGE
    case 0x033E: AddText(c.F(0), c.F(1), Text(c.S(2))); return true;                   // DISPLAY_TEXT(x, y, label)
    case 0x045A: {                                                                      // DISPLAY_TEXT_WITH_NUMBER(x, y, label, n)
        const int n[1] = {c.I(3)};
        AddText(c.F(0), c.F(1), WithNumbers(Text(c.S(2)), n, 1));
        return true;
    }
    case 0x054C: if (hud) hud->Text().LoadMission(c.S(0).c_str()); return true;       // LOAD_MISSION_TEXT
    case 0x05A9: case 0x05AA: c.OutStr(0, c.S(1)); return true;                       // SET_VAR_TEXT_LABEL / SET_LVAR_TEXT_LABEL
    // ---- prints -------------------------------------------------------------------------
    case 0x00BA: case 0x00BB: case 0x00BC: case 0x00BD: case 0x0217: {                // PRINT_BIG / PRINT / PRINT_NOW / PRINT_SOON / PRINT_BIG_Q (label, ms, flag)
        m_subtitle = Text(c.S(0));
        m_subTimer = std::max(0.1f, c.I(1) / 1000.0f);
        return true;
    }
    case 0x01E4: case 0x01E3: {                                                         // PRINT_WITH_NUMBER(_BIG)(label, n, ms, flag)
        const int n[1] = {c.I(1)};
        m_subtitle = WithNumbers(Text(c.S(0)), n, 1);
        m_subTimer = std::max(0.1f, c.I(2) / 1000.0f);
        return true;
    }
    case 0x00BE: m_subtitle.clear(); m_subTimer = 0; return true;                      // CLEAR_PRINTS
    case 0x03D5: m_subtitle.clear(); m_subTimer = 0; return true;                      // CLEAR_THIS_PRINT(label)
    case 0x03E5: if (hud) hud->ShowMessage(Text(c.S(0)), 8.0f); return true;           // PRINT_HELP(label)
    case 0x03E6: if (hud) hud->ShowMessage("", 0.0f); return true;                      // CLEAR_HELP
    // ---- time and random ----------------------------------------------------------------
    case 0x01BD: c.Out(0, scm ? scm->Now() : 0); return true;                           // GET_GAME_TIMER
    case 0x0208: {                                                                      // GENERATE_RANDOM_FLOAT_IN_RANGE(lo, hi) -> f
        const float lo = c.F(0), hi = c.F(1);
        c.OutF(2, lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX));
        return true;
    }
    case 0x0209: {                                                                      // GENERATE_RANDOM_INT_IN_RANGE(lo, hi) -> n
        const int lo = c.I(0), hi = c.I(1);
        c.Out(2, hi > lo ? lo + std::rand() % (hi - lo) : lo);
        return true;
    }
    // ---- camera -------------------------------------------------------------------------
    case 0x015F:   // SET_FIXED_CAMERA_POSITION(x, y, z, offx, offy, offz)
        cam.active = true; cam.pos[0] = c.F(0); cam.pos[1] = c.F(1); cam.pos[2] = c.F(2);
        cam.moveOn = false;
        return true;
    case 0x0160: {   // POINT_CAMERA_AT_POINT(x, y, z, switch type); z <= -100 means "the ground there"
        cam.active = true; cam.look[0] = c.F(0); cam.look[1] = c.F(1); cam.look[2] = c.F(2);
        if (cam.look[2] <= -100.0f && groundAt) { const float g = groundAt(cam.look[0], cam.look[1]); if (g > -1e5f) cam.look[2] = g; }
        cam.trackOn = false;
        return true;
    }
    case 0x0936:   // CAMERA_SET_VECTOR_MOVE(from xyz, to xyz, ms, ease)
        cam.active = true; cam.moveOn = true; cam.moveT = 0; cam.moveDur = std::max(0.001f, c.I(6) / 1000.0f);
        for (int k = 0; k < 3; k++) { cam.moveFrom[k] = c.F((size_t)k); cam.moveTo[k] = c.F((size_t)k + 3); cam.pos[k] = cam.moveFrom[k]; }
        return true;
    case 0x0920:   // CAMERA_SET_VECTOR_TRACK(from xyz, to xyz, ms, ease)
        cam.active = true; cam.trackOn = true; cam.trackT = 0; cam.trackDur = std::max(0.001f, c.I(6) / 1000.0f);
        for (int k = 0; k < 3; k++) { cam.trackFrom[k] = c.F((size_t)k); cam.trackTo[k] = c.F((size_t)k + 3); cam.look[k] = cam.trackFrom[k]; }
        return true;
    case 0x092F: cam.persistTrack = c.I(0) != 0; return true;   // CAMERA_PERSIST_TRACK
    case 0x0930: cam.persistPos = c.I(0) != 0; return true;     // CAMERA_PERSIST_POS
    case 0x0931: cam.persistFov = c.I(0) != 0; return true;     // CAMERA_PERSIST_FOV
    case 0x0925: cam.moveOn = cam.trackOn = false; cam.persistPos = cam.persistTrack = cam.persistFov = false; return true;   // CAMERA_RESET_NEW_SCRIPTABLES
    case 0x015A: case 0x02EB: case 0x0373:   // RESTORE_CAMERA / RESTORE_CAMERA_JUMPCUT / SET_CAMERA_BEHIND_PLAYER
        cam = ScriptCamera{};
        return true;
    case 0x099C:   // CAMERA_SET_SHAKE_SIMULATION_SIMPLE(type, ms, intensity)
        cam.shakeDur = cam.shakeT = std::max(0.0f, c.I(1) / 1000.0f); cam.shakeAmp = c.F(2);
        return true;
    case 0x041D: cam.nearClip = c.F(0); return true;   // SET_NEAR_CLIP
    // ---- models and streaming: everything is resident ---------------------------------
    case 0x0247: case 0x0249: case 0x07C0: case 0x04E4: case 0x03CB: case 0x0A0B: case 0x0776: case 0x0395: return true;   // REQUEST_MODEL, MARK_..., REQUEST_CAR_RECORDING, REQUEST_COLLISION, LOAD_SCENE, LOAD_SCENE_IN_DIRECTION, REQUEST_IPL, CLEAR_AREA
    case 0x0248: case 0x07C1: case 0x023D: c.result = true; return true;   // HAS_MODEL_LOADED, HAS_CAR_RECORDING_BEEN_LOADED, HAS_SPECIAL_CHARACTER_LOADED
    case 0x023C: {   // LOAD_SPECIAL_CHARACTER(slot, name)
        const size_t slot = (size_t)std::max(1, c.I(0));
        if (m_special.size() < slot) m_special.resize(slot);
        m_special[slot - 1] = c.S(1);
        return true;
    }
    case 0x0296: return true;   // UNLOAD_SPECIAL_CHARACTER
    // ---- cutscenes: loading and timing only (the player is Phase B) -----------------------
    case 0x02E4: m_cut = Cutscene{}; m_cut.name = c.S(0); m_cut.state = 1; return true;   // LOAD_CUTSCENE
    case 0x06B9: c.result = m_cut.state >= 1; return true;                                  // HAS_CUTSCENE_LOADED
    case 0x02E7: m_cut.state = 2; m_cut.t = 0; m_cut.dur = 6.0f; return true;              // START_CUTSCENE
    case 0x02E9: c.result = m_cut.state == 3; return true;                                  // HAS_CUTSCENE_FINISHED
    case 0x02EA: m_cut = Cutscene{}; return true;                                           // CLEAR_CUTSCENE
    case 0x056A: c.result = m_cut.skipped; return true;                                     // WAS_CUTSCENE_SKIPPED
    case 0x0707: case 0x0701: return true;                                                  // SKIP_CUTSCENE_START_INTERNAL / SKIP_CUTSCENE_END
    // ---- characters ---------------------------------------------------------------------
    case 0x009A: {   // CREATE_CHAR(pedtype, model, x, y, z) -> char
        if (!peds || !r) { c.Out(5, 0); return true; }
        float z = c.F(4);
        if (groundAt) { const float g = groundAt(c.F(2), c.F(3)); if (g > -1e5f && z < g + 0.2f) z = g; }
        const int i = peds->Spawn(*r, peds->HasModel(c.I(1)) ? c.I(1) : -1, c.F(2), c.F(3), z, 0.0f);
        if (i < 0) { c.Out(5, 0); return true; }
        peds->SetScripted((size_t)i, true);
        c.Out(5, peds->Uid((size_t)i));
        return true;
    }
    case 0x009B: {   // DELETE_CHAR(char)
        const int i = CharIndex(c.I(0));
        if (i >= 0) peds->Remove((size_t)i);
        m_charCar.erase(c.I(0));
        return true;
    }
    case 0x0129: case 0x01C8: case 0x0560: {   // CREATE_CHAR_INSIDE_CAR(car, type, model) / CREATE_CHAR_AS_PASSENGER(car, type, model, seat) / CREATE_RANDOM_CHAR_AS_DRIVER(car)
        VehicleInstance* v = carAlive(c.I(0));
        const size_t outArg = c.op == 0x0129 ? 3 : (c.op == 0x01C8 ? 4 : 1);
        if (!v || !peds || !r) { c.Out(outArg, 0); return true; }
        const int model = c.op == 0x0560 ? -1 : (peds->HasModel(c.I(2)) ? c.I(2) : -1);
        const int i = peds->Spawn(*r, model, v->x, v->y, v->z, v->heading);
        if (i < 0) { c.Out(outArg, 0); return true; }
        peds->SetScripted((size_t)i, true);
        peds->SetHidden((size_t)i, true);
        const int uid = peds->Uid((size_t)i);
        m_charCar[uid] = c.I(0);
        c.Out(outArg, uid);
        return true;
    }
    case 0x00DB: {   // IS_CHAR_IN_CAR(char, car)
        if (c.I(0) == kPlayer) c.result = playerCar && playerCar() == c.I(1) - 1;
        else { const auto it = m_charCar.find(c.I(0)); c.result = it != m_charCar.end() && it->second == c.I(1); }
        return true;
    }
    case 0x00DF: {   // IS_CHAR_IN_ANY_CAR(char)
        c.result = c.I(0) == kPlayer ? (playerInCar && playerInCar()) : m_charCar.count(c.I(0)) != 0;
        return true;
    }
    case 0x0430: {   // WARP_CHAR_INTO_CAR_AS_PASSENGER(char, car, seat)
        if (c.I(0) == kPlayer) { if (putPlayerInCar) putPlayerInCar(c.I(1) - 1); return true; }
        const int i = CharIndex(c.I(0));
        if (i >= 0 && carAlive(c.I(1))) { peds->SetHidden((size_t)i, true); m_charCar[c.I(0)] = c.I(1); }
        return true;
    }
    case 0x0362: {   // WARP_CHAR_FROM_CAR_TO_COORD(char, x, y, z)
        m_charCar.erase(c.I(0));
        if (c.I(0) == kPlayer) {
            if (playerLeaveCar) playerLeaveCar();
            if (scm && scm->host.setPlayerPos) scm->host.setPlayerPos(c.F(1), c.F(2), c.F(3));
            return true;
        }
        const int i = CharIndex(c.I(0));
        if (i >= 0) { peds->SetHidden((size_t)i, false); peds->SetPos((size_t)i, c.F(1), c.F(2), c.F(3)); }
        return true;
    }
    case 0x0622: {   // TASK_LEAVE_CAR_IMMEDIATELY(char, car)
        const auto it = m_charCar.find(c.I(0));
        if (c.I(0) == kPlayer) { if (playerLeaveCar) playerLeaveCar(); return true; }
        const int i = CharIndex(c.I(0));
        if (i >= 0 && it != m_charCar.end()) {
            VehicleInstance* v = car(it->second);
            if (v) peds->SetPos((size_t)i, v->x - std::cos(v->heading) * 1.4f, v->y - std::sin(v->heading) * 1.4f, v->z);
            peds->SetHidden((size_t)i, false);
            m_charCar.erase(it);
        }
        return true;
    }
    case 0x05D3: {   // TASK_GO_STRAIGHT_TO_COORD(char, x, y, z, mode, time)
        const int i = CharIndex(c.I(0));
        if (i >= 0) peds->SetGoal((size_t)i, c.F(1), c.F(2), c.I(4));
        return true;
    }
    case 0x0792: case 0x0687: {   // CLEAR_CHAR_TASKS(_IMMEDIATELY)(char)
        const int i = CharIndex(c.I(0));
        if (i >= 0) { peds->ClearGoal((size_t)i); }
        m_drives.erase(c.I(0));
        return true;
    }
    case 0x05D1: {   // TASK_CAR_DRIVE_TO_COORD(char, car, x, y, z, speed, mode, model, behaviour)
        m_drives[c.I(0)] = Drive{c.I(1), c.F(2), c.F(3), c.F(5)};
        return true;
    }
    case 0x0173: {   // SET_CHAR_HEADING(char, degrees)
        const float h = c.F(1) * 3.14159265f / 180.0f;
        if (c.I(0) == kPlayer) { if (setPlayerHeading) setPlayerHeading(h); }
        else { const int i = CharIndex(c.I(0)); if (i >= 0) peds->SetHeading((size_t)i, h); }
        return true;
    }
    case 0x00A1: {   // SET_CHAR_COORDINATES(char, x, y, z)
        if (c.I(0) == kPlayer) { if (scm && scm->host.setPlayerPos) scm->host.setPlayerPos(c.F(1), c.F(2), c.F(3)); return true; }
        const int i = CharIndex(c.I(0));
        if (i >= 0) peds->SetPos((size_t)i, c.F(1), c.F(2), c.F(3));
        return true;
    }
    case 0x0223: case 0x048F: case 0x01C2: case 0x0330: return true;   // SET_CHAR_HEALTH, REMOVE_ALL_CHAR_WEAPONS, MARK_CHAR_AS_NO_LONGER_NEEDED, SET_PLAYER_NEVER_GETS_TIRED
    case 0x00FE: {   // LOCATE_CHAR_ANY_MEANS_3D(char, x, y, z, rx, ry, rz, drawSphere)
        float x, y, z;
        c.result = CharPos(c.I(0), x, y, z) && std::fabs(x - c.F(1)) <= c.F(4) && std::fabs(y - c.F(2)) <= c.F(5) && std::fabs(z - c.F(3)) <= c.F(6);
        return true;
    }
    // ---- cars ---------------------------------------------------------------------------
    case 0x00A5: {   // CREATE_CAR(model, x, y, z) -> car
        if (!cars || !r) { c.Out(4, 0); return true; }
        float z = c.F(3);
        if (groundAt) { const float g = groundAt(c.F(1), c.F(2)); if (g > -1e5f && z < g + 0.3f) z = g + 0.9f; }
        const int i = cars->Spawn(*r, c.I(0), c.F(1), c.F(2), z, 0.0f);
        c.Out(4, i >= 0 ? i + 1 : 0);
        return true;
    }
    case 0x00A6: {   // DELETE_CAR(car)
        if (VehicleInstance* v = car(c.I(0))) v->removed = true;
        for (auto it = m_charCar.begin(); it != m_charCar.end();) {
            if (it->second == c.I(0)) { const int i = CharIndex(it->first); if (i >= 0) peds->Remove((size_t)i); it = m_charCar.erase(it); } else ++it;
        }
        return true;
    }
    case 0x0119: { VehicleInstance* v = car(c.I(0)); c.result = !v || v->removed || v->wrecked || v->health <= 0.0f; return true; }   // IS_CAR_DEAD
    case 0x0175: if (VehicleInstance* v = carAlive(c.I(0))) v->heading = c.F(1) * 3.14159265f / 180.0f; return true;   // SET_CAR_HEADING
    case 0x01C3: case 0x099A: case 0x067F: return true;   // MARK_CAR_AS_NO_LONGER_NEEDED, SET_CAR_COLLISION, FORCE_CAR_LIGHTS
    case 0x060E: c.result = false; return true;           // IS_PLAYBACK_GOING_ON_FOR_CAR
    case 0x05EB: return true;                              // START_PLAYBACK_RECORDED_CAR
    case 0x0186: {   // ADD_BLIP_FOR_CAR(car) -> blip
        VehicleInstance* v = carAlive(c.I(0));
        c.Out(1, v && scm && scm->host.addBlip ? scm->host.addBlip(v->x, v->y, v->z, 0, false) : 0);
        return true;
    }
    case 0x07E0: return true;   // SET_BLIP_AS_FRIENDLY
    // ---- world switches and stats that have no game effect yet ------------------------
    case 0x01E8: case 0x022B: case 0x091E: case 0x06D7: case 0x06D8: case 0x06D9: case 0x06DC: case 0x06DD:   // roads and trains
    case 0x02B9: case 0x076C:                                                                                 // garages, gang strength
    case 0x0955: case 0x0952: case 0x0954: case 0x040D: case 0x01EB: case 0x03DE: case 0x09BA: // beat track, mission audio, densities, zone names, stats
    case 0x0109: case 0x01B7: case 0x01B6: case 0x0662: case 0x0459: case 0x0A2D:
        return true;
    case 0x0953: c.Out(0, 2); return true;   // GET_BEAT_TRACK_STATUS: 2 = ready to play
    case 0x0112: c.result = false; return true;  // HAS_DEATHARREST_BEEN_EXECUTED
    case 0x09B2: c.Out(1, 496); c.Out(2, 1); c.result = true; return true;   // GET_RANDOM_CAR_MODEL_IN_MEMORY: a common sedan is always resident
    case 0x04AE: case 0x04AF: c.Out(0, c.I(1)); return true;                              // SET_LVAR_INT_TO_CONSTANT
    case 0x03D0: c.result = true; return true;                               // HAS_MISSION_AUDIO_LOADED
    case 0x00D8: c.t.finished = true; return true;                           // MISSION_HAS_FINISHED: the mission thread ends
    case 0x00C0: if (scm && scm->host.setTime) scm->host.setTime(c.I(0), c.I(1)); return true;   // SET_TIME_OF_DAY
    default: break;
    }
    return HandleMore(c);
}

bool ScriptGame::EntityPos(int kind, int h, float& x, float& y, float& z, bool& inCar, float* speed) {
    inCar = false;
    if (speed) *speed = 0.0f;
    if (kind == 0) {
        if (h == kPlayer) inCar = playerInCar && playerInCar();
        else inCar = m_charCar.count(h) != 0;
        return CharPos(h, x, y, z);
    }
    if (kind == 1) {
        if (!cars || h < 1 || (size_t)h > cars->Count()) return false;
        const VehicleInstance& v = cars->Car((size_t)h - 1);
        if (v.removed) return false;
        x = v.x; y = v.y; z = v.z;
        if (speed) *speed = std::sqrt(v.vx * v.vx + v.vy * v.vy);
        return true;
    }
    if (h < 1 || (size_t)h > m_objs.size() || m_objs[(size_t)h - 1].removed) return false;
    const Obj& o = m_objs[(size_t)h - 1];
    x = o.x; y = o.y; z = o.z;
    return true;
}

// LOCATE_[STOPPED_]<subject>_<mode>_[<target>_]<2D|3D>: the subject is within the box (or sphere radii) around a point or another entity.
bool ScriptGame::Locate(ScmMachine::Call& c, const std::string& n) {
    size_t p = 7;   // after "LOCATE_"
    bool stopped = false;
    if (n.compare(p, 8, "STOPPED_") == 0) { stopped = true; p += 8; }
    auto eat = [&](const char* w) { const size_t l = std::strlen(w); if (n.compare(p, l, w) == 0 && (p + l == n.size() || n[p + l] == '_')) { p += l + (p + l < n.size() ? 1 : 0); return true; } return false; };
    int subjKind = -1;
    bool subjPlayer = false;
    if (eat("PLAYER")) { subjKind = 0; subjPlayer = true; }
    else if (eat("CHAR")) subjKind = 0;
    else if (eat("CAR")) subjKind = 1;
    else return false;   // COLL / OBJECT / SNIPER: not handled
    int mode = 0;   // 0 any means, 1 on foot, 2 in car
    if (eat("ANY_MEANS")) mode = 0; else if (eat("ON_FOOT")) mode = 1; else if (eat("IN_CAR")) mode = 2;
    int targKind = -1;
    if (eat("CHAR")) targKind = 0; else if (eat("CAR")) targKind = 1; else if (eat("OBJECT")) targKind = 2;
    const bool is3d = n.compare(n.size() - 2, 2, "3D") == 0;
    size_t a = 0;
    const int subj = subjPlayer ? kPlayer : c.I(a);
    a++;
    float sx, sy, sz, spd; bool inCar;
    if (!EntityPos(subjKind, subj, sx, sy, sz, inCar, &spd)) { c.result = false; return true; }
    if (subjKind == 0 && subj == kPlayer) {
        if (playerInCar && playerInCar() && playerCar && cars) {
            const int ci = playerCar();
            if (ci >= 0 && (size_t)ci < cars->Count()) { const VehicleInstance& v = cars->Car((size_t)ci); sx = v.x; sy = v.y; sz = v.z; spd = std::sqrt(v.vx * v.vx + v.vy * v.vy); }
        }
    }
    float tx, ty, tz = 0;
    if (targKind >= 0) { bool ic; if (!EntityPos(targKind, c.I(a), tx, ty, tz, ic)) { c.result = false; return true; } a++; }
    else { tx = c.F(a++); ty = c.F(a++); if (is3d) tz = c.F(a++); }
    const float rx = c.F(a++), ry = c.F(a++), rz = is3d ? c.F(a++) : 0.0f;
    bool ok = std::fabs(sx - tx) <= rx && std::fabs(sy - ty) <= ry && (!is3d || std::fabs(sz - tz) <= rz);
    if (mode == 1 && inCar) ok = false;
    if (mode == 2 && !inCar && subjKind == 0) ok = false;
    if (stopped && spd > 0.5f) ok = false;
    c.result = ok;
    return true;
}

// IS_<subject>_IN_AREA_[ON_FOOT_|IN_CAR_]2D/3D(subject, x1, y1, [z1,] x2, y2, [z2,] draw)
bool ScriptGame::InArea(ScmMachine::Call& c, const std::string& n) {
    const int subjKind = n.compare(3, 6, "PLAYER") == 0 || n.compare(3, 4, "CHAR") == 0 ? 0 : (n.compare(3, 3, "CAR") == 0 ? 1 : -1);
    if (subjKind < 0) return false;
    const bool player = n.compare(3, 6, "PLAYER") == 0;
    const bool is3d = n.compare(n.size() - 2, 2, "3D") == 0;
    const bool onFoot = n.find("ON_FOOT") != std::string::npos, inCarOnly = n.find("IN_CAR") != std::string::npos;
    float x, y, z; bool inCar;
    if (!EntityPos(subjKind, player ? kPlayer : c.I(0), x, y, z, inCar)) { c.result = false; return true; }
    if (player && inCar && playerCar && cars) { const int ci = playerCar(); if (ci >= 0 && (size_t)ci < cars->Count()) { x = cars->Car((size_t)ci).x; y = cars->Car((size_t)ci).y; z = cars->Car((size_t)ci).z; } }
    size_t a = 1;
    const float x1 = c.F(a++), y1 = c.F(a++), z1 = is3d ? c.F(a++) : 0.0f;
    const float x2 = c.F(a++), y2 = c.F(a++), z2 = is3d ? c.F(a++) : 0.0f;
    bool ok = x >= std::min(x1, x2) && x <= std::max(x1, x2) && y >= std::min(y1, y2) && y <= std::max(y1, y2) &&
              (!is3d || (z >= std::min(z1, z2) && z <= std::max(z1, z2)));
    if (onFoot && inCar) ok = false;
    if (inCarOnly && !inCar && subjKind == 0) ok = false;
    c.result = ok;
    return true;
}

bool ScriptGame::HandleMore(ScmMachine::Call& c) {
    const std::string name = ScmCommandName(c.op);
    if (name.compare(0, 7, "LOCATE_") == 0 && Locate(c, name)) return true;
    if (name.size() > 12 && name.compare(0, 3, "IS_") == 0 && name.find("_IN_AREA_") != std::string::npos && InArea(c, name)) return true;
    switch (c.op) {
    // ---- language: comparisons with constants, bit tests, bitwise operators --------------
    case 0x04A3: case 0x04A4: c.result = c.I(0) == c.I(1); return true;                      // IS_INT_(L)VAR_EQUAL_TO_CONSTANT
    case 0x04B0: case 0x04B1: c.result = c.I(0) > c.I(1); return true;                      // IS_INT_(L)VAR_GREATER_THAN_CONSTANT
    case 0x04B2: case 0x04B3: c.result = c.I(0) > c.I(1); return true;                      // IS_CONSTANT_GREATER_THAN_INT_(L)VAR(const, var)
    case 0x04B4: case 0x04B5: c.result = c.I(0) >= c.I(1); return true;                     // ..._GREATER_OR_EQUAL_TO_CONSTANT
    case 0x04B6: case 0x04B7: c.result = c.I(0) >= c.I(1); return true;                     // IS_CONSTANT_GREATER_OR_EQUAL_TO_INT_(L)VAR(const, var)
    case 0x08B4: case 0x08B5: case 0x08B6: case 0x08B7: case 0x08B8: case 0x08B9:            // IS_*_VAR_BIT_SET_*(var, bit)
        c.result = ((uint32_t)c.I(0) >> (c.I(1) & 31)) & 1u; return true;
    case 0x08BA: case 0x08BB: case 0x08BC: case 0x08BD: case 0x08BE: case 0x08BF:            // SET_*_VAR_BIT_*(var, bit)
        c.Out(0, (int32_t)((uint32_t)c.I(0) | (1u << (c.I(1) & 31)))); return true;
    case 0x08C0: case 0x08C1: case 0x08C2: case 0x08C3: case 0x08C4: case 0x08C5:            // CLEAR_*_VAR_BIT_*(var, bit)
        c.Out(0, (int32_t)((uint32_t)c.I(0) & ~(1u << (c.I(1) & 31)))); return true;
    case 0x0B17: c.Out(0, c.I(0) & c.I(1)); return true;                                    // BITWISE_AND_THING_WITH_THING
    case 0x0B18: c.Out(0, c.I(0) | c.I(1)); return true;
    case 0x0B19: c.Out(0, c.I(0) ^ c.I(1)); return true;
    case 0x0B1A: c.Out(0, ~c.I(0)); return true;
    case 0x0B1B: c.Out(0, c.I(1) ? c.I(0) % c.I(1) : 0); return true;
    case 0x0B1C: c.Out(0, (int32_t)((uint32_t)c.I(0) >> (c.I(1) & 31))); return true;
    case 0x0B1D: c.Out(0, (int32_t)((uint32_t)c.I(0) << (c.I(1) & 31))); return true;
    case 0x06D1: c.OutStr(0, c.S(1, 16), 16); return true;                                    // SET_VAR_TEXT_LABEL16
    // ---- stats ----------------------------------------------------------------------------
    case 0x0629: Stats::Set(c.I(0), (float)c.I(1)); return true;                             // SET_INT_STAT
    case 0x062A: Stats::Set(c.I(0), c.F(1)); return true;                                    // SET_FLOAT_STAT
    case 0x0652: c.Out(1, (int32_t)Stats::Get(c.I(0))); return true;                         // GET_INT_STAT
    case 0x08F8: return true;                                                                  // SHOW_UPDATE_STATS
    case 0x058C: c.Out(0, 0); return true;                                                    // GET_PROGRESS_PERCENTAGE
    case 0x08E1: c.Out(0, 0); return true;                                                    // FIND_NUMBER_TAGS_TAGGED
    // ---- queries ------------------------------------------------------------------------
    case 0x0118: {   // IS_CHAR_DEAD(char)
        if (c.I(0) == kPlayer) { c.result = false; return true; }
        const int i = CharIndex(c.I(0));
        c.result = i < 0 || !peds->Alive((size_t)i);
        return true;
    }
    case 0x0445: case 0x09AE: case 0x04C8: case 0x04A7: case 0x04A9: case 0x04AB: case 0x0965: case 0x09BE: case 0x04AD: case 0x0A0C: c.result = false; return true;   // cheats, train, flying, boat, heli, plane, swimming, minigame, water, jetpack
    case 0x00DD: {   // IS_CHAR_IN_MODEL(char, model)
        float x, y, z; bool ic;
        if (c.I(0) == kPlayer && playerCar && cars) { const int ci = playerCar(); c.result = ci >= 0 && (size_t)ci < cars->Count() && cars->Car((size_t)ci).model == c.I(1); }
        else { const auto it = m_charCar.find(c.I(0)); const VehicleInstance* v = it != m_charCar.end() && cars && it->second >= 1 && (size_t)it->second <= cars->Count() ? &cars->Car((size_t)it->second - 1) : nullptr; c.result = v && v->model == c.I(1); }
        (void)x; (void)y; (void)z; (void)ic;
        return true;
    }
    case 0x09E8: c.Out(1, (c.I(0) == kPlayer && getArea) ? getArea() : 0); return true;   // GET_CHAR_AREA_VISIBLE
    case 0x077E: c.Out(0, getArea ? getArea() : 0); return true;                           // GET_AREA_VISIBLE
    case 0x04BB: if (setArea) setArea(c.I(0)); return true;                                 // SET_AREA_VISIBLE
    case 0x0860: if (c.I(0) == kPlayer && setArea) setArea(c.I(1)); return true;            // SET_CHAR_AREA_VISIBLE
    case 0x07FB: if (enex) enex->SetEnabledByName(c.S(0).c_str(), c.I(1) != 0); return true;   // SWITCH_ENTRY_EXIT
    case 0x08E7: if (enex) enex->SetAllDisabled(c.I(0) != 0); return true;                  // DISABLE_ALL_ENTRY_EXITS
    case 0x094B: c.OutStr(1, enex ? enex->LastName() : ""); return true;                    // GET_NAME_OF_ENTRY_EXIT_CHAR_USED
    case 0x08AD: return true;                                                                // SET_CHAR_HAS_USED_ENTRY_EXIT
    case 0x03EE: c.result = true; return true;  // CAN_PLAYER_START_MISSION
    case 0x07D0: c.Out(0, 3); return true;      // GET_CURRENT_DAY_OF_WEEK
    case 0x09FB: c.Out(0, 0); return true;      // GET_CURRENT_LANGUAGE: American
    case 0x0842: c.Out(1, 1); return true;      // GET_CITY_PLAYER_IS_IN: Los Santos
    case 0x03B0: c.result = false; return true; // IS_GARAGE_OPEN
    case 0x0491: c.result = false; return true; // HAS_CHAR_GOT_WEAPON
    case 0x075C: c.result = c.I(0) != 0; return true;   // DOES_BLIP_EXIST
    case 0x00A3: case 0x00A4: return InArea(c, c.op == 0x00A3 ? "IS_CHAR_IN_AREA_2D" : "IS_CHAR_IN_AREA_3D");
    // ---- mission audio: no voice playback yet; loading and playing finish at once ---------
    case 0x03CF: case 0x03D1: case 0x0A09: case 0x03E7: return true;   // LOAD_MISSION_AUDIO, PLAY_MISSION_AUDIO, SHUT_CHAR_UP_FOR_SCRIPTED_SPEECH, FLASH_HUD_OBJECT
    case 0x03D2: c.result = true; return true;                          // HAS_MISSION_AUDIO_FINISHED
    // ---- plain switches with no game effect yet -------------------------------------------
    case 0x042C: case 0x030D: case 0x0997: case 0x01F0: case 0x0111: case 0x06CF: case 0x0746: case 0x0180:   // totals, wanted, death state, timer bars, relationship, mission flag
    case 0x014C: case 0x0914: case 0x0929: case 0x07D3: case 0x0884: case 0x0928: case 0x08E8: case 0x0363: case 0x03B7: case 0x090F:
    case 0x06C8: case 0x09B4: case 0x0814: case 0x08CA: case 0x0767: case 0x0874: case 0x076A: case 0x09B7: case 0x04F8: case 0x0827:
    case 0x016C: case 0x016D: case 0x02FA: case 0x0A17:
        return true;
    case 0x07AF: c.Out(1, 0); return true;   // GET_PLAYER_GROUP
    case 0x010F: c.result = wantedLevel && wantedLevel() > c.I(1); return true;                // IS_WANTED_LEVEL_GREATER(player, level)
    case 0x010D: case 0x010E:                                                                  // ALTER_WANTED_LEVEL(_NO_DROP)(player, level)
        if (setWantedLevel && (c.op == 0x010D || !wantedLevel || wantedLevel() < c.I(1))) setWantedLevel(std::clamp(c.I(1), 0, 6));
        return true;
    case 0x0110: if (setWantedLevel) setWantedLevel(0); return true;                           // CLEAR_WANTED_LEVEL
    case 0x0A03: c.result = false; return true;                                                // IS_GANG_WAR_FIGHTING_GOING_ON
    case 0x0623: Stats::Set(c.I(0), Stats::Get(c.I(0)) + (float)c.I(1)); return true;       // INCREMENT_INT_STAT
    case 0x0625: Stats::Set(c.I(0), Stats::Get(c.I(0)) - (float)c.I(1)); return true;       // DECREMENT_INT_STAT
    case 0x014B: c.Out(12, 0); return true;                                                   // CREATE_CAR_GENERATOR: parked car spawns are not run
    case 0x09E2: c.Out(13, 0); return true;                                                   // CREATE_CAR_GENERATOR_WITH_PLATE
    case 0x0926: c.Out(1, 0); return true;                                                    // GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT
    case 0x08AB: c.result = true; return true;                                                // HAS_STREAMED_SCRIPT_LOADED
    case 0x0A40: c.Out(4, ++m_markers); return true;                                          // CREATE_USER_3D_MARKER
    case 0x044B: c.result = !(c.I(0) == kPlayer ? (playerInCar && playerInCar()) : m_charCar.count(c.I(0)) != 0); return true;   // IS_CHAR_ON_FOOT
    case 0x0400: {   // GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS(obj, ox, oy, oz) -> x, y, z
        float x = 0, y = 0, z = 0, h = 0;
        if (c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size()) { const Obj& o = m_objs[(size_t)c.I(0) - 1]; x = o.x; y = o.y; z = o.z; h = o.heading; }
        const float r = h * 0.017453292f, cs = std::cos(r), sn = std::sin(r);
        c.OutF(4, x + c.F(1) * cs - c.F(2) * sn); c.OutF(5, y + c.F(1) * sn + c.F(2) * cs); c.OutF(6, z + c.F(3));
        return true;
    }
    case 0x093A: case 0x0361: case 0x0750: case 0x0382: case 0x08E0: case 0x08DF: case 0x08DE: case 0x08DD:
        return true;   // SET_GARAGE_RESPRAY_FREE, CLOSE_GARAGE, SET_OBJECT_VISIBLE / COLLISION, restart points, penalties
    // ---- pickups -----------------------------------------------------------------------------
    case 0x0213: c.Out(5, AddPickup(c.I(0), c.I(1), -1, c.F(2), c.F(3), c.F(4))); return true;                 // CREATE_PICKUP(model, type, x, y, z)
    case 0x032B: c.Out(6, AddPickup(c.I(0), c.I(1), c.I(2), c.F(3), c.F(4), c.F(5))); return true;             // CREATE_PICKUP_WITH_AMMO(model, type, ammo, x, y, z)
    case 0x0958: case 0x0959: case 0x095A: c.Out(3, AddPickup(-1, 0, 0, c.F(0), c.F(1), c.F(2))); return true;   // CREATE_SNAPSHOT / HORSESHOE / OYSTER pickup
    case 0x0517: { c.Out(4, AddPickup(-2, 0, 0, c.F(0), c.F(1), c.F(2))); return true; }                       // CREATE_LOCKED_PROPERTY_PICKUP(x, y, z, text)
    case 0x0518: c.Out(5, AddPickup(-1, 0, 0, c.F(0), c.F(1), c.F(2))); return true;                           // CREATE_FORSALE_PROPERTY_PICKUP(x, y, z, price, text)
    case 0x0214: {   // HAS_PICKUP_BEEN_COLLECTED
        const int h = c.I(0);
        c.result = h >= 1 && (size_t)h <= m_pickups.size() && m_pickups[(size_t)h - 1].collected;
        if (std::getenv("GTA_SCMGAME")) std::fprintf(stderr, "[SCMGAME] collected? pickup %d -> %d\n", h, (int)c.result);
        return true;
    }
    case 0x0215: {   // REMOVE_PICKUP
        const int h = c.I(0);
        if (h >= 1 && (size_t)h <= m_pickups.size()) {
            ScriptPickup& p = m_pickups[(size_t)h - 1];
            if (p.slot >= 0 && pickups) pickups->Remove(p.slot);
            p.removed = true; p.slot = -1;
        }
        return true;
    }
    // ---- objects: tracked, not drawn -------------------------------------------------------
    case 0x0107: case 0x029B: {   // CREATE_OBJECT / CREATE_OBJECT_NO_OFFSET(model, x, y, z) -> object
        Obj o; o.model = c.I(0); o.x = c.F(1); o.y = c.F(2); o.z = c.F(3);
        m_objs.push_back(o);
        c.Out(4, (int32_t)m_objs.size());
        return true;
    }
    case 0x03CA: c.result = c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size() && !m_objs[(size_t)c.I(0) - 1].removed; return true;   // DOES_OBJECT_EXIST
    case 0x0177: case 0x0453: case 0x034D: case 0x0392: case 0x0550: case 0x07F7: case 0x09CA: case 0x01C7: case 0x0566:
        if (c.op == 0x0177 && c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size()) m_objs[(size_t)c.I(0) - 1].heading = c.F(1);
        return true;   // SET_OBJECT_HEADING and other per-object settings
    case 0x0176: c.OutF(1, c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size() ? m_objs[(size_t)c.I(0) - 1].heading : 0.0f); return true;   // GET_OBJECT_HEADING
    case 0x01BB: {   // GET_OBJECT_COORDINATES(obj) -> x, y, z
        if (c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size()) { const Obj& o = m_objs[(size_t)c.I(0) - 1]; c.OutF(1, o.x); c.OutF(2, o.y); c.OutF(3, o.z); }
        return true;
    }
    default: break;
    }
    return false;
}

// The weapon type of a pickup model (CPickups::GenerateNewOne is given the model, the weapon is its
// CWeaponInfo::m_nModelId owner); -1 when the model is no weapon.
static int WeaponOfModel(int m) {
    static const int kTab[][2] = {{331, 1}, {333, 2}, {334, 3}, {335, 4}, {336, 5}, {337, 6}, {338, 7}, {339, 8}, {341, 9}, {321, 10},
        {322, 11}, {323, 12}, {324, 13}, {325, 14}, {326, 15}, {342, 16}, {343, 17}, {344, 18}, {346, 22}, {347, 23}, {348, 24},
        {349, 25}, {350, 26}, {351, 27}, {352, 28}, {353, 29}, {355, 30}, {356, 31}, {372, 32}, {357, 33}, {358, 34}, {359, 35},
        {360, 36}, {361, 37}, {362, 38}, {363, 39}, {364, 40}, {365, 41}, {366, 42}, {367, 43}, {368, 44}, {369, 45}, {371, 46}};
    for (const auto& e : kTab) if (e[0] == m) return e[1];
    return -1;
}

int ScriptGame::AddPickup(int model, int type, int ammo, float x, float y, float z) {
    ScriptPickup p; p.x = x; p.y = y; p.z = z;
    p.locked = model == -2;
    if (model > 0 && pickups && nowMs) {
        int kind = -1, weapon = 0;
        if (int w = WeaponOfModel(model); w >= 0) { kind = Pickups::Weapon; weapon = w; }
        else if (model == 1240) kind = Pickups::Health;
        else if (model == 1242) kind = Pickups::Armour;
        else if (model == 1241) kind = Pickups::Adrenaline;
        else if (model == 1247) kind = Pickups::Bribe;
        else if (model == 1239) kind = Pickups::Info;
        else if (model == 1212 || model == 1274) kind = Pickups::Cash;
        else if (model == 370) kind = Pickups::Jetpack;
        if (kind >= 0) {
            const float pos[3] = {x, y, z};
            p.slot = pickups->Add(pos, kind, weapon, type, ammo < 0 ? 0 : ammo, false, nowMs());
        }
    }
    m_pickups.push_back(p);
    if (std::getenv("GTA_SCMGAME")) std::fprintf(stderr, "[SCMGAME] pickup %zu model %d type %d at %.1f %.1f %.1f slot %d\n", m_pickups.size(), model, type, x, y, z, p.slot);
    return (int)m_pickups.size();
}

void ScriptGame::Update(float dt) {
    for (ScriptPickup& p : m_pickups) {   // collected: the slot freed itself, or the player stands on a tracked one
        if (p.removed || p.collected) continue;
        if (p.slot >= 0) { if (pickups && !pickups->At(p.slot).used) p.collected = true; continue; }
        if (p.locked) continue;
        float px, py, pz;
        if (CharPos(kPlayer, px, py, pz) && std::fabs(pz - p.z) < 2.0f && (px - p.x) * (px - p.x) + (py - p.y) * (py - p.y) < 1.8f) p.collected = true;
    }
    auto car = [&](int h) -> VehicleInstance* { return cars && h >= 1 && (size_t)h <= cars->Count() ? &cars->Car((size_t)h - 1) : nullptr; };
    auto carAlive = [&](int h) -> VehicleInstance* { VehicleInstance* v = car(h); return v && !v->removed ? v : nullptr; };
    if (m_fading) {
        m_fadeT += dt;
        const float k = m_fadeDur > 0 ? std::min(1.0f, m_fadeT / m_fadeDur) : 1.0f;
        fade = m_fadeFrom + (m_fadeTo - m_fadeFrom) * k;
        if (k >= 1.0f) m_fading = false;
    }
    const float step = dt / 0.5f;
    wide = wideTarget ? std::min(1.0f, wide + step) : std::max(0.0f, wide - step);
    if (m_subTimer > 0) { m_subTimer -= dt; if (m_subTimer <= 0) m_subtitle.clear(); }
    if (cam.moveOn) {
        cam.moveT += dt;
        const float k = std::min(1.0f, cam.moveT / cam.moveDur);
        for (int i = 0; i < 3; i++) cam.pos[i] = cam.moveFrom[i] + (cam.moveTo[i] - cam.moveFrom[i]) * k;
        if (k >= 1.0f) cam.moveOn = false;
    }
    if (cam.trackOn) {
        cam.trackT += dt;
        const float k = std::min(1.0f, cam.trackT / cam.trackDur);
        for (int i = 0; i < 3; i++) cam.look[i] = cam.trackFrom[i] + (cam.trackTo[i] - cam.trackFrom[i]) * k;
        if (k >= 1.0f) cam.trackOn = false;
    }
    if (cam.shakeT > 0) cam.shakeT = std::max(0.0f, cam.shakeT - dt);
    if (m_cut.state == 2) {
        m_cut.t += dt;
        if (m_cut.t >= m_cut.dur) m_cut.state = 3;
    }
    // characters in cars follow them
    for (auto& [uid, carH] : m_charCar) {
        VehicleInstance* v = car(carH);
        const int i = CharIndex(uid);
        if (v && i >= 0) peds->SetPos((size_t)i, v->x, v->y, v->z);
    }
    // TASK_CAR_DRIVE_TO_COORD: a simple steering to the point
    for (auto it = m_drives.begin(); it != m_drives.end();) {
        VehicleInstance* v = carAlive(it->second.car);
        if (!v) { it = m_drives.erase(it); continue; }
        const float dx = it->second.x - v->x, dy = it->second.y - v->y;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < 3.0f) { v->speed = 0; it = m_drives.erase(it); continue; }
        v->heading = std::atan2(dy, dx);
        v->speed = std::min(it->second.speed, 14.0f);
        v->x += std::cos(v->heading) * v->speed * dt;
        v->y += std::sin(v->heading) * v->speed * dt;
        ++it;
    }
}

ScriptOverlay ScriptGame::Overlay() {
    ScriptOverlay o;
    o.fade = std::max(fade, enex ? enex->Fade() : 0.0f);
    o.fadeRgb = ((uint32_t)m_fadeRgb[0] << 16) | ((uint32_t)m_fadeRgb[1] << 8) | (uint32_t)m_fadeRgb[2];
    o.wide = wide;
    o.subtitle = m_subtitle;
    o.text = m_queue;
    m_queue.clear();
    return o;
}
