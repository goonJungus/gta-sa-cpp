#include "Script.h"
#include <algorithm>
#include <map>

#include <cstdlib>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>

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

struct CmdInfo { uint16_t op; uint8_t args, outs, cond; };

// Argument counts of the script commands, settled by scripts/scm_args.py against the compiled script.
const CmdInfo kCmds[] = {
#include "ScmArgs.inc"

};

// Opcodes the compiled script uses in condition slots (scripts/scm_cond.py); the argument table alone misses many.
bool IsCondOp(uint16_t op) {
    static const std::set<uint16_t> s_conds = {
#include "ScmCond.inc"
    };
    return s_conds.count(op) != 0;
}

const char* CmdName(uint16_t op) {   // dev: the trace prints command names
    static const std::map<uint16_t, const char*> names = {
#include "ScmNames.inc"
    };
    auto it = names.find(op);
    return it == names.end() ? "?" : it->second;
}

const CmdInfo* FindCmd(uint16_t op) {
    size_t lo = 0, hi = sizeof(kCmds) / sizeof(kCmds[0]);
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (kCmds[mid].op < op) lo = mid + 1; else hi = mid;
    }
    return lo < sizeof(kCmds) / sizeof(kCmds[0]) && kCmds[lo].op == op ? &kCmds[lo] : nullptr;
}


// Byte length of the argument at p (type byte included), 0 when it is not an argument.
size_t ArgSpan(const std::vector<uint8_t>& b, size_t p) {
    if (p >= b.size()) return 0;
    switch (b[p]) {
    case 1: case 6: return p + 5 <= b.size() ? 5 : 0;
    case 2: case 3: case 5: case 0x0A: case 0x0B: case 0x10: case 0x11: return p + 3 <= b.size() ? 3 : 0;
    case 4: return p + 2 <= b.size() ? 2 : 0;
    case 7: case 8: case 0x0C: case 0x0D: case 0x12: case 0x13: return p + 7 <= b.size() ? 7 : 0;
    case 9: return p + 9 <= b.size() ? 9 : 0;
    case 0x0E: return p + 2 <= b.size() && p + 2 + b[p + 1] <= b.size() ? 2 + (size_t)b[p + 1] : 0;
    case 0x0F: return p + 17 <= b.size() ? 17 : 0;
    }
    return 0;
}

// Does a run of `depth` commands parse from p? Commands the table lacks get a count tried from 0 up (one count per
// opcode along the path, `assumed`), within a node budget.
bool ParseRun(const std::vector<uint8_t>& b, size_t p, int depth, std::map<uint16_t, int>& assumed, int& budget) {
    if (depth == 0 || p + 2 > b.size()) return true;
    if (--budget < 0) return false;
    const uint16_t op = (uint16_t)((b[p] | (b[p + 1] << 8)) & 0x7FFF);
    if (op > 0x0AF0) return false;
    p += 2;
    const CmdInfo* ci = FindCmd(op);
    if (op == 0x004F || op == 0x0913) {
        while (p < b.size() && b[p] != 0) { const size_t l = ArgSpan(b, p); if (!l) return false; p += l; }
        return ParseRun(b, p + 1, depth - 1, assumed, budget);
    }
    auto after = [&](int n, size_t& q) {
        q = p;
        for (int i = 0; i < n; i++) { const size_t l = ArgSpan(b, q); if (!l) return false; q += l; }
        return true;
    };
    size_t q;
    if (ci) return after(ci->args, q) && ParseRun(b, q, depth - 1, assumed, budget);
    if (CmdName(op)[0] == '?') return false;   // not a command at all
    auto it = assumed.find(op);
    if (it != assumed.end()) return after(it->second, q) && ParseRun(b, q, depth - 1, assumed, budget);
    for (int n = 0; n <= 12; n++) {
        if (!after(n, q)) break;
        assumed[op] = n;
        if (ParseRun(b, q, depth - 1, assumed, budget)) return true;
        assumed.erase(op);
    }
    return false;
}

