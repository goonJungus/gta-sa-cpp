#include "Script.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

uint32_t Rd32(const std::vector<uint8_t>& b, size_t o) { uint32_t v = 0; if (o + 4 <= b.size()) std::memcpy(&v, &b[o], 4); return v; }
uint16_t Rd16(const std::vector<uint8_t>& b, size_t o) { uint16_t v = 0; if (o + 2 <= b.size()) std::memcpy(&v, &b[o], 2); return v; }

std::string Name(const std::vector<uint8_t>& b, size_t o, size_t n) {
    std::string s;
    for (size_t i = 0; i < n && o + i < b.size() && b[o + i]; i++) s += (char)b[o + i];
    return s;
}

float AsFloat(int32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
int32_t AsBits(float f) { int32_t v; std::memcpy(&v, &f, 4); return v; }

}  // namespace

bool ScmFile::Load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    m_bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (m_bytes.size() < 16) return false;
    // Walk the chunk chain: GOTO (02 00 01 <int32>), then the chunk index byte.
    size_t off = 0;
    for (int i = 0; i < 6 && off + 8 <= m_bytes.size(); i++) {
        if (m_bytes[off] != 0x02 || m_bytes[off + 1] != 0x00 || m_bytes[off + 2] != 0x01) { m_hdr.mainOffset = (uint32_t)off; break; }
        const uint32_t next = Rd32(m_bytes, off + 3);
        const uint8_t idx = m_bytes[off + 7];
        if (idx == 's') m_hdr.globalVarSpace = next;
        else if (idx == 0) {
            const uint32_t n = Rd32(m_bytes, off + 8);
            for (uint32_t k = 0; k < n && k < 4096; k++) m_hdr.usedObjects.push_back(Name(m_bytes, off + 12 + k * 24, 24));
        } else if (idx == 1) {
            m_hdr.mainSize = Rd32(m_bytes, off + 8);
            m_hdr.largestMission = Rd32(m_bytes, off + 12);
            m_hdr.missionCount = Rd16(m_bytes, off + 16);
            m_hdr.exclusiveMissions = Rd16(m_bytes, off + 18);
            for (uint32_t k = 0; k < m_hdr.missionCount; k++) m_hdr.missionOffsets.push_back(Rd32(m_bytes, off + 24 + k * 4));
        } else if (idx == 2) {
            const uint32_t n = Rd32(m_bytes, off + 12);
            for (uint32_t k = 0; k < n && k < 4096; k++) {
                const size_t e = off + 16 + k * 28;
                m_hdr.streamed.push_back({Name(m_bytes, e, 20), Rd32(m_bytes, e + 20), Rd32(m_bytes, e + 24)});
            }
        }
        off = next;
        m_hdr.mainOffset = (uint32_t)off;
    }
    return true;
}

bool ScmMachine::Init(const std::string& gameDir) {
    if (!m_file.Load(gameDir + "/data/script/main.scm")) return false;
    m_space = m_file.Bytes();
    ScmThread main;
    main.pc = m_file.Header().mainOffset;
    main.name = "main";
    m_threads.clear();
    m_threads.push_back(main);
    m_ready = true;
    const ScmHeader& h = m_file.Header();
    std::fprintf(stderr, "[SCM] %zu bytes, vars %u, %zu used models, %u missions, %zu streamed scripts, main at 0x%X\n",
                 m_space.size(), h.globalVarSpace, h.usedObjects.size(), (unsigned)h.missionCount, h.streamed.size(), h.mainOffset);
    return true;
}

int32_t ScmMachine::Global(uint32_t offset) const {
    return offset + 4 <= m_space.size() ? (int32_t)Rd32(m_space, offset) : 0;
}
float ScmMachine::GlobalFloat(uint32_t offset) const { return AsFloat(Global(offset)); }

