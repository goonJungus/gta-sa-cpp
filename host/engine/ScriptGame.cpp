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
    case 0x03E3: m_spritesBefore = c.I(0) != 0; return true;                                                          // SET_SPRITES_DRAW_BEFORE_FADE
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
    case 0x07C0: RecordingLoaded(c.I(0)); return true;   // REQUEST_CAR_RECORDING
    case 0x0247: case 0x0249: case 0x04E4: case 0x03CB: case 0x0A0B: case 0x0776: case 0x0395: return true;   // REQUEST_MODEL, MARK_..., REQUEST_CAR_RECORDING, REQUEST_COLLISION, LOAD_SCENE, LOAD_SCENE_IN_DIRECTION, REQUEST_IPL, CLEAR_AREA
    case 0x07C1: c.result = RecordingLoaded(c.I(0)); return true;   // HAS_CAR_RECORDING_BEEN_LOADED
    case 0x0248: case 0x023D: c.result = true; return true;   // HAS_MODEL_LOADED, HAS_SPECIAL_CHARACTER_LOADED
    case 0x023C: {   // LOAD_SPECIAL_CHARACTER(slot, name)
        const size_t slot = (size_t)std::max(1, c.I(0));
        if (m_special.size() < slot) m_special.resize(slot);
        m_special[slot - 1] = c.S(1);
        if (peds && slot >= 1 && slot <= 10) peds->SetSpecial((int)slot, m_special[slot - 1]);
        return true;
    }
    case 0x0296: return true;   // UNLOAD_SPECIAL_CHARACTER
    // ---- cutscenes: loading and timing only (the player is Phase B) -----------------------
    case 0x02E4: {   // LOAD_CUTSCENE(name): NAME.cut / .dat / .ifp
        if (r) m_cutscene.Free(*r);
        m_cut = CutState{};
        m_cut.name = c.S(0);
        m_cut.state = 1;
        if (!r || !m_cutscene.Load(*r, gameDir, m_cut.name)) std::fprintf(stderr, "[SCMGAME] cutscene %s did not load\n", m_cut.name.c_str());
        return true;
    }
    case 0x06B9: c.result = m_cut.state >= 1; return true;                                  // HAS_CUTSCENE_LOADED
    case 0x0244: m_cutscene.SetOffset(c.F(0), c.F(1), c.F(2)); return true;                 // SET_CUTSCENE_OFFSET
    case 0x02E5: c.Out(1, ++m_cutObjects); return true;                                     // CREATE_CUTSCENE_OBJECT: the .cut lists the objects
    case 0x02E6: case 0x04BC: case 0x02F5: return true;                                     // SET_CUTSCENE_ANIM / _TO_LOOP / HEAD_ANIM
    case 0x02F4: c.Out(1, ++m_cutObjects); return true;                                     // CREATE_CUTSCENE_HEAD
    case 0x02E7: {                                                                           // START_CUTSCENE
        m_cut.state = 2; m_cut.t = 0;
        m_cut.dur = m_cutscene.Loaded() ? m_cutscene.Duration() : 6.0f;
        m_cutscene.Start(std::getenv("GTA_CUTSEEK") ? (float)std::atof(std::getenv("GTA_CUTSEEK")) : 0.0f);   // dev: GTA_CUTSEEK=ms
        wideTarget = true;   // CCutsceneMgr::StartCutscene turns the widescreen on
        const float* o = m_cutscene.Offset();
        if (m_cutscene.Loaded() && scm && scm->host.setPlayerPos) scm->host.setPlayerPos(o[0], o[1], o[2]);   // the world streams in at the scene
        if (showPlayer) showPlayer(false);
        if (cutsceneAudio && _stricmp(m_cut.name.c_str(), "finale") != 0) cutsceneAudio(m_cut.name);   // retail skips the audio of "finale"
        std::fprintf(stderr, "[SCMGAME] cutscene %s starts: %.2f s\n", m_cut.name.c_str(), m_cut.dur);
        return true;
    }
    case 0x02E8: c.Out(0, (int)m_cutscene.TimeMs()); return true;                           // GET_CUTSCENE_TIME
    case 0x02E9: c.result = m_cut.state == 3; return true;                                  // HAS_CUTSCENE_FINISHED
    case 0x02EA: {                                                                           // CLEAR_CUTSCENE
        if (cutsceneAudioStop) cutsceneAudioStop();
        if (r) m_cutscene.Free(*r);
        m_cut = CutState{};
        if (cam.cutscene) cam = ScriptCamera{};
        wideTarget = false;
        if (showPlayer) showPlayer(true);
        return true;
    }
    case 0x056A: c.result = m_cut.skipped; std::fprintf(stderr, "[SCMGAME] WAS_CUTSCENE_SKIPPED -> %d\n", (int)m_cut.skipped); return true;                                     // WAS_CUTSCENE_SKIPPED
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
        else if (c.I(0) == kPlayer) { playerGoal.on = true; playerGoal.x = c.F(1); playerGoal.y = c.F(2); playerGoal.mode = c.I(4); }
        return true;
    }
    case 0x0792: case 0x0687: {   // CLEAR_CHAR_TASKS(_IMMEDIATELY)(char)
        const int i = CharIndex(c.I(0));
        if (i >= 0) { peds->ClearGoal((size_t)i); }
        if (c.I(0) == kPlayer) playerGoal.on = false;
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
        if (c.I(0) == kPlayer) {
            if (playerInCar && playerInCar() && playerCar && cars && playerCar() >= 0) {   // a char in a car takes the car along
                if (VehicleInstance* v = car(playerCar() + 1)) { v->x = c.F(1); v->y = c.F(2); v->z = c.F(3); v->speed = 0; v->vx = v->vy = 0; }
            }
            if (scm && scm->host.setPlayerPos) scm->host.setPlayerPos(c.F(1), c.F(2), c.F(3));
            return true;
        }
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
    case 0x060E: c.result = IsPlaybackGoingOn(c.I(0)); return true;           // IS_PLAYBACK_GOING_ON_FOR_CAR
    case 0x05EB: StartPlayback(c.I(0), c.I(1), false, false); return true;   // START_PLAYBACK_RECORDED_CAR
    case 0x085E: StartPlayback(c.I(0), c.I(1), true, false); return true;    // START_PLAYBACK_RECORDED_CAR_LOOPED
    case 0x0705: StartPlayback(c.I(0), c.I(1), false, true); return true;    // START_PLAYBACK_RECORDED_CAR_USING_AI
    case 0x085F: StartPlayback(c.I(0), c.I(1), true, true); return true;     // START_PLAYBACK_RECORDED_CAR_USING_AI_LOOPED
    case 0x05EC: StopPlayback(c.I(0), false); return true;                   // STOP_PLAYBACK_RECORDED_CAR
    case 0x06C5: StopPlayback(c.I(0), true); return true;                    // SKIP_TO_END_AND_STOP_PLAYBACK_RECORDED_CAR
    case 0x05ED: case 0x05EE: { const int s = PlaybackSlot(c.I(0)); if (s >= 0) m_play[s].paused = c.op == 0x05ED; return true; }   // PAUSE / UNPAUSE_PLAYBACK_RECORDED_CAR
    case 0x090A: { const int s = PlaybackSlot(c.I(0)); c.result = s >= 0 && m_play[s].paused; return true; }   // IS_PLAYBACK_FOR_CAR_PAUSED
    case 0x06FD: { const int s = PlaybackSlot(c.I(0)); if (s >= 0) m_play[s].speed = c.F(1); return true; }   // SET_PLAYBACK_SPEED
    case 0x099B: { const int s = PlaybackSlot(c.I(0)); if (s >= 0) m_play[s].ai = true; return true; }         // CHANGE_PLAYBACK_TO_USE_AI
    case 0x0873: return true;                                                // REMOVE_CAR_RECORDING
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
    case 0x01B7: case 0x01B6: case 0x0662: case 0x0459: case 0x0A2D:
        return true;
    case 0x0953: c.Out(0, 2); return true;   // GET_BEAT_TRACK_STATUS: 2 = ready to play
    case 0x0112: c.result = false; return true;  // HAS_DEATHARREST_BEEN_EXECUTED
    case 0x09B2: c.Out(1, 496); c.Out(2, 1); c.result = true; return true;   // GET_RANDOM_CAR_MODEL_IN_MEMORY: a common sedan is always resident
    case 0x04AE: case 0x04AF: c.Out(0, c.I(1)); return true;                              // SET_LVAR_INT_TO_CONSTANT
    case 0x03D0: c.result = true; return true;                               // HAS_MISSION_AUDIO_LOADED (the bank is read when it plays)
    case 0x00D8: c.t.finished = true; return true;                           // MISSION_HAS_FINISHED: the mission thread ends
    case 0x00BF: { int h = 12, m = 0; if (scm && scm->host.getTime) scm->host.getTime(h, m); c.Out(0, h); c.Out(1, m); std::fprintf(stderr, "[SCMGAME] GET_TIME_OF_DAY %02d:%02d\n", h, m); return true; }   // GET_TIME_OF_DAY
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
    auto carAlive = [&](int h) -> VehicleInstance* { VehicleInstance* v = cars && h >= 1 && (size_t)h <= cars->Count() ? &cars->Car((size_t)h - 1) : nullptr; return v && !v->removed ? v : nullptr; };
    const std::string name = ScmCommandName(c.op);
    if (name.compare(0, 7, "LOCATE_") == 0 && Locate(c, name)) return true;
    if (name.size() > 12 && name.compare(0, 3, "IS_") == 0 && name.find("_IN_AREA_") != std::string::npos && InArea(c, name)) return true;
    switch (c.op) {
    // ---- maths and units (CRunningScript::ProcessCommands*) ---------------------------------
    case 0x01FB: c.OutF(1, std::sqrt(c.F(0))); return true;                                   // SQRT
    case 0x02F6: c.OutF(1, std::sin(c.F(0) * 0.017453292f)); return true;                      // SIN (degrees)
    case 0x02F7: c.OutF(1, std::cos(c.F(0) * 0.017453292f)); return true;                      // COS
    case 0x0509: c.OutF(4, std::hypot(c.F(0) - c.F(2), c.F(1) - c.F(3))); return true;         // GET_DISTANCE_BETWEEN_COORDS_2D
    case 0x050A: c.OutF(6, std::sqrt((c.F(0) - c.F(3)) * (c.F(0) - c.F(3)) + (c.F(1) - c.F(4)) * (c.F(1) - c.F(4)) + (c.F(2) - c.F(5)) * (c.F(2) - c.F(5)))); return true;   // GET_DISTANCE_BETWEEN_COORDS_3D
    case 0x0604: {   // GET_HEADING_FROM_VECTOR_2D: CGeneral::GetATanOfXY * 57.295776 - 90, wrapped to 0..360
        float a = std::atan2(c.F(1), c.F(0));
        if (a < 0.0f) a += 6.2831853f;
        float h = a * 57.295776f - 90.0f;
        while (h < 0.0f) h += 360.0f;
        c.OutF(2, h); return true;
    }
    case 0x05A4: {   // GET_ANGLE_BETWEEN_2D_VECTORS(x1, y1, x2, y2): heading from the first point to the second, degrees
        float a = std::atan2(-(c.F(2) - c.F(0)), c.F(3) - c.F(1)) * 57.295776f;
        while (a < 0.0f) a += 360.0f;
        c.OutF(4, a); return true;
    }
    case 0x05A5: {   // DO_2D_RECTANGLES_COLLIDE(x1, y1, w1, h1, x2, y2, w2, h2): centres and sizes
        const float x1 = c.F(0), y1 = c.F(1), w1 = c.F(2), h1 = c.F(3), x2 = c.F(4), y2 = c.F(5), w2 = c.F(6), h2 = c.F(7);
        c.result = y1 - h1 * 0.5f <= y2 + h2 * 0.5f && y2 - h2 * 0.5f <= y1 + h1 * 0.5f && x1 + w1 * 0.5f >= x2 - w2 * 0.5f && x2 + w2 * 0.5f >= x1 - w1 * 0.5f;
        return true;
    }
    case 0x0424: c.result = true; return true;                                                  // ARE_MEASUREMENTS_IN_METRES
    case 0x0425: c.OutF(1, c.F(0) * 3.3333333f); return true;                                   // CONVERT_METRES_TO_FEET
    case 0x042D: c.Out(1, (int)(c.F(0) * 3.3333333f)); return true;                             // CONVERT_METRES_TO_FEET_INT
    case 0x07D6: c.result = c.I(0) == c.I(1); return true;                                      // IS_INT_LVAR_EQUAL_TO_INT_VAR
    case 0x0653: c.OutF(1, Stats::Get(c.I(0))); return true;                                    // GET_FLOAT_STAT
    case 0x0627: case 0x0628: return true;                                                      // REGISTER_INT_STAT / REGISTER_FLOAT_STAT
    // ---- debug and PC-only commands the retail build does nothing visible with ----------------
    case 0x03A9: case 0x0663: case 0x065D: case 0x065E: case 0x086A: case 0x09D8: case 0x0A3F: case 0x091B: return true;   // debug file / view variables / second pad / co-op / console reset
    case 0x03A4: c.t.name = c.S(0); return true;                                                // SCRIPT_NAME
    case 0x0A4B: case 0x0736: case 0x09FA: c.result = false; return true;                       // IS_PC_USING_JOYPAD, IS_PS2_KEYBOARD_KEY_JUST_PRESSED, HAS_GAME_JUST_RETURNED_FROM_FRONTEND
    case 0x0A4A: c.Out(0, 0); c.Out(1, 0); return true;                                         // GET_PC_MOUSE_MOVEMENT
    case 0x091A: c.Out(0, 0); return true;                                                      // GET_LATEST_CONSOLE_COMMAND
    // ---- score (the player's cash) -----------------------------------------------------------
    case 0x0109: if (scm && scm->host.addMoney) scm->host.addMoney(c.I(1)); return true;       // ADD_SCORE(player, n)
    case 0x010A: c.result = scm && scm->host.getMoney && scm->host.getMoney() > c.I(1); return true;   // IS_SCORE_GREATER(player, n)
    case 0x010B: c.Out(1, scm && scm->host.getMoney ? scm->host.getMoney() : 0); return true;   // STORE_SCORE(player) -> n
    // ---- cars: queries and settings ------------------------------------------------------------
    case 0x0137: { VehicleInstance* v = carAlive(c.I(0)); c.result = v && v->model == c.I(1); return true; }   // IS_CAR_MODEL
    case 0x0441: { VehicleInstance* v = carAlive(c.I(0)); c.Out(1, v ? v->model : -1); return true; }          // GET_CAR_MODEL
    case 0x0174: { VehicleInstance* v = carAlive(c.I(0)); c.OutF(1, v ? v->heading * 57.295776f : 0.0f); return true; }   // GET_CAR_HEADING (degrees)
    case 0x0172: {   // GET_CHAR_HEADING(char) (degrees)
        float h = 0.0f;
        if (c.I(0) == kPlayer) { if (getPlayerHeading) h = getPlayerHeading(); }
        else { const int i = CharIndex(c.I(0)); if (i >= 0) h = peds->Heading((size_t)i); }
        c.OutF(1, h * 57.295776f); return true;
    }
    case 0x01C1: { VehicleInstance* v = carAlive(c.I(0)); c.result = v && std::fabs(v->speed) < 0.01f; return true; }   // IS_CAR_STOPPED
    case 0x020D: { VehicleInstance* v = carAlive(c.I(0)); c.result = v != nullptr; return true; }               // IS_CAR_UPRIGHT: cars do not roll over yet
    case 0x0227: { VehicleInstance* v = carAlive(c.I(0)); c.Out(1, v ? (int)v->health : 0); return true; }     // GET_CAR_HEALTH
    case 0x056E: c.result = carAlive(c.I(0)) != nullptr; return true;                                           // DOES_VEHICLE_EXIST
    case 0x056D: c.result = c.I(0) == kPlayer || CharIndex(c.I(0)) >= 0; return true;                           // DOES_CHAR_EXIST
    case 0x0519: if (VehicleInstance* v = carAlive(c.I(0)); v && c.I(1)) m_frozenCars[c.I(0)] = {v->x, v->y, v->z}; else m_frozenCars.erase(c.I(0)); return true;        // FREEZE_CAR_POSITION(car, on)
    case 0x07D5: if (VehicleInstance* v = carAlive(c.I(0))) { v->vx += c.F(1); v->vy += c.F(2); } return true;   // APPLY_FORCE_TO_CAR(car, fx, fy, fz, ox, oy, oz): per-mass impulse, no mass model
    case 0x0431: c.result = carAlive(c.I(0)) != nullptr; return true;                                           // IS_CAR_PASSENGER_SEAT_FREE: no passengers are simulated
    case 0x0432: c.Out(2, -1); return true;                                                                      // GET_CHAR_IN_CAR_PASSENGER_SEAT
    case 0x0A32: c.result = false; return true;                                                                  // IS_CHAR_ATTACHED_TO_ANY_CAR
    case 0x096E: case 0x0803: c.result = false; return true;                                                     // IS_CAR_LOW_RIDER, DOES_CAR_HAVE_HYDRAULICS
    case 0x096D: c.Out(2, 0); return true;                                                                       // GET_CURRENT_CAR_MOD: nothing fitted
    case 0x0811: c.Out(1, c.I(0) == kPlayer && playerCar && playerCar() >= 0 ? playerCar() + 1 : (m_charCar.count(c.I(0)) ? m_charCar[c.I(0)] : -1)); return true;   // GET_CAR_CHAR_IS_USING
    case 0x056C: case 0x0602: {   // IS_CHAR_IN_ANY_POLICE_VEHICLE / IS_CHAR_IN_TAXI
        int h = -1;
        if (c.I(0) == kPlayer) { if (playerInCar && playerInCar() && playerCar) h = playerCar() + 1; }
        else { const auto it = m_charCar.find(c.I(0)); if (it != m_charCar.end()) h = it->second; }
        const VehicleInstance* v = carAlive(h);
        const int m = v ? v->model : 0;
        if (c.op == 0x0602) c.result = m == 420 || m == 438;
        else c.result = m == 427 || m == 430 || m == 432 || m == 433 || m == 490 || m == 523 || m == 528 || (m >= 596 && m <= 599) || m == 601;
        return true;
    }
    case 0x0878: case 0x02AC: case 0x07F5: return true;                                                          // SET_VEHICLE_DIRT_LEVEL, SET_CAR_PROOFS, CONTROL_CAR_HYDRAULICS: no effect yet
    case 0x02CE: {   // GET_GROUND_Z_FOR_3D_COORD(x, y, z) -> z
        float g = groundAt ? groundAt(c.F(0), c.F(1)) : -1e6f;
        c.OutF(3, g > -1e5f ? g : 0.0f); return true;
    }
    // ---- chars: queries and settings -----------------------------------------------------------
    case 0x02F2: {   // IS_CHAR_MODEL(char, model)
        if (c.I(0) == kPlayer) { c.result = c.I(1) == 0; return true; }
        const int i = CharIndex(c.I(0));
        c.result = i >= 0 && peds->ModelOf((size_t)i) == c.I(1);
        return true;
    }
    case 0x0337: { const int i = CharIndex(c.I(0)); if (i >= 0) peds->SetHidden((size_t)i, !c.I(1)); return true; }   // SET_CHAR_VISIBLE
    case 0x0376: {   // CREATE_RANDOM_CHAR(x, y, z) -> char
        if (!peds || !r) { c.Out(3, 0); return true; }
        float z = c.F(2);
        if (groundAt) { const float g = groundAt(c.F(0), c.F(1)); if (g > -1e5f && z < g + 0.2f) z = g; }
        const int i = peds->Spawn(*r, -1, c.F(0), c.F(1), z, 0.0f);
        if (i < 0) { c.Out(3, 0); return true; }
        peds->SetScripted((size_t)i, true);
        c.Out(3, peds->Uid((size_t)i));
        return true;
    }
    case 0x02A9: case 0x02AB: case 0x0237: case 0x03BF: case 0x01C5: case 0x03FE: case 0x0393: case 0x04D7: case 0x0619:   // char/gang flags: only-damaged-by-player, proofs, gang weapons, ignore player, keep alive, money, anim speed, freeze, collision
    case 0x0489: case 0x09F5: case 0x094E: case 0x094F: case 0x0967: case 0x09AB:                                         // speech: shut up, disable/enable, facial talk, passenger say
    case 0x015B: case 0x018C: case 0x0221: case 0x0299: case 0x014F: case 0x0151:                                         // pad shake, one-off sound, brakes, garage, clear on-screen timer / counter
        return true;
    case 0x094D: case 0x00E1: case 0x02E0: case 0x074F: c.result = false; return true;                                   // IS_CHAR_TALKING, IS_BUTTON_PRESSED, IS_CHAR_SHOOTING, IS_CHAR_RESPONDING_TO_EVENT: nothing reports these yet
    // ---- objects --------------------------------------------------------------------------------
    case 0x0108: if (c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size()) m_objs[(size_t)c.I(0) - 1].removed = true; return true;   // DELETE_OBJECT
    case 0x01BC: case 0x0815: if (c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size()) { Obj& o = m_objs[(size_t)c.I(0) - 1]; o.x = c.F(1); o.y = c.F(2); o.z = c.F(3); } return true;   // SET_OBJECT_COORDINATES (_AND_VELOCITY: the velocity is not simulated)
    case 0x034E: {   // SLIDE_OBJECT(obj, x, y, z, sx, sy, sz, collision): jumps to the target, reports done
        if (c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size()) { Obj& o = m_objs[(size_t)c.I(0) - 1]; o.x = c.F(1); o.y = c.F(2); o.z = c.F(3); }
        c.result = true; return true;
    }
    case 0x04E6: {   // LOCATE_OBJECT_3D(obj, x, y, z, rx, ry, rz, draw)
        if (c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size() && !m_objs[(size_t)c.I(0) - 1].removed) {
            const Obj& o = m_objs[(size_t)c.I(0) - 1];
            c.result = std::fabs(o.x - c.F(1)) <= c.F(4) && std::fabs(o.y - c.F(2)) <= c.F(5) && std::fabs(o.z - c.F(3)) <= c.F(6);
        } else c.result = false;
        return true;
    }
    case 0x0984: c.Out(1, c.I(0) >= 1 && (size_t)c.I(0) <= m_objs.size() ? m_objs[(size_t)c.I(0) - 1].model : -1); return true;   // GET_OBJECT_MODEL
    case 0x04DA: c.result = false; return true;                                                  // HAS_OBJECT_COLLIDED_WITH_ANYTHING: objects have no physics yet
    case 0x0381: case 0x05A2: case 0x070B: case 0x0418: case 0x04D9: case 0x0797: case 0x0798: case 0x0799: return true;   // object velocity, rotation velocity, drop, draw last, record collisions, ropes: no effect yet
    // ---- text and hud -----------------------------------------------------------------------------
    case 0x02FD: case 0x02FF: case 0x0302: case 0x0308: {   // PRINT_WITH_2/3/4/6_NUMBERS(label, n..., ms, flag)
        const int k = c.op == 0x02FD ? 2 : c.op == 0x02FF ? 3 : c.op == 0x0302 ? 4 : 6;
        int n[6] = {};
        for (int i = 0; i < k; i++) n[i] = c.I(1 + i);
        m_subtitle = WithNumbers(Text(c.S(0)), n, k);
        m_subTimer = std::max(0.1f, c.I(1 + k) / 1000.0f);
        return true;
    }
    case 0x045B: {   // DISPLAY_TEXT_WITH_2_NUMBERS(x, y, label, n1, n2)
        const int n[2] = {c.I(3), c.I(4)};
        AddText(c.F(0), c.F(1), WithNumbers(Text(c.S(2)), n, 2));
        return true;
    }
    case 0x0512: if (hud) hud->ShowMessage(Text(c.S(0)), 3600.0f); return true;                  // PRINT_HELP_FOREVER
    case 0x076F: c.result = m_subTimer > 0.0f && !m_subtitle.empty(); return true;               // IS_MESSAGE_BEING_DISPLAYED
    case 0x08FE: c.result = hud && hud->MessageShown(); return true;                             // IS_HELP_MESSAGE_BEING_DISPLAYED
    case 0x0A2A: c.result = hud && hud->MessageShown() && hud->Message() == Text(c.S(0)); return true;   // IS_THIS_HELP_MESSAGE_BEING_DISPLAYED
    case 0x0581: radarOn = c.I(0) != 0; return true;                                             // DISPLAY_RADAR
    case 0x0826: hudOn = c.I(0) != 0; return true;                                               // DISPLAY_HUD
    case 0x0989: case 0x09EE: case 0x03C3: case 0x04F7: return true;                            // help box size, forced big message, on-screen timer / counter with string: no effect yet
    case 0x09FD: c.OutF(1, 0.0f); return true;                                                   // GET_STRING_WIDTH: font metrics are not exposed to the script yet
    case 0x0904: c.Out(1, 255); c.Out(2, 255); c.Out(3, 255); c.Out(4, 255); return true;        // GET_HUD_COLOUR: white
    // ---- save, input, audio and world switches with nothing behind them yet -------------------------
    case 0x03D8: case 0x043C: case 0x04F9: case 0x04FA: case 0x057E: case 0x07B4: case 0x07CC: case 0x0830: case 0x08DC: case 0x08F5:
    case 0x0949: case 0x094A: case 0x097A: case 0x097B: case 0x0A41: case 0x0A43: return true;
    case 0x03D9: c.result = true; return true;                                                   // HAS_SAVE_GAME_FINISHED
    case 0x0494: c.Out(1, 0); c.Out(2, 0); c.Out(3, 0); c.Out(4, 0); return true;                 // GET_POSITION_OF_ANALOGUE_STICKS
    case 0x07B1: c.OutF(0, 0.0f); return true;                                                   // GET_BEAT_PROXIMITY
    // Script menus (CMenuSystem standard menus), drawn from Update with the script text and rectangle queues.
    case 0x08D4: {   // CREATE_MENU(title, x, y, width, columns, interactive, background, alignment) -> menu
        int id = 0;
        for (const Menu& mm : m_menus) if (mm.used) id++;
        if (id > 1) { c.Out(8, 0); return true; }
        Menu& m = m_menus[id];
        m = Menu{};
        m.used = true;
        const std::string t = c.S(0);
        m.title = t == "DUMMY" ? std::string() : t;
        m.x = c.F(1); m.y = c.F(2);
        m.columns = std::min(4, c.I(4));
        m.interactive = c.I(5) != 0; m.background = c.I(6) != 0;
        for (int i = 0; i < 4; i++) { m.width[i] = c.F(3); m.align[i] = c.I(7); }
        for (int i = 0; i < 4; i++) for (int r = 0; r < 12; r++) m.n1[i][r] = m.n2[i][r] = -1;
        for (int r = 0; r < 12; r++) m.on[r] = true;
        m_curMenu = 0;
        c.Out(8, id);
        return true;
    }
    case 0x08D7: {   // GET_MENU_ITEM_SELECTED(menu): the highlighted row, -99 when the menu is not in use
        const int id = c.I(0);
        c.Out(1, id >= 0 && id < 2 && m_menus[id].used ? m_menus[id].active : -99);
        return true;
    }
    case 0x08DA: {   // DELETE_MENU
        const int id = c.I(0);
        if (id >= 0 && id < 2 && m_menus[id].used) {
            m_menus[id] = Menu{};
            m_curMenu = m_menus[0].used ? 0 : (m_menus[1].used ? 1 : 0);
        }
        return true;
    }
    case 0x08DB: {   // SET_MENU_COLUMN(menu, column, header, 12 rows)
        const int id = c.I(0), col = c.I(1);
        if (id < 0 || id > 1 || !m_menus[id].used || col < 0 || col > 3) return true;
        Menu& m = m_menus[id];
        auto lab = [&](size_t i) { const std::string v = c.S(i); return v == "DUMMY" ? std::string() : v; };
        m.head[col] = lab(2);
        for (int r = 0; r < 12; r++) { m.cell[col][r] = lab(3 + (size_t)r); m.n1[col][r] = m.n2[col][r] = -1; }
        if (col == 0) MenuRows(m);
        return true;
    }
    case 0x08EE: {   // SET_MENU_ITEM_WITH_NUMBER(menu, column, row, text, number)
        const int id = c.I(0), col = c.I(1), row = c.I(2);
        if (id < 0 || id > 1 || !m_menus[id].used || col < 0 || col > 3 || row < 0 || row > 11) return true;
        Menu& m = m_menus[id];
        const std::string v = c.S(3);
        m.cell[col][row] = v == "DUMMY" ? std::string() : v;
        m.n1[col][row] = c.I(4); m.n2[col][row] = -1;
        MenuRows(m);
        return true;
    }
    case 0x090E: {   // SET_ACTIVE_MENU_ITEM(menu, row)
        const int id = c.I(0);
        if (id >= 0 && id < 2 && m_menus[id].used) m_menus[id].active = c.I(1);
        return true;
    }
    case 0x08E2: c.Out(1, 0); return true;                                                       // GET_TERRITORY_UNDER_CONTROL_PERCENTAGE
    case 0x0956: c.Out(1, 0); return true;                                                       // FIND_MAX_NUMBER_OF_GROUP_MEMBERS
    case 0x0981: case 0x0596: case 0x051A: case 0x0611: case 0x02D8: c.result = false; return true;   // HAS_TRAIN_DERAILED, IS_PLAYER_IN_SHORTCUT_TAXI, HAS_CHAR_BEEN_DAMAGED_BY_CHAR, IS_CHAR_PLAYING_ANIM, IS_CURRENT_CHAR_WEAPON
    case 0x0613: c.OutF(2, 0.0f); return true;                                                   // GET_CHAR_ANIM_CURRENT_TIME
    case 0x0614: case 0x0655: case 0x06A9: case 0x0729: case 0x0812: case 0x0159: return true;   // anim time, look-at tasks, mobile phone, non-interruptable anim, camera at char
    case 0x0632: case 0x0643: case 0x065C: case 0x0708: case 0x0709: case 0x07E5: case 0x0978: return true;   // groups, sequences, decision makers: no effect yet
    case 0x09F2: c.result = false; return true;                                                  // DOES_DECISION_MAKER_EXIST
    case 0x05CD: {   // TASK_LEAVE_CAR(char, car)
        if (c.I(0) == kPlayer) { if (playerLeaveCar) playerLeaveCar(); }
        else m_charCar.erase(c.I(0));
        return true;
    }
    case 0x0762: { const int i = CharIndex(c.I(0)); if (i >= 0) peds->Damage(i, 1000.0f, false); return true; }   // TASK_DEAD
    // ---- 2D drawing: rectangles and sprites (CTheScripts::IntroRectangles) -------------------------
    case 0x0390: if (hud) hud->LoadScriptTxd(c.S(0)); return true;                               // LOAD_TEXTURE_DICTIONARY
    case 0x0391: if (hud) hud->FreeScriptTxd(); m_spriteTex.clear(); return true;                // REMOVE_TEXTURE_DICTIONARY
    case 0x038F: {   // LOAD_SPRITE(slot, name): names are looked up lower case
        std::string n = c.S(1);
        for (char& ch : n) ch = (char)std::tolower((unsigned char)ch);
        m_spriteTex[c.I(0)] = n;
        return true;
    }
    case 0x038E: case 0x038D: case 0x074B: {   // DRAW_RECT(x, y, w, h, r, g, b, a) / DRAW_SPRITE(slot, x, y, w, h, r, g, b, a) / _WITH_ROTATION(slot, x, y, w, h, angle, r, g, b, a)
        const bool tex = c.op != 0x038E;
        const int o = tex ? 1 : 0;
        ScriptSprite sp;
        const float w = c.F(o + 2), h = c.F(o + 3);
        sp.x0 = c.F(o) - w * 0.5f; sp.x1 = c.F(o) + w * 0.5f;
        sp.y0 = c.F(o + 1) - h * 0.5f; sp.y1 = c.F(o + 1) + h * 0.5f;
        int k = o + 4;
        if (c.op == 0x074B) sp.angle = c.F(k++) * 0.017453292f;
        auto ch = [&](int v) { return (uint32_t)std::clamp(v, 0, 255); };
        sp.colour = (ch(c.I(k + 3)) << 24) | (ch(c.I(k)) << 16) | (ch(c.I(k + 1)) << 8) | ch(c.I(k + 2));
        if (tex) { const auto it = m_spriteTex.find(c.I(0)); if (it == m_spriteTex.end()) return true; sp.tex = it->second; }
        sp.beforeFade = m_spritesBefore;
        m_spriteQueue.push_back(sp);
        return true;
    }
    case 0x0937: {   // DRAW_WINDOW(x1, y1, x2, y2, label, style): a dark box with its caption; the retail text-box styles are not reproduced
        ScriptSprite sp;
        sp.x0 = c.F(0); sp.y0 = c.F(1); sp.x1 = c.F(2); sp.y1 = c.F(3);
        sp.colour = 0xB4000000;
        const std::string lab = c.S(4);
        if (lab != "DUMMY") sp.label = Text(lab);
        sp.beforeFade = m_spritesBefore;
        m_spriteQueue.push_back(sp);
        return true;
    }
    case 0x08A9: return true;   // STREAM_SCRIPT: the script data is all resident
    case 0x0656: { float a = c.F(0); while (a < 0.0f) a += 360.0f; while (a > 360.0f) a -= 360.0f; c.OutF(0, a); return true; }   // LIMIT_ANGLE(var): wraps the variable into 0..360
    // ---- cars, pickups and models: remaining queries ---------------------------------------------------
    case 0x0704: if (carAlive(c.I(0))) m_drives[-c.I(0)] = Drive{c.I(0), c.F(1), c.F(2), 14.0f}; return true;   // CAR_GOTO_COORDINATES_RACING(car, x, y, z): drives straight there
    case 0x073E: {   // GET_RANDOM_CAR_IN_SPHERE_NO_SAVE(x, y, z, radius, model) -> car, the closest match or -1
        int best = -1; float bestD = c.F(3);
        for (size_t i = 0; cars && i < cars->Count(); i++) {
            const VehicleInstance& v = cars->Car(i);
            if (v.removed || (c.I(4) >= 0 && v.model != c.I(4))) continue;
            const float d = std::sqrt((v.x - c.F(0)) * (v.x - c.F(0)) + (v.y - c.F(1)) * (v.y - c.F(1)) + (v.z - c.F(2)) * (v.z - c.F(2)));
            if (d < bestD) { bestD = d; best = (int)i + 1; }
        }
        c.Out(5, best);
        return true;
    }
    case 0x04A6: c.Out(5, AddPickup(-1, 0, 0, c.F(0), c.F(1), c.F(2))); return true;   // CREATE_PROTECTION_PICKUP(x, y, z, rate, rate2) -> pickup
    case 0x08EC: { const VehicleInfo* vi = cars ? cars->Info(c.I(0) >= 1 && (size_t)c.I(0) <= cars->Count() ? cars->Car((size_t)c.I(0) - 1).model : -1) : nullptr; c.Out(1, vi ? vi->vclass : 0); return true; }   // GET_VEHICLE_CLASS(car) -> vehicles.ide class
    case 0x09E1: {                                                                              // GET_CAR_MODEL_VALUE(model): handling.cfg nMonetaryValue
        const VehicleInfo* vi = cars ? cars->Info(c.I(0)) : nullptr;
        const HandlingData* hd = vi ? cars->Handling(vi->handling) : nullptr;
        c.Out(1, hd ? hd->monetaryValue : 0); return true;
    }
    case 0x07E4: for (int i = 1; i <= 6; i++) c.OutF(i, 0.0f); return true;   // GET_MODEL_DIMENSIONS(model): collision bounds of a model are not exposed yet
    case 0x080A: return true;                   // GET_LEVEL_DESIGN_COORDS_FOR_OBJECT
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
    case 0x0844: case 0x0845: c.result = c.S(0).empty(); return true;                          // IS_(L)VAR_TEXT_LABEL_EMPTY
    case 0x0846: case 0x0847: c.result = c.S(0, 16).empty(); return true;                     // IS_(L)VAR_TEXT_LABEL16_EMPTY
    case 0x05AD: case 0x05AE: c.result = c.S(0) == c.S(1); return true;                       // IS_(L)VAR_TEXT_LABEL_EQUAL_TO_TEXT_LABEL
    case 0x08F9: case 0x08FA: c.result = c.S(0, 16) == c.S(1); return true;                   // IS_(L)VAR_TEXT_LABEL16_EQUAL_TO_TEXT_LABEL
    case 0x01C0: c.Out(1, wantedLevel ? wantedLevel() : 0); return true;                      // STORE_WANTED_LEVEL
    case 0x07B0: c.Out(0, -1); return true;                                                   // GET_LOADED_SHOP: none
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
    case 0x04BB: fprintf(stderr, "[SCMGAME] SET_AREA_VISIBLE %d by %s pc 0x%X\n", c.I(0), c.t.name.c_str(), c.t.pc); if (setArea) setArea(c.I(0)); return true;                                 // SET_AREA_VISIBLE
    case 0x0860: if (c.I(0) == kPlayer && setArea) setArea(c.I(1)); return true;            // SET_CHAR_AREA_VISIBLE
    case 0x07FB: if (getenv("GTA_SCMGAME")) fprintf(stderr, "[SCMGAME] SWITCH_ENTRY_EXIT %s %d by %s\n", c.S(0).c_str(), c.I(1), c.t.name.c_str()); if (enex) enex->SetEnabledByName(c.S(0).c_str(), c.I(1) != 0); return true;   // SWITCH_ENTRY_EXIT
    case 0x08E7: if (getenv("GTA_SCMGAME")) fprintf(stderr, "[SCMGAME] DISABLE_ALL_ENTRY_EXITS %d by %s\n", c.I(0), c.t.name.c_str()); if (enex) enex->SetAllDisabled(c.I(0) != 0); return true;                  // DISABLE_ALL_ENTRY_EXITS
    case 0x094B: c.OutStr(1, enex ? enex->LastName() : ""); return true;                    // GET_NAME_OF_ENTRY_EXIT_CHAR_USED
    case 0x0317: return true;                                                                // REGISTER_MISSION_GIVEN
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
    case 0x03CF: {   // LOAD_MISSION_AUDIO(slot, sampleId): CAudioEngine::PreloadMissionAudio
        const int slot = c.I(0) - 1, ev = c.I(1);
        if (slot < 0 || slot > 3) return true;
        MissionAudio& m = m_mission[slot];
        if (m.voice && voicePlaying && voicePlaying(m.voice)) return true;   // a playing sample blocks the preload
        m = MissionAudio();
        if (ev >= 2000 && ev != 65535) {   // GetBankAndSoundFromScriptSlotAudioEvent: 200 sounds per bank from bank 147
            m.bank = 0x93 + (ev - 2000) / 200;
            m.sound = (ev - 2000) % 200;
            m.event = ev;
        }
        if (getenv("GTA_SCMGAME")) fprintf(stderr, "[SCMGAME] LOAD_MISSION_AUDIO slot %d event %d -> bank %d sound %d\n", slot + 1, ev, m.bank, m.sound);
        return true;
    }
    case 0x03D1: {   // PLAY_MISSION_AUDIO(slot)
        const int slot = c.I(0) - 1;
        if (slot < 0 || slot > 3) return true;
        MissionAudio& m = m_mission[slot];
        if (m.bank >= 0 && playSound) m.voice = playSound(m.bank, m.sound);
        if (getenv("GTA_SCMGAME")) fprintf(stderr, "[SCMGAME] PLAY_MISSION_AUDIO slot %d bank %d sound %d voice %d\n", slot + 1, m.bank, m.sound, m.voice);
        return true;
    }
    case 0x0A09: case 0x03E7: return true;   // LOAD_MISSION_AUDIO, PLAY_MISSION_AUDIO, SHUT_CHAR_UP_FOR_SCRIPTED_SPEECH, FLASH_HUD_OBJECT
    case 0x03D2: {   // HAS_MISSION_AUDIO_FINISHED(slot)
        const int slot = c.I(0) - 1;
        c.result = slot < 0 || slot > 3 || !m_mission[slot].voice || !voicePlaying || !voicePlaying(m_mission[slot].voice);
        if (getenv("GTA_SCMGAME")) fprintf(stderr, "[SCMGAME] HAS_MISSION_AUDIO_FINISHED slot %d -> %d\n", slot + 1, (int)c.result);
        return true;
    }
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
    // ---- mission set-up: cars, characters, tasks (instant where the retail task takes time) ----------
    case 0x00AB: {   // SET_CAR_COORDINATES(car, x, y, z)
        if (VehicleInstance* v = carAlive(c.I(0))) {
            float z = c.F(3);
            if (groundAt) { const float g = groundAt(c.F(1), c.F(2)); if (g > -1e5f && z < g + 0.3f) z = g + 0.9f; }
            v->x = c.F(1); v->y = c.F(2); v->z = z; v->speed = 0; v->vx = v->vy = 0;
        }
        return true;
    }
    case 0x0224: if (VehicleInstance* v = carAlive(c.I(0))) v->health = c.F(1); return true;   // SET_CAR_HEALTH
    case 0x0229: if (VehicleInstance* v = carAlive(c.I(0))) { v->paint[0] = c.I(1); v->paint[1] = c.I(2); } return true;   // CHANGE_CAR_COLOUR
    case 0x070C: if (cars && carAlive(c.I(0))) cars->Damage(c.I(0) - 1, 1e6f); return true;   // EXPLODE_CAR_IN_CUTSCENE
    case 0x0187: {   // ADD_BLIP_FOR_CHAR(char) -> blip
        float x, y, z;
        c.Out(1, CharPos(c.I(0), x, y, z) && scm && scm->host.addBlip ? scm->host.addBlip(x, y, z, 0, false) : 0);
        return true;
    }
    case 0x01B2: if (c.I(0) == kPlayer && giveWeapon) giveWeapon(c.I(1), c.I(2)); return true;   // GIVE_WEAPON_TO_CHAR(char, weapon, ammo)
    case 0x01B9: if (c.I(0) == kPlayer && setWeapon) setWeapon(c.I(1)); return true;             // SET_CURRENT_CHAR_WEAPON(char, weapon)
    case 0x05BA: { const int i = CharIndex(c.I(0)); if (i >= 0) peds->ClearGoal((size_t)i); return true; }   // TASK_STAND_STILL(char, ms)
    case 0x05CA: case 0x05CB: {   // TASK_ENTER_CAR_AS_PASSENGER(char, car, ms, seat) / TASK_ENTER_CAR_AS_DRIVER(char, car, ms)
        if (!carAlive(c.I(1))) return true;
        if (c.I(0) == kPlayer) { if (putPlayerInCar) putPlayerInCar(c.I(1) - 1); return true; }
        const int i = CharIndex(c.I(0));
        if (i >= 0) { peds->ClearGoal((size_t)i); peds->SetHidden((size_t)i, true); m_charCar[c.I(0)] = c.I(1); }
        return true;
    }
    case 0x00AA: {   // GET_CAR_COORDINATES(car) -> x, y, z
        if (VehicleInstance* v = carAlive(c.I(0))) { c.OutF(1, v->x); c.OutF(2, v->y); c.OutF(3, v->z); }
        return true;
    }
    case 0x02C1: c.OutF(3, c.F(0)); c.OutF(4, c.F(1)); c.OutF(5, c.F(2)); return true;   // GET_CLOSEST_CAR_NODE(x, y, z) -> x, y, z: no node data, the point itself
    case 0x02CA: c.result = false; return true;   // IS_CAR_ON_SCREEN
    case 0x04BA: if (VehicleInstance* v = carAlive(c.I(0))) v->speed = c.F(1); return true;   // SET_CAR_FORWARD_SPEED
    case 0x04C4: {   // GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS(char, ox, oy, oz) -> x, y, z
        float x = 0, y = 0, z = 0;
        if (CharPos(c.I(0), x, y, z)) {
            const float h = c.I(0) == kPlayer && getPlayerHeading ? getPlayerHeading() : 0.0f;
            const float s = std::sin(h), co = std::cos(h);
            c.OutF(4, x + c.F(1) * co - c.F(2) * s); c.OutF(5, y + c.F(1) * s + c.F(2) * co); c.OutF(6, z + c.F(3));
        }
        return true;
    }
    case 0x062E: c.Out(2, 7); return true;   // GET_SCRIPT_TASK_STATUS(char, task) -> status: FINISHED_TASK
    case 0x039F: return true;   // TURN_CAR_TO_FACE_COORD
    case 0x02BF: case 0x047A: c.result = false; return true;   // IS_CAR_IN_WATER, IS_CHAR_ON_ANY_BIKE
    case 0x022A: case 0x030C: case 0x0318: case 0x034F: case 0x0394: case 0x04EF: case 0x06E1: case 0x06FD:
    case 0x0733: case 0x0968: case 0x0998:   // ped roads, mission progress and pass bookkeeping, tunes, animation release, car missions, playback speed, talk
        return true;
    case 0x0448: c.result = false; return true;   // IS_CHAR_SITTING_IN_CAR
    case 0x09D0: c.result = true; return true;    // IS_VEHICLE_ON_ALL_WHEELS
    case 0x0674: case 0x0852: case 0x08AF: case 0x09F6: return true;   // CUSTOM_PLATE_FOR_NEXT_CAR, SET_CAR_CAN_BE_VISIBLY_DAMAGED, SET_CHAR_MAX_HEALTH, SET_CHAR_GET_OUT_UPSIDE_DOWN_CAR
    case 0x04EE: c.result = true; return true;   // HAS_ANIMATION_LOADED
    case 0x01F3: c.result = false; return true;   // IS_CAR_IN_AIR_PROPER
    case 0x0735: c.result = false; return true;   // IS_PS2_KEYBOARD_KEY_PRESSED
    case 0x03C0: c.Out(1, c.I(0) == kPlayer && playerCar && playerCar() >= 0 ? playerCar() + 1 : -1); return true;   // STORE_CAR_CHAR_IS_IN_NO_SAVE(char) -> car
    case 0x04FC: for (int i = 1; i <= 6; i++) c.Out(i, 0); return true;   // GET_WHEELIE_STATS
    // tasks, sequences, freezes and requests with nothing to act on yet
    case 0x0350: case 0x04EB: case 0x04ED: case 0x0555: case 0x0574: case 0x0575: case 0x05C0:
    case 0x0615: case 0x0616: case 0x0618: case 0x061B: case 0x0647:
        return true;
    case 0x060A: c.Out(1, 1); return true;   // LOAD_CHAR_DECISION_MAKER(type) -> handle
    // flags and settings with nothing to act on yet
    case 0x020A: case 0x0245: case 0x02E2: case 0x038B: case 0x039E: case 0x03EF: case 0x03F5: case 0x041E:
    case 0x0446: case 0x04E0: case 0x0526: case 0x053F: case 0x0568: case 0x0587: case 0x0605: case 0x060B:
    case 0x06AB: case 0x0713: case 0x0732: case 0x077A: case 0x07A1: case 0x07DD: case 0x08C6: case 0x09D2:
    case 0x0802:
        return true;
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


// CMenuSystem::InsertMenu / InsertOneMenuItemWithNumber: the row count runs to the last filled row of column 0.
void ScriptGame::MenuRows(Menu& m) {
    m.rows = 0;
    for (int r = 0; r < 12; r++) if (!m.cell[0][r].empty()) m.rows = r + 1;
    MenuFix(m);
}

// CMenuSystem::ActivateItems: the active row moves on to the next live row, or back to the first.
void ScriptGame::MenuFix(Menu& m) {
    auto valid = [&](int r) { return r >= 0 && r < 12 && m.on[r] && !m.cell[0][r].empty(); };
    while (!valid(m.active) && m.active < m.rows) m.active++;
    if (m.active >= m.rows) { m.active = 0; while (!valid(m.active) && m.active < m.rows) m.active++; }
    if (m.active >= m.rows) m.active = 0;
}

// CMenuSystem::Process for the menu in use: up and down move the highlight, accept marks the row.
void ScriptGame::MenuUpdate() {
    if (!m_menus[m_curMenu].used) m_curMenu = m_menus[0].used ? 0 : 1;
    for (int i = 0; i < 2; i++) {
        Menu& m = m_menus[i];
        if (!m.used) continue;
        const bool cur = i == m_curMenu;
        if (cur && m.interactive) {
            const int in = menuInput ? menuInput() : 0;
            auto valid = [&](int r) { return r >= 0 && r < 12 && m.on[r] && !m.cell[0][r].empty(); };
            if (in & 4) { if (m.on[std::max(0, std::min(11, m.active))]) m.accepted = m.active; }
            if (m.accepted != m.active) m.accepted = 0x9d;
            if (m.rows > 1) {
                if (in & 1) {
                    do { m.active--; } while (m.active >= 0 && !valid(m.active));
                    if (m.active < 0) { m.active = m.rows; do { m.active--; } while (m.active >= 0 && !valid(m.active)); }
                }
                if (in & 2) {
                    do { m.active++; } while (m.active < m.rows && !valid(m.active));
                    if (m.active >= m.rows) { m.active = -1; do { m.active++; } while (m.active < m.rows && !valid(m.active)); }
                }
                if (m.active < 0) m.active = 0;
            }
        }
        MenuDraw(m, cur);
    }
}

// CMenuSystem::DisplayStandardMenu. The retail layout leans on font metrics this port does not read, so the
// rows sit on a fixed 16 unit pitch and the highlight colour is a guess.
void ScriptGame::MenuDraw(const Menu& m, bool current) {
    const bool head = !m.head[0].empty() || !m.head[1].empty() || !m.head[2].empty() || !m.head[3].empty();
    float total = 20.0f;
    for (int i = 0; i < m.columns; i++) total += m.width[i];
    const float top = m.y + 20.0f + (head ? 20.0f : 0.0f);
    if (m.background) {
        ScriptSprite sp;
        sp.x0 = m.x; sp.y0 = m.y; sp.x1 = m.x + total; sp.y1 = top + 16.0f * (float)m.rows + 6.0f;
        sp.colour = 0xBE000000;
        if (!m.title.empty()) sp.label = Text(m.title);
        sp.beforeFade = false;
        m_spriteQueue.push_back(sp);
    }
    auto put = [&](int col, float y, const std::string& txt, uint32_t colour) {
        float x = m.x + 10.0f;
        for (int i = 0; i < col; i++) x += m.width[i];
        ScriptTextItem it;
        it.sx = 0.52f; it.sy = 1.0f; it.font = 1; it.shadow = 2; it.colour = colour; it.wrap = 640.0f;
        if (m.align[col] == 0) { it.centre = true; x += m.width[col] * 0.5f; }
        else if (m.align[col] == 2) { it.right = true; x += m.width[col]; }
        it.x = x; it.y = y; it.text = txt;
        m_queue.push_back(it);
    };
    for (int col = 0; col < m.columns; col++) {
        if (!m.head[col].empty()) put(col, m.y + 20.0f, Text(m.head[col]), 0xFFE1E1E1);
        for (int r = 0; r < m.rows; r++) {
            if (m.cell[col][r].empty()) continue;
            std::string t = Text(m.cell[col][r]);
            const int n[2] = {m.n1[col][r], m.n2[col][r]};
            for (int k = 0; k < 2 && n[k] != -1; k++) { const size_t p = t.find("~1~"); if (p != std::string::npos) t.replace(p, 3, std::to_string(n[k])); }
            const bool act = m.interactive && current && r == m.active && m.on[r];
            put(col, top + 16.0f * (float)r, t, act ? 0xFFFFE600 : (m.on[r] ? 0xFFE1E1E1 : 0xFF808080));
        }
    }
}

// ---- CVehicleRecording ---------------------------------------------------------------------
bool ScriptGame::RecordingLoaded(int number) {
    if (m_recs.count(number)) return true;
    if (!m_carrecOpen) { m_carrecOpen = m_carrecImg.Open(gameDir + "/data/Paths/carrec.img") || m_carrecImg.Open(gameDir + "/data/paths/carrec.img"); if (!m_carrecOpen) return false; }
    char name[32];
    std::snprintf(name, sizeof(name), "carrec%d.rrr", number);
    std::vector<uint8_t> raw = m_carrecImg.Extract(name);
    if (raw.empty()) { std::fprintf(stderr, "[CARREC] %s not in carrec.img\n", name); m_recs[number]; return true; }
    size_t n = raw.size() / 32;
    std::vector<Frame> fr(n);
    std::memcpy(fr.data(), raw.data(), n * 32);
    for (size_t i = 1; i < n; i++) if (fr[i].time == 0) { n = i; break; }   // CVehicleRecording::Load: the first zero time past frame 0 ends the data
    fr.resize(n);
    std::fprintf(stderr, "[CARREC] %s: %zu frames, %u ms\n", name, n, n ? fr[n - 1].time : 0u);
    m_recs[number] = std::move(fr);
    return true;
}

int ScriptGame::PlaybackSlot(int car) const {
    for (int i = 0; i < 16; i++) if (m_play[i].on && m_play[i].car == car) return i;
    return -1;
}

bool ScriptGame::IsPlaybackGoingOn(int car) const { return PlaybackSlot(car) >= 0; }

// CVehicleRecording::RestoreInfoForCar / RestoreInfoForMatrix
void ScriptGame::RestoreFrame(VehicleInstance& v, const Frame& f, bool pause) {
    float r[3], t[3];
    for (int k = 0; k < 3; k++) { r[k] = f.right[k] * 0.007874016f; t[k] = f.top[k] * 0.007874016f; }
    v.rot[0] = r[0]; v.rot[1] = r[1]; v.rot[2] = r[2];
    v.rot[3] = t[0]; v.rot[4] = t[1]; v.rot[5] = t[2];
    v.rot[6] = r[1] * t[2] - r[2] * t[1]; v.rot[7] = r[2] * t[0] - r[0] * t[2]; v.rot[8] = r[0] * t[1] - r[1] * t[0];
    v.hasRot = true;
    v.x = f.pos[0]; v.y = f.pos[1]; v.z = f.pos[2];
    v.vx = pause ? 0.0f : f.vel[0] * 6.103702e-05f * 50.0f;   // per-tick units to m/s
    v.vy = pause ? 0.0f : f.vel[1] * 6.103702e-05f * 50.0f;
    v.heading = std::atan2(-t[0], t[1]);
    v.speed = pause ? 0.0f : std::sqrt(v.vx * v.vx + v.vy * v.vy);
    v.steer = f.steer * 0.05f;
}

void ScriptGame::StartPlayback(int car, int number, bool looped, bool useAi) {
    if (!cars || car < 1 || (size_t)car > cars->Count()) return;
    RecordingLoaded(number);
    int s = 0;
    while (s < 16 && m_play[s].on) s++;
    if (s == 16) s = 15;
    Playback& p = m_play[s];
    p = Playback{};
    p.on = true; p.looped = looped; p.ai = useAi; p.car = car; p.number = number;
    std::fprintf(stderr, "[CARREC] start car %d recording %d slot %d%s%s\n", car, number, s, looped ? " looped" : "", useAi ? " ai" : "");
}

void ScriptGame::StopPlayback(int car, bool skipToEnd) {
    const int s = PlaybackSlot(car);
    if (s < 0) return;
    Playback& p = m_play[s];
    VehicleInstance* v = cars && car >= 1 && (size_t)car <= cars->Count() ? &cars->Car((size_t)car - 1) : nullptr;
    const std::vector<Frame>& fr = m_recs[p.number];
    if (v && skipToEnd && !fr.empty()) RestoreFrame(*v, fr.back(), false);
    else if (v) v->vx = v->vy = v->speed = 0.0f;
    if (v) v->hasRot = false;
    p.on = false;
    std::fprintf(stderr, "[CARREC] stop car %d recording %d%s\n", car, p.number, skipToEnd ? " (skipped to end)" : "");
}

// CVehicleRecording::SaveOrRetrieveDataForThisFrame
void ScriptGame::PlaybackStep(float dt) {
    for (int s = 0; s < 16; s++) {
        Playback& p = m_play[s];
        if (!p.on) continue;
        VehicleInstance* v = cars && p.car >= 1 && (size_t)p.car <= cars->Count() ? &cars->Car((size_t)p.car - 1) : nullptr;
        if (!v || v->removed || v->wrecked) { p.on = false; if (v) v->hasRot = false; continue; }
        const std::vector<Frame>& fr = m_recs[p.number];
        if (fr.size() < 2) { StopPlayback(p.car, false); continue; }
        if (!p.paused) p.time += dt * 1000.0f * p.speed;
        size_t i = p.index;
        while (i + 1 < fr.size() && (float)fr[i + 1].time < p.time) i++;
        while (i > 0 && p.time < (float)fr[i].time) i--;
        p.index = i;
        if (i + 1 < fr.size()) {
            const Frame& a = fr[i];
            const Frame& b = fr[i + 1];
            const float k = ((float)p.time - (float)a.time) / (float)(b.time - a.time);
            RestoreFrame(*v, a, false);
            // InterpolateInfoForCar: matrix and velocity blended by k
            float r[9];
            for (int j = 0; j < 3; j++) { r[j] = b.right[j] * 0.007874016f; r[3 + j] = b.top[j] * 0.007874016f; }
            r[6] = r[1] * r[5] - r[2] * r[4]; r[7] = r[2] * r[3] - r[0] * r[5]; r[8] = r[0] * r[4] - r[1] * r[3];
            for (int j = 0; j < 9; j++) v->rot[j] = v->rot[j] * (1.0f - k) + r[j] * k;
            v->x = a.pos[0] * (1.0f - k) + b.pos[0] * k; v->y = a.pos[1] * (1.0f - k) + b.pos[1] * k; v->z = a.pos[2] * (1.0f - k) + b.pos[2] * k;
            v->vx = (a.vel[0] * (1.0f - k) + b.vel[0] * k) * 6.103702e-05f * 50.0f;
            v->vy = (a.vel[1] * (1.0f - k) + b.vel[1] * k) * 6.103702e-05f * 50.0f;
            v->heading = std::atan2(-v->rot[3], v->rot[4]);
            v->speed = std::sqrt(v->vx * v->vx + v->vy * v->vy);
            v->wheelSpin += v->speed * dt / 0.35f;
        } else if (p.looped) { p.time = 0; p.index = 0; }
        else StopPlayback(p.car, false);
    }
}

void ScriptGame::Update(float dt) {
    PlaybackStep(dt);
    MenuUpdate();
    for (const auto& f : m_frozenCars) {   // FREEZE_CAR_POSITION: held where it was frozen
        if (!cars || f.first < 1 || (size_t)f.first > cars->Count()) continue;
        VehicleInstance& v = cars->Car((size_t)f.first - 1);
        v.x = f.second[0]; v.y = f.second[1]; v.z = f.second[2]; v.speed = 0; v.vx = v.vy = 0;
    }
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
        m_cutscene.Update(dt);
        if (scm && scm->host.setPlayerPos) { const float* o = m_cutscene.Offset(); scm->host.setPlayerPos(o[0], o[1], o[2]); }   // the player waits at the scene
        if (std::getenv("GTA_CUTLOG") && (int)(m_cutscene.TimeMs() / 1000.0f) != m_cutSec) {
            m_cutSec = (int)(m_cutscene.TimeMs() / 1000.0f);
            CutsceneCamera lc;
            const bool hc = m_cutscene.Camera(lc);
            std::fprintf(stderr, "[SCMGAME] cutscene t=%d s cam %d pos %.2f %.2f %.2f look %.2f %.2f %.2f fov %.1f\n", m_cutSec, (int)hc, lc.pos[0], lc.pos[1], lc.pos[2], lc.look[0], lc.look[1], lc.look[2], lc.fov);
        }
        CutsceneCamera cc;
        if (m_cutscene.Camera(cc)) {
            cam.active = cam.cutscene = true;
            cam.moveOn = cam.trackOn = false;
            for (int i = 0; i < 3; i++) { cam.pos[i] = cc.pos[i]; cam.look[i] = cc.look[i]; cam.up[i] = cc.up[i]; }
            cam.fov = cc.fov;
            cam.nearClip = 0.0f;
        }
        const std::string key = m_cutscene.Loaded() ? m_cutscene.Subtitle() : std::string();
        if (key != m_cutKey) { m_cutKey = key; std::fprintf(stderr, "[SCMGAME] cutscene %.0f ms subtitle '%s'\n", m_cutscene.TimeMs(), key.c_str()); m_subtitle = key.empty() ? std::string() : Text(key); m_subTimer = 0; }
        // Retail Update_overlay: the skip button (or GTA_CUTSKIP=ms, dev) marks the cutscene skipped and finishes it.
        static const float devSkipMs = std::getenv("GTA_CUTSKIP") ? (float)std::atof(std::getenv("GTA_CUTSKIP")) : -1.0f;
        const bool skip = (skipPressed && skipPressed()) || (devSkipMs >= 0.0f && m_cutscene.TimeMs() >= devSkipMs);
        if (skip && !m_cut.skipped) {
            m_cut.skipped = true;
            std::fprintf(stderr, "[SCMGAME] cutscene %s skipped at %.0f ms\n", m_cut.name.c_str(), m_cutscene.TimeMs());
            m_cutscene.Skip();
        }
        if (m_cut.skipped || m_cut.t >= m_cut.dur) {
            m_cut.state = 3;
            if (cutsceneAudioStop) cutsceneAudioStop();
            if (cam.cutscene) { cam = ScriptCamera{}; }
            wideTarget = false;
            if (showPlayer) showPlayer(true);
            m_subtitle.clear();
        }
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

void ScriptGame::DrawCutscene(D3DRenderer& rr) {
    if (m_cut.state == 2 && m_cutscene.Loaded()) m_cutscene.Draw(rr, drawCj);
}

ScriptOverlay ScriptGame::Overlay() {
    ScriptOverlay o;
    o.fade = std::max(fade, enex ? enex->Fade() : 0.0f);
    o.fadeRgb = ((uint32_t)m_fadeRgb[0] << 16) | ((uint32_t)m_fadeRgb[1] << 8) | (uint32_t)m_fadeRgb[2];
    o.wide = wide;
    o.subtitle = m_subtitle;
    o.text = m_queue;
    m_queue.clear();
    o.sprites = m_spriteQueue;
    m_spriteQueue.clear();
    return o;
}