// Commands the compiled-script table lacks (mission scripts use many main never does): the argument count is
// the one after which the following commands parse with the fewest further unknown commands (a short literal
// such as 04 04 would otherwise read as a command of its own), found once and kept.
int InferArgs(const std::vector<uint8_t>& b, size_t afterOp, uint16_t op) {
    // Counts checked by hand against the script bytes, where the inference alone picks a wrong one.
    static std::map<uint16_t, int> s_cache = {
        {0x0605, 9}, {0x05CA, 4}, {0x05CB, 3}, {0x0245, 2}, {0x0187, 2}, {0x09D2, 1},
        {0x070C, 1}, {0x041E, 1}, {0x07A1, 1}, {0x0713, 10}};
    auto it = s_cache.find(op);
    if (it != s_cache.end()) return it->second;
    int best = -1;
    size_t bestSize = 0, p = afterOp;
    for (int n = 0; n <= 20; n++) {
        std::map<uint16_t, int> assumed;
        assumed[op] = n;
        int budget = 40000;
        if (ParseRun(b, p, 20, assumed, budget) && (best < 0 || assumed.size() < bestSize)) { best = n; bestSize = assumed.size(); }
        const size_t l = ArgSpan(b, p);
        if (!l) break;
        p += l;
    }
    if (static const bool dbg = std::getenv("GTA_SCMGAME") != nullptr; dbg) std::fprintf(stderr, "[SCM] inferred %d args for 0x%04X %s\n", best, op, CmdName(op));
    s_cache[op] = best;
    return best;
}

// A command not in the condition list that is named like a test.
bool NamedCond(uint16_t op) {
    const char* n = CmdName(op);
    static const char* pre[] = {"IS_", "HAS_", "CAN_", "LOCATE_", "DOES_", "ARE_", "WAS_", "SHOULD_"};
    for (const char* q : pre) if (!std::strncmp(n, q, std::strlen(q))) return true;
    return false;
}

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
    if (!std::getenv("GTA_SCMNOMAIN")) m_threads.push_back(main);   // dev: run only the GTA_SCMSNIP thread
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
    a.text = false;
    auto need = [&](uint32_t n) { return t.pc + n <= m_space.size(); };
    switch (a.type) {
    case 1: if (!need(4)) return false; a.i = (int32_t)Rd32(m_space, t.pc); t.pc += 4; break;
    case 2: case 3: if (!need(2)) return false; a.i = Rd16(m_space, t.pc); t.pc += 2; break;
    case 4: if (!need(1)) return false; a.i = (int8_t)m_space[t.pc]; t.pc += 1; break;
    case 5: if (!need(2)) return false; a.i = (int16_t)Rd16(m_space, t.pc); t.pc += 2; break;
    case 6: if (!need(4)) return false; a.i = (int32_t)Rd32(m_space, t.pc); a.f = AsFloat(a.i); t.pc += 4; break;
    case 7: case 8: case 0x0C: case 0x0D: case 0x12: case 0x13: {   // array element: base, index variable, size, flags
        if (!need(6)) return false;
        const uint32_t base = Rd16(m_space, t.pc), iv = Rd16(m_space, t.pc + 2);
        const uint8_t size = m_space[t.pc + 4], flags = m_space[t.pc + 5];
        t.pc += 6;
        int32_t idx = 0;
        if (flags & 0x80) idx = iv < ScmThread::kLocals ? t.locals[iv] : 0;
        else if ((size_t)iv + 4 <= m_space.size()) idx = (int32_t)Rd32(m_space, iv);
        if (idx < 0 || idx >= size) idx = 0;
        const bool local = a.type == 8 || a.type == 0x0D || a.type == 0x13;
        const uint32_t stride = (a.type == 7 || a.type == 8) ? 1 : (a.type == 0x12 || a.type == 0x13) ? 4 : 2;
        a.text = true;
        a.type = local ? 3 : 2;
        a.i = (int32_t)(local ? base + (uint32_t)idx * stride : base + (uint32_t)idx * stride * 4);
        if (stride == 1) a.text = false;
        break;
    }
    case 9: if (!need(8)) return false; a.text = true; a.i = (int32_t)(t.pc); t.pc += 8; break;
    case 0x0A: case 0x0B: case 0x10: case 0x11:   // a text label variable: global (byte offset) or local (index)
        if (!need(2)) return false;
        a.text = true;
        a.i = Rd16(m_space, t.pc);
        a.type = (a.type == 0x0A || a.type == 0x10) ? 2 : 3;
        t.pc += 2;
        break;
    case 0x0E: {
        if (!need(1)) return false;
        const uint32_t n = m_space[t.pc];
        if (!need(1 + n)) return false;
        a.text = true;
        a.i = (int32_t)(t.pc + 1);
        t.pc += 1 + n;
        break;
    }
    case 0x0F: if (!need(16)) return false; a.text = true; a.i = (int32_t)(t.pc); t.pc += 16; break;
    default: return false;
    }
    return true;
}