bool ScmMachine::ReadArg(ScmThread& t, Arg& a) {
    if (t.pc >= m_space.size()) return false;
    a.type = m_space[t.pc++];
    auto need = [&](uint32_t n) { return t.pc + n <= m_space.size(); };
    switch (a.type) {
    case 1: if (!need(4)) return false; a.i = (int32_t)Rd32(m_space, t.pc); t.pc += 4; break;
    case 2: case 3: if (!need(2)) return false; a.i = Rd16(m_space, t.pc); t.pc += 2; break;
    case 4: if (!need(1)) return false; a.i = (int8_t)m_space[t.pc]; t.pc += 1; break;
    case 5: if (!need(2)) return false; a.i = (int16_t)Rd16(m_space, t.pc); t.pc += 2; break;
    case 6: if (!need(4)) return false; a.i = (int32_t)Rd32(m_space, t.pc); a.f = AsFloat(a.i); t.pc += 4; break;
    default: return false;   // strings, arrays: not handled yet
    }
    return true;
}

int32_t& ScmMachine::Var(ScmThread& t, const Arg& a) {
    static int32_t sink = 0;
    if (a.type == 2) {
        if (a.i >= 8 && (size_t)a.i + 4 <= m_space.size()) return *reinterpret_cast<int32_t*>(&m_space[(size_t)a.i]);
    } else if (a.type == 3) {
        if (a.i >= 0 && a.i < 64) return t.locals[a.i];
    }
    sink = 0;
    return sink;
}

void ScmMachine::Jump(ScmThread& t, int32_t target) {
    t.pc = target >= 0 ? (uint32_t)target : t.base + (uint32_t)(-target);
}

void ScmMachine::Compare(ScmThread& t, bool r) {
    if (t.andOrMode == 0) { t.cond = r; return; }
    t.andOrAcc = t.andOrMode == 1 ? (t.andOrAcc && r) : (t.andOrAcc || r);
    if (--t.andOrLeft <= 0) { t.cond = t.andOrAcc; t.andOrMode = 0; }
}

