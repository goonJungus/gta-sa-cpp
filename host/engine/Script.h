#pragma once
// Script: the compiled mission script (data/script/main.scm) and a first virtual machine
// (CTheScripts / CRunningScript). The file starts with a chain of header chunks, each one a
// GOTO to the next; the last one jumps to the main script code.
//
// The machine runs the core language: variables, arithmetic, comparisons, jumps, subroutines,
// AND/OR condition groups, thread naming and waits. Game commands (the thousands of opcodes that
// act on the world) are not wired yet: a thread stops with a log line at the first one.

#include <cstdint>
#include <string>
#include <vector>

struct ScmHeader {
    uint32_t globalVarSpace = 0;                 // bytes, includes the 8 byte chunk header
    std::vector<std::string> usedObjects;        // model names the script asks for
    uint32_t mainSize = 0, largestMission = 0;
    uint16_t missionCount = 0, exclusiveMissions = 0;
    std::vector<uint32_t> missionOffsets;
    struct Streamed { std::string name; uint32_t offset, size; };
    std::vector<Streamed> streamed;
    uint32_t mainOffset = 0;                     // where the main script code starts
};

class ScmFile {
public:
    bool Load(const std::string& path);
    const ScmHeader& Header() const { return m_hdr; }
    const std::vector<uint8_t>& Bytes() const { return m_bytes; }

private:
    std::vector<uint8_t> m_bytes;
    ScmHeader m_hdr;
};

// One running script thread.
struct ScmThread {
    uint32_t pc = 0;
    uint32_t base = 0;                   // 0 for the main script; relative jumps count from here
    std::string name = "noname";
    int32_t wakeTime = 0;                // ms; the thread sleeps until the clock reaches it
    int32_t locals[64] = {};
    uint32_t stack[8] = {};
    int sp = 0;
    bool cond = false;
    int andOrMode = 0;                   // 0 none, 1 AND, 2 OR
    int andOrLeft = 0;
    bool andOrAcc = false;
    bool finished = false;
    uint16_t stoppedAt = 0;              // opcode this thread could not run, 0 = none
    uint64_t executed = 0;
};

class ScmMachine {
public:
    bool Init(const std::string& gameDir);
    bool Ready() const { return m_ready; }
    const ScmFile& File() const { return m_file; }

    // Run every awake thread until it waits, finishes or reaches an opcode that is not implemented.
    void Update(int32_t nowMs, int maxInstructions = 200000);
    size_t ThreadCount() const { return m_threads.size(); }
    const ScmThread& Thread(size_t i) const { return m_threads[i]; }
    // Start mission script number `index` (the mission table of the header) as a new thread.
    bool LaunchMission(int index);
    int32_t Global(uint32_t offset) const;
    float GlobalFloat(uint32_t offset) const;

private:
    struct Arg { uint8_t type = 0; int32_t i = 0; float f = 0.0f; };
    bool ReadArg(ScmThread& t, Arg& a);
    int32_t& Var(ScmThread& t, const Arg& a);
    bool Step(ScmThread& t, int32_t nowMs);
    void Compare(ScmThread& t, bool result);
    void Jump(ScmThread& t, int32_t target);

    bool m_ready = false;
    ScmFile m_file;
    std::vector<uint8_t> m_space;        // the whole script image: variables are read and written in place
    std::vector<ScmThread> m_threads;
    std::vector<ScmThread> m_pending;    // threads started during a step
};