int32_t& ScmMachine::Var(ScmThread& t, const Arg& a) {
    static int32_t sink = 0;
    if (a.type == 2) {
        if (a.i >= 8 && (size_t)a.i + 4 <= m_space.size()) return *reinterpret_cast<int32_t*>(&m_space[(size_t)a.i]);
    } else if (a.type == 3) {
        if (a.i >= 0 && a.i < ScmThread::kLocals) return t.locals[a.i];
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


    static const char* s_pc = std::getenv("GTA_SCMPC");
    static const uint32_t s_pcLo = s_pc ? (uint32_t)std::strtoul(s_pc, nullptr, 16) : 0;
    if (s_pc && t.name == "MAIN" && start >= s_pcLo && start < s_pcLo + 0x80)   // dev: MAIN instructions in a 0x80-byte window
        std::fprintf(stderr, "[SCMPC] %06X %04X %s\n", start, op, CmdName(op));
    static const char* s_ta = std::getenv("GTA_SCMTRACEALL");
    static const uint64_t s_from = std::getenv("GTA_SCMOPFROM") ? (uint64_t)std::atoll(std::getenv("GTA_SCMOPFROM")) : 0;
    if (s_ta && t.name == s_ta && t.executed >= s_from && t.executed < s_from + 3000)   // dev: a 3000-instruction window of one thread
        std::fprintf(stderr, "[SCMOP] %s %06X %04X %s\n", t.name.c_str(), start, op, CmdName(op));

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
    if (op == 0x0871 || op == 0x0872) {                                         // SWITCH_START / SWITCH_CONTINUED
        std::vector<Arg> sa(18);
        for (Arg& x : sa) if (!ReadArg(t, x)) return fail();
        if (op == 0x0872) return true;   // its cases were read by the START that precedes it
        const int32_t val = readValue(sa[0], false, t);
        const int count = readValue(sa[1], false, t);
        int32_t target = sa[3].i;   // the default
        int seen = 0;
        bool hit = false;
        auto tryPair = [&](const Arg& cv, const Arg& jv) {
            if (hit || seen++ >= count) return;
            if (readValue(cv, false, t) == val) { target = jv.i; hit = true; }
        };
        for (int i = 4; i + 1 < 18; i += 2) tryPair(sa[i], sa[i + 1]);
        uint32_t look = t.pc;   // the continuation instructions hold the remaining cases
        while (seen < count && look + 2 <= m_space.size() && (Rd16(m_space, look) & 0x7FFF) == 0x0872) {
            ScmThread tmp = t;
            tmp.pc = look + 2;
            for (int i = 0; i < 18; i++) {
                Arg x;
                if (!ReadArg(tmp, x)) break;
                sa[i % 18] = x;
                if (i % 2) tryPair(sa[i - 1], sa[i]);
            }
            look = tmp.pc;
        }
        Jump(t, target);
        return true;
    }
    if (op == 0x004E) { t.finished = true; return false; }                     // TERMINATE_THIS_SCRIPT
    if (op == 0x004F || op == 0x00D7) {                                         // START_NEW_SCRIPT, LAUNCH_MISSION
        if (!ReadArg(t, a)) return fail();
        ScmThread n;
        n.pc = a.i >= 0 ? (uint32_t)a.i : 0;
        if (op == 0x00D7) {   // a mission script lives in its own block; its jumps count from the block start
            if (a.i < 0 || (size_t)a.i >= m_space.size()) return fail();
            n.base = (uint32_t)a.i;
            n.name = "mission";
        } else if (a.i < 0) return fail();
        if (op == 0x004F) {   // the extra arguments become the new thread's first locals
            int li = 0;
            while (t.pc < m_space.size() && m_space[t.pc] != 0) {
                Arg v;
                if (!ReadArg(t, v)) return fail();
                if (li < 32) n.locals[li++] = (v.type == 2 || v.type == 3) ? Var(t, v) : v.i;
            }
            if (t.pc < m_space.size()) t.pc++;   // the end-of-arguments byte
        }
        n.wakeTime = nowMs;
        if (std::getenv("GTA_SCMGAME")) std::fprintf(stderr, "[SCM] %s starts %s at 0x%X (op %04X)\n", t.name.c_str(), op == 0x00D7 ? "mission" : "script", n.pc, op);
        m_pending.push_back(n);
        return true;
    }
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
    // A game command: the argument count comes from the table. Commands the host implements run;
    // the rest are skipped (their outputs read 0, their condition fails) and listed once in the log.
    const CmdInfo* ci = FindCmd(op);
    int inferred = -1;
    if (!ci && op != 0x0913) {
        inferred = InferArgs(m_space, t.pc, op);
        if (inferred < 0) return fail();
    }
    std::vector<Arg> args;
    if (op == 0x0913) {   // START_NEW_STREAMED_SCRIPT: arguments up to a zero byte
        while (t.pc < m_space.size() && m_space[t.pc] != 0) {
            Arg v;
            if (!ReadArg(t, v)) return fail();
            args.push_back(v);
        }
        if (t.pc < m_space.size()) t.pc++;
    } else {
        for (int i = 0; i < (ci ? (int)ci->args : inferred); i++) {
            Arg v;
            if (!ReadArg(t, v)) return fail();
            args.push_back(v);
        }
    }
    bool result = false;
    if (static const char* tr = std::getenv("GTA_SCMTRACE"); tr && (t.name == tr || !std::strcmp(tr, "*"))) {
        static int s_lines = 0;
        static const int s_max = std::getenv("GTA_SCMTRACEMAX") ? std::atoi(std::getenv("GTA_SCMTRACEMAX")) : 3000;
        static const int s_skip = std::getenv("GTA_SCMTRACESKIP") ? std::atoi(std::getenv("GTA_SCMTRACESKIP")) : 0;
        static int s_seen = 0;
        static const uint32_t s_after = std::getenv("GTA_SCMTRACEAFTER") ? (uint32_t)std::strtoul(std::getenv("GTA_SCMTRACEAFTER"), nullptr, 16) : 0;
        static bool s_armed = s_after == 0;
        if (op == s_after) s_armed = true;               // dev: start tracing once this command has run
        if (s_armed && s_seen++ >= s_skip && s_lines++ < s_max) {   // dev: skip the first N traced commands
            std::string s;
            char b[64];
            for (const Arg& x : args) {
                if (x.text && x.type != 2 && x.type != 3) { std::snprintf(b, sizeof b, " '%.8s'", x.i >= 0 && (size_t)x.i + 8 <= m_space.size() ? (const char*)&m_space[x.i] : "?"); }
                else if (x.type == 2 || x.type == 3) std::snprintf(b, sizeof b, " v%d=%d", x.i, Var(t, x));
                else if (x.type == 6) std::snprintf(b, sizeof b, " %.2f", x.f);
                else std::snprintf(b, sizeof b, " %d", x.i);
                s += b;
            }
            std::fprintf(stderr, "[SCMTRACE] %s %06X %s%s%s\n", t.name.c_str(), start, notFlag ? "NOT " : "", CmdName(op), s.c_str());
        }
    }
    if (!Command(t, op, args, result)) {
        if (m_logged.insert(op).second) std::fprintf(stderr, "[SCM] %s: command 0x%04X %s not implemented (%zu args)\n", t.name.c_str(), op, CmdName(op), args.size());
        if (ci)
            for (size_t i = args.size() - ci->outs; i < args.size() && ci->outs; i++)
                if (args[i].type == 2 || args[i].type == 3) Var(t, args[i]) = 0;
        result = false;
    }
    if ((ci && ci->cond) || IsCondOp(op) || (!ci && NamedCond(op))) Compare(t, notFlag ? !result : result);
    return true;
}

int32_t ScmMachine::Call::I(size_t i) const {
    if (i >= args.size()) return 0;
    const Arg& x = args[i];
    return (x.type == 2 || x.type == 3) ? m.Var(t, x) : (x.type == 6 ? (int32_t)x.f : x.i);
}
float ScmMachine::Call::F(size_t i) const {
    if (i >= args.size()) return 0.0f;
    const Arg& x = args[i];
    if (x.type == 2 || x.type == 3) return AsFloat(m.Var(t, x));
    return x.type == 6 ? x.f : (float)x.i;
}
const char* ScmCommandName(uint16_t op) { return CmdName(op); }

std::string ScmMachine::Call::S(size_t i, size_t cap) const {
    if (i >= args.size()) return "";
    const Arg& x = args[i];
    const char* p = nullptr;
    if (x.type == 2 || x.type == 3) p = (const char*)&m.Var(t, x);
    else if (x.text) { if (x.i >= 0 && (size_t)x.i + cap <= m.m_space.size()) p = (const char*)&m.m_space[(size_t)x.i]; }
    return p ? std::string(p, strnlen(p, cap)) : "";
}
void ScmMachine::Call::Out(size_t i, int32_t v) const {
    if (i < args.size() && (args[i].type == 2 || args[i].type == 3)) m.Var(t, args[i]) = v;
    static const bool s_log = std::getenv("GTA_SCMOUT") != nullptr;   // dev: every command output, for the headless tests
    if (s_log) std::fprintf(stderr, "[SCMOUT] %04X out%zu int %d float %g\n", op, i, v, AsFloat(v));
}
void ScmMachine::Call::OutF(size_t i, float v) const { Out(i, AsBits(v)); }
void ScmMachine::Call::OutStr(size_t i, const std::string& s, size_t cap) const {
    if (i >= args.size() || (args[i].type != 2 && args[i].type != 3)) return;
    char* p = (char*)&m.Var(t, args[i]);
    // a local text variable is a run of consecutive locals; a global one lies in the script image
    if (args[i].type == 3 && (size_t)args[i].i + (cap + 3) / 4 > (size_t)ScmThread::kLocals) return;
    std::memset(p, 0, cap);
    std::memcpy(p, s.data(), std::min(s.size(), cap));
}

std::vector<std::string> ScmMachine::Census() {
    std::vector<std::string> missing;
    ScmThread t;
    t.name = "CENSUS";
    for (const CmdInfo& ci : kCmds) {
        if (ci.op <= 0x00D7) continue;
        std::vector<Arg> args(ci.args);
        for (Arg& x : args) { x.type = 1; x.i = 1; }
        bool result = false;
        if (!Command(t, ci.op, args, result)) { char b[8]; std::snprintf(b, sizeof b, "%04X ", ci.op); missing.push_back(std::string(b) + CmdName(ci.op)); }
    }
    return missing;
}

bool ScmMachine::Command(ScmThread& t, uint16_t op, std::vector<Arg>& args, bool& result) {
    auto num = [&](size_t i) -> float {
        if (i >= args.size()) return 0.0f;
        const Arg& x = args[i];
        if (x.type == 2 || x.type == 3) return AsFloat(Var(t, x));
        return x.type == 6 ? x.f : (float)x.i;
    };
    auto inum = [&](size_t i) -> int32_t {
        if (i >= args.size()) return 0;
        const Arg& x = args[i];
        return (x.type == 2 || x.type == 3) ? Var(t, x) : x.i;
    };
    auto out = [&](size_t i, int32_t v) { if (i < args.size() && (args[i].type == 2 || args[i].type == 3)) Var(t, args[i]) = v; };
    auto outF = [&](size_t i, float v) { out(i, AsBits(v)); };
    // an 8-byte text argument (a name in the script image)
    auto str = [&](size_t i) -> std::string {
        if (i >= args.size() || !args[i].text || args[i].i < 0 || (size_t)args[i].i + 8 > m_space.size()) return "";
        return std::string((const char*)&m_space[args[i].i], strnlen((const char*)&m_space[args[i].i], 8));
    };
    if (command) {
        Call call{*this, t, op, args};
        call.notFlag = false;
        if (command(call)) {
            result = call.result;
            static const bool s_logRes = std::getenv("GTA_SCMOUT") != nullptr;   // dev: condition results for the headless tests
            if (s_logRes) std::fprintf(stderr, "[SCMRES] %04X -> %d\n", op, (int)result);
            return true;
        }
    }
    switch (op) {
    case 0x0784: case 0x087B:   // GIVE_PLAYER_CLOTHES / _OUTSIDE_SHOP(player, texture, model, texture part)
        if (host.giveClothes) host.giveClothes(str(1).c_str(), str(2).c_str(), inum(3));
        return true;
    case 0x0500:                // IS_PLAYER_WEARING(player, texture part, texture)
        result = host.wearing && host.wearing(inum(1), str(2).c_str());
        return true;
    case 0x070D: return true;   // BUILD_PLAYER_MODEL: the outfit is rebuilt as soon as it is given
    case 0x0793: if (host.storeClothes) host.storeClothes(); return true;     // STORE_CLOTHES_STATE
    case 0x0794: if (host.restoreClothes) host.restoreClothes(); return true; // RESTORE_CLOTHES_STATE
    case 0x0053:   // CREATE_PLAYER(id, x, y, z) -> player handle
        if (host.setPlayerPos) host.setPlayerPos(num(1), num(2), num(3));
        out(4, 0);
        return true;
    case 0x01F5: out(1, 1); return true;   // GET_PLAYER_CHAR: the player's ped is handle 1
    case 0x0054: case 0x00A0: {            // GET_PLAYER_COORDINATES / GET_CHAR_COORDINATES
        float x = 0, y = 0, z = 0;
        if (host.playerPos) host.playerPos(x, y, z);
        outF(1, x); outF(2, y); outF(3, z);
        return true;
    }
    case 0x00A1:   // SET_CHAR_COORDINATES(char, x, y, z)
        if (inum(0) == 1 && host.setPlayerPos) host.setPlayerPos(num(1), num(2), num(3));
        return true;
    case 0x018A:   // ADD_BLIP_FOR_COORD(x, y, z) -> blip
        out(3, host.addBlip ? host.addBlip(num(0), num(1), num(2), 0, false) : 0);
        return true;
    case 0x02A7:   // ADD_SPRITE_BLIP_FOR_CONTACT_POINT(x, y, z, sprite) -> blip
    case 0x02A8:   // ADD_SPRITE_BLIP_FOR_COORD(x, y, z, sprite) -> blip
    case 0x04CE:   // ADD_SHORT_RANGE_SPRITE_BLIP_FOR_CONTACT_POINT(x, y, z, sprite) -> blip
    case 0x0570:   // ADD_SHORT_RANGE_SPRITE_BLIP_FOR_COORD(x, y, z, sprite) -> blip
        out(4, host.addBlip ? host.addBlip(num(0), num(1), num(2), inum(3), op == 0x04CE || op == 0x0570) : 0);
        return true;
    case 0x0164:   // REMOVE_BLIP(blip)
        if (host.removeBlip) host.removeBlip(inum(0));
        return true;
    case 0x018B:   // CHANGE_BLIP_DISPLAY(blip, mode)
        if (host.blipDisplay) host.blipDisplay(inum(0), inum(1));
        return true;
    case 0x0256:   // IS_PLAYER_PLAYING(player): the player is always alive and in control here
        result = true;
        return true;
    case 0x0417: {   // LOAD_AND_LAUNCH_MISSION_INTERNAL(index): the mission script becomes a thread of its own
        const ScmHeader& h = m_file.Header();
        const int idx = inum(0);
        if (idx < 0 || idx >= (int)h.missionOffsets.size()) return true;
        ScmThread n;
        n.pc = n.base = h.missionOffsets[(size_t)idx];
        n.name = "mission";
        if (std::getenv("GTA_SCMGAME")) std::fprintf(stderr, "[SCM] %s loads mission %d at 0x%X\n", t.name.c_str(), idx, n.pc);
        m_pending.push_back(n);
        return true;
    }
    case 0x00C0:   // SET_TIME_OF_DAY(hour, minute)
        if (host.setTime) host.setTime(inum(0), inum(1));
        return true;
    default: break;
    }
    return false;
}

void ScmMachine::Update(int32_t nowMs, int maxInstructions) {
    const int32_t dt = m_lastMs ? nowMs - m_lastMs : 0;   // the two timer locals of every thread count up between updates
    m_lastMs = nowMs;
    if (dt > 0) for (ScmThread& t : m_threads) { t.locals[32] += dt; t.locals[33] += dt; }
    for (size_t i = 0; i < m_threads.size(); i++) {   // new threads started on the way join the same pass
        ScmThread& t = m_threads[i];
        if (!t.finished && !t.stoppedAt && t.wakeTime <= nowMs)
            for (int n = 0; n < maxInstructions && !t.finished && !t.stoppedAt && t.wakeTime <= nowMs; n++) {
                if (!Step(t, nowMs)) break;
                // a loop that never waits (its game command is skipped) must not eat the frame
                if (n == maxInstructions - 1 && !t.finished) t.wakeTime = nowMs + 100;
            }
        if (!m_pending.empty()) {
            for (ScmThread& p : m_pending) m_threads.push_back(p);
            m_pending.clear();
        }
    }
}

bool ScmMachine::LaunchSnippet(const std::vector<uint8_t>& code) {
    ScmThread n;
    n.pc = n.base = (uint32_t)m_space.size();
    n.name = "snippet";
    m_space.insert(m_space.end(), code.begin(), code.end());
    m_threads.push_back(n);
    return true;
}

int ScmMachine::LaunchMissionThread(int index, const char* name) {
    const ScmHeader& h = m_file.Header();
    if (index < 0 || index >= (int)h.missionOffsets.size()) return -1;
    ScmThread n;
    n.pc = n.base = h.missionOffsets[(size_t)index];
    n.name = name;
    n.wakeTime = m_lastMs;
    m_pending.push_back(n);
    return (int)(m_threads.size() + m_pending.size() - 1);
}

bool ScmMachine::LaunchMission(int index) {
    const ScmHeader& h = m_file.Header();
    if (index < 0 || index >= (int)h.missionOffsets.size()) return false;
    ScmThread n;
    n.pc = n.base = h.missionOffsets[(size_t)index];
    n.name = "mission";
    m_threads.push_back(n);
    return true;
}