bool ScmMachine::Step(ScmThread& t, int32_t nowMs) {
    const uint32_t start = t.pc;
    if (t.pc + 2 > m_space.size()) { t.finished = true; return false; }
    uint16_t op = Rd16(m_space, t.pc);
    t.pc += 2;
    const bool notFlag = (op & 0x8000) != 0;   // the result of this condition is inverted
    op &= 0x7FFF;
    auto fail = [&]() { t.pc = start; t.stoppedAt = op; return false; };
    Arg a, b;
    t.executed++;

    auto readValue = [&](const Arg& x, bool isFloat, ScmThread& th) -> int32_t {
        if (x.type == 2 || x.type == 3) return Var(th, x);
        return isFloat ? AsBits(x.type == 6 ? x.f : (float)x.i) : x.i;
    };
    auto cmpOp = [&](bool isFloat, int kind) {   // kind: 0 >, 1 >=, 2 ==, 3 !=
        const int32_t x = readValue(a, isFloat, t), y = readValue(b, isFloat, t);
        bool r;
        if (isFloat) {
            const float fx = AsFloat(x), fy = AsFloat(y);
            r = kind == 0 ? fx > fy : kind == 1 ? fx >= fy : kind == 2 ? fx == fy : fx != fy;
        } else {
            r = kind == 0 ? x > y : kind == 1 ? x >= y : kind == 2 ? x == y : x != y;
        }
        Compare(t, notFlag ? !r : r);
    };

    if (op == 0x0000) return true;                                              // NOP
    if (op == 0x0001) {                                                         // WAIT
        if (!ReadArg(t, a)) return fail();
        t.wakeTime = nowMs + a.i;
        return true;
    }
    if (op == 0x0002 || op == 0x004C || op == 0x004D || op == 0x0050) {         // GOTO, GOTO_IF_TRUE/FALSE, GOSUB
        if (!ReadArg(t, a)) return fail();
        const bool take = op == 0x0002 || op == 0x0050 || (op == 0x004C ? t.cond : !t.cond);
        if (op == 0x0050 && take && t.sp < 8) t.stack[t.sp++] = t.pc;
        if (take) Jump(t, a.i);
        return true;
    }
    if (op == 0x0051) {                                                         // RETURN
        if (t.sp == 0) { t.finished = true; return false; }
        t.pc = t.stack[--t.sp];
        return true;
    }
    if (op == 0x00C5 || op == 0x00C6) {                                         // RETURN_TRUE / RETURN_FALSE
        t.cond = op == 0x00C5;
        if (t.sp == 0) { t.finished = true; return false; }
        t.pc = t.stack[--t.sp];
        return true;
    }
    if (op == 0x004E) { t.finished = true; return false; }                     // TERMINATE_THIS_SCRIPT
    if (op == 0x00D6) {                                                         // ANDOR
        if (!ReadArg(t, a)) return fail();
        if (a.i == 0) t.andOrMode = 0;
        else if (a.i >= 1 && a.i <= 7) { t.andOrMode = 1; t.andOrLeft = a.i + 1; t.andOrAcc = true; }
        else if (a.i >= 21 && a.i <= 27) { t.andOrMode = 2; t.andOrLeft = a.i - 20 + 1; t.andOrAcc = false; }
        return true;
    }
    if (op == 0x03A4) {                                                         // SCRIPT_NAME: one 8 byte string
        if (t.pc + 9 > m_space.size() || m_space[t.pc] != 9) return fail();
        t.name = Name(m_space, t.pc + 1, 8);
        t.pc += 9;
        return true;
    }
    // Two argument families: assignment, arithmetic and comparison.
    const bool twoArgs = (op >= 0x0004 && op <= 0x0017) || (op >= 0x0018 && op <= 0x004B) || (op >= 0x0058 && op <= 0x0077) ||
                         (op >= 0x0084 && op <= 0x0093);
    if (twoArgs) {
        if (!ReadArg(t, a) || !ReadArg(t, b)) return fail();
        if (op >= 0x0018 && op <= 0x004B) {   // comparisons
            if (op <= 0x0027) cmpOp(op >= 0x0020, 0);
            else if (op <= 0x0037) cmpOp(op >= 0x0030, 1);
            else if (op <= 0x003C) cmpOp(false, 2);
            else if (op <= 0x0041) cmpOp(false, 3);
            else if (op <= 0x0046) cmpOp(true, 2);
            else cmpOp(true, 3);
            return true;
        }
        int group, kindIsFloat;
        // group: 0 set, 1 add, 2 sub, 3 mul, 4 div, 5 convert-set
        if (op <= 0x0007) { group = 0; kindIsFloat = (op - 4) & 1; }
        else if (op <= 0x0017) { group = 1 + (op - 8) / 4; kindIsFloat = (op - 8) & 1; }
        else if (op <= 0x0077) { group = 1 + (op - 0x58) / 8; kindIsFloat = (op - 0x58) & 1; }
        else if (op <= 0x008B) { group = 0; kindIsFloat = (op == 0x86 || op == 0x87 || op == 0x88 || op == 0x89); }
        else { group = 5; kindIsFloat = (op & 1) == 1; }   // 8D, 8F, 91, 93 write a float; 8C, 8E, 90, 92 an int
        if (op >= 0x0078 && op <= 0x0083) return fail();   // timed arithmetic: needs the frame time
        if (a.type != 2 && a.type != 3) return fail();
        int32_t& dst = Var(t, a);
        if (group == 5) {   // CSET: destination type given by the opcode, source the other one
            if (kindIsFloat) dst = AsBits((float)readValue(b, false, t));
            else dst = (int32_t)AsFloat(readValue(b, true, t));
            return true;
        }
        const int32_t src = readValue(b, kindIsFloat != 0, t);
        if (kindIsFloat) {
            float d = AsFloat(dst);
            const float s = AsFloat(src);
            switch (group) { case 0: d = s; break; case 1: d += s; break; case 2: d -= s; break; case 3: d *= s; break; default: d = s != 0.0f ? d / s : d; }
            dst = AsBits(d);
        } else {
            switch (group) { case 0: dst = src; break; case 1: dst += src; break; case 2: dst -= src; break; case 3: dst *= src; break; default: dst = src ? dst / src : dst; }
        }
        return true;
    }
    return fail();   // a game command
}

void ScmMachine::Update(int32_t nowMs, int maxInstructions) {
    for (ScmThread& t : m_threads) {
        if (t.finished || t.stoppedAt || t.wakeTime > nowMs) continue;
        for (int n = 0; n < maxInstructions && !t.finished && !t.stoppedAt && t.wakeTime <= nowMs; n++)
            if (!Step(t, nowMs)) break;
    }
}
