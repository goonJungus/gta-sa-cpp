// CRunningScript.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CRunningScript (the SCM script VM thread).
// Adapted from gta-reversed for clean-room C++ build.
//
// Decomp reference: <decomp>/src/CRunningScript/*.c
//   (Ghidra 12.1.4 headless analysis of gta_sa.exe, 2026-10-08).
// Ghidra-isms converted throughout:
//   - SUB21(x>>0xf,0)      -> ((x >> 15) & 1)
//   - SEXT14(byte)         -> sign-extended byte (int8_t)
//   - CONCAT22/CONCAT31    -> explicit shifts/ors
//   - __thiscall param_1   -> this
//   - SEH boilerplate (ExceptionList/pcStack_8/uStack_4) -> dropped; the
//     guarded logic is preserved, the MSVC exception frames are not needed
//     in the clean-room build.
//   - byte-offset struct pokes (param_1+0x3c etc.) -> named members.
//
// Opcode dispatch: s_OriginalCommandHandlerTable[command / 100] selects one
// of 27 ProcessCommands*To* member functions, each handling 100 opcodes.
// Only ProcessCommands0To99 is implemented (the core VM opcodes: control
// flow, WAIT, arithmetic, comparisons, GOSUB/RETURN, script control).
// The remaining 26 blocks are TODO stubs (see bottom of file).

#include "CRunningScript.h"

#include "CHud.h"    // CHud::m_BigMessage (cutscene skip)
#include "CPad.h"    // CPad::GetPad (GetPadState)
#include "CPed.h"    // CPed::m_nPedState (IsPedDead)
#include "CTimer.h"  // CTimer::m_snTimeInMilliseconds (opcode 1: WAIT)
#include "ePedState.h"

#include <cmath>     // std::isnan (float comparison opcodes)
#include <exception> // std::terminate (CustomCommandHandlerOf TODO stub)

// CCamera.h is not included: it conflicts with CTheScripts.h (eModelID
// scoped/unscoped mismatch - pre-existing header issue, TODO). The two
// CCamera.h included 2026-10-09 (its eModelID scoped/unscoped conflict with
// CTheScripts.h is resolved via the canonical eModelID.h); provides
// CamShakeNoPos + `extern CCamera TheCamera`.
#include "CCamera.h"

// ---- Forward declarations for not-yet-ported subsystems ----
// TODO: replace with the real headers when those subsystems are ported.

// Cutscene manager (opcode-adjacent: Process() cutscene-skip path).
// TODO: port with the cutscene subsystem.
class CCutsceneMgr {
public:
    static bool IsCutsceneSkipButtonBeingPressed();
};

// Player subsystem bits needed by DoDeathArrestCheck().
// TODO: port with the player subsystem (CPlayerInfo, CWorld::Players).
// (Kept as a documented dependency; DoDeathArrestCheck is a TODO stub
// until the player subsystem lands.)

// ---- Static member definitions (original 1.0 US addresses in comments) ----
// TODO: re-resolve for the clean-room build.
std::array<tScriptParam, 32> CRunningScript::ScriptParams{}; // 0xA43C78
std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers{}; // NOTSA
uint8_t CRunningScript::ScriptArgCharNextFreeBuffer{}; // NOTSA

// The 27-entry opcode dispatch table. Index = commandId / 100.
// See the ProcessCommands*To* method definitions at the bottom of this file.
CRunningScript::CommandHandlerTable_t CRunningScript::s_OriginalCommandHandlerTable{ // 0x8A6168
    &CRunningScript::ProcessCommands0To99,
    &CRunningScript::ProcessCommands100To199,
    &CRunningScript::ProcessCommands200To299,
    &CRunningScript::ProcessCommands300To399,
    &CRunningScript::ProcessCommands400To499,
    &CRunningScript::ProcessCommands500To599,
    &CRunningScript::ProcessCommands600To699,
    &CRunningScript::ProcessCommands700To799,
    &CRunningScript::ProcessCommands800To899,
    &CRunningScript::ProcessCommands900To999,
    &CRunningScript::ProcessCommands1000To1099,
    &CRunningScript::ProcessCommands1100To1199,
    &CRunningScript::ProcessCommands1200To1299,
    &CRunningScript::ProcessCommands1300To1399,
    &CRunningScript::ProcessCommands1400To1499,
    &CRunningScript::ProcessCommands1500To1599,
    &CRunningScript::ProcessCommands1600To1699,
    &CRunningScript::ProcessCommands1700To1799,
    &CRunningScript::ProcessCommands1800To1899,
    &CRunningScript::ProcessCommands1900To1999,
    &CRunningScript::ProcessCommands2000To2099,
    &CRunningScript::ProcessCommands2100To2199,
    &CRunningScript::ProcessCommands2200To2299,
    &CRunningScript::ProcessCommands2300To2399,
    &CRunningScript::ProcessCommands2400To2499,
    &CRunningScript::ProcessCommands2500To2599,
    &CRunningScript::ProcessCommands2600To2699,
};

// ---------------------------------------------------------------------------
// Core VM
// ---------------------------------------------------------------------------

// @ 015626b0 [.HOODLUM] (original entry stub at 004648e0)
void CRunningScript::Init() {
    m_pNext = nullptr;
    m_pPrev = nullptr;
    // Default script name is "noname" (decomp writes 'n','o','n','a',0x656d,0x00).
    m_szName[0] = 'n';
    m_szName[1] = 'o';
    m_szName[2] = 'n';
    m_szName[3] = 'a';
    m_szName[4] = 'm';
    m_szName[5] = 'e';
    m_szName[6] = '\0';
    m_szName[7] = '\0';
    m_BaseIP = nullptr;
    m_IP = nullptr;
    m_IPStack.fill(nullptr);
    m_StackDepth = 0;
    for (auto& var : m_LocalVars)
        var.uParam = 0;
    m_WakeTime = 0;
    m_IsActive = false;
    m_CondResult = false;
    m_UsesMissionCleanup = false;
    m_IsExternal = false;
    m_IsTextBlockOverride = false;
    m_ExternalType = -1;
    m_AndOrState = 0;
    m_NotFlag = false;
    m_DoneDeathArrest = false;
    m_SceneSkipIP = 0;
    m_ThisMustBeTheOnlyMissionRunning = false;
    m_IsDeathArrestCheckEnabled = true;
}

// @ 00469f00 [.text]
OpcodeResult CRunningScript::Process() {
    // Cutscene skip: jump the instruction pointer to the skip label.
    if (m_SceneSkipIP != 0) {
        if (CCutsceneMgr::IsCutsceneSkipButtonBeingPressed()) {
            CHud::m_BigMessage[1][0] = '\0';
            if (m_SceneSkipIP < 0)
                m_IP = m_BaseIP - m_SceneSkipIP;
            else
                m_IP = CTheScripts::ScriptSpace + m_SceneSkipIP;
            m_SceneSkipIP = 0;
            m_WakeTime = 0;
        }
    }

    if (m_UsesMissionCleanup)
        DoDeathArrestCheck();

    // Mission failed: unwind the call stack to the outermost frame.
    if (m_ThisMustBeTheOnlyMissionRunning && CTheScripts::FailCurrentMission == 1) {
        if (m_StackDepth > 1)
            m_StackDepth = 1;
        if (m_StackDepth == 1) {
            m_StackDepth = 0;
            m_IP = m_IPStack[0];
        }
    }

    CTheScripts::ReinitialiseSwitchStatementData();

    if (static_cast<uint32_t>(m_WakeTime) <= CTimer::m_snTimeInMilliseconds) {
        OpcodeResult result;
        do {
            CTheScripts::CommandsExecuted++;
            const uint16_t rawInstr = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            const uint32_t command = static_cast<uint32_t>(static_cast<int16_t>(rawInstr & 0x7FFF));
            m_NotFlag = ((rawInstr >> 15) & 1) != 0;
            result = (this->*s_OriginalCommandHandlerTable[command / 100])(static_cast<int32_t>(command));
        } while (result == OR_CONTINUE);
    }
    return OR_CONTINUE;
}

// @ 00469eb0 [.text]
OpcodeResult CRunningScript::ProcessOneCommand() {
    CTheScripts::CommandsExecuted++;
    const uint16_t rawInstr = *reinterpret_cast<uint16_t*>(m_IP);
    m_IP += 2;
    const uint32_t command = static_cast<uint32_t>(static_cast<int16_t>(rawInstr & 0x7FFF));
    m_NotFlag = ((rawInstr >> 15) & 1) != 0;
    return (this->*s_OriginalCommandHandlerTable[command / 100])(static_cast<int32_t>(command));
}

// ---------------------------------------------------------------------------
// Parameter collection / storage
// ---------------------------------------------------------------------------

// Helper: read the local variable at `loc`, honouring the mission-locals
// override (m_ThisMustBeTheOnlyMissionRunning).
static tScriptParam* GetLocalVarPtr(CRunningScript* script, uint16_t loc) {
    if (script->m_ThisMustBeTheOnlyMissionRunning)
        return &CTheScripts::LocalVariablesForCurrentMission[loc];
    return &script->m_LocalVars[loc];
}

// @ 00464080 [.text]
// Reads `count` parameters from the bytecode at m_IP into ScriptParams[].
void CRunningScript::CollectParameters(int16_t count) {
    tScriptParam* out = ScriptParams.data();
    for (int16_t i = 0; i < count; i++, out++) {
        const uint8_t type = *m_IP++;
        switch (type) {
        case SCRIPT_PARAM_STATIC_INT_32BITS: // 1
        case SCRIPT_PARAM_STATIC_FLOAT:      // 6
            *out = *reinterpret_cast<tScriptParam*>(m_IP);
            m_IP += sizeof(tScriptParam);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE: { // 2
            const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            *out = *reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + off);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE: { // 3
            const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            *out = *GetLocalVarPtr(this, loc);
            break;
        }
        case SCRIPT_PARAM_STATIC_INT_8BITS: // 4
            out->iParam = static_cast<int32_t>(static_cast<int8_t>(*m_IP++));
            break;
        case SCRIPT_PARAM_STATIC_INT_16BITS: { // 5
            const int16_t v = *reinterpret_cast<int16_t*>(m_IP);
            m_IP += 2;
            out->iParam = static_cast<int32_t>(v);
            break;
        }
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY: { // 7
            const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
            const uint16_t arrBase = ip[0];
            const uint16_t idxVarLoc = ip[1];
            const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
            m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
            int32_t idx;
            if (idxVarIsGlobal) {
                idx = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
            } else {
                idx = GetLocalVarPtr(this, idxVarLoc)->iParam;
            }
            *out = *reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + idx * 4 + arrBase);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY: { // 8
            const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
            const uint16_t arrBase = ip[0];
            const uint16_t idxVarLoc = ip[1];
            const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
            m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
            uint32_t idx;
            if (idxVarIsGlobal) {
                idx = *reinterpret_cast<uint32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
            } else {
                idx = GetLocalVarPtr(this, idxVarLoc)->uParam;
            }
            const uint16_t loc = static_cast<uint16_t>(arrBase + (idx & 0xFFFF));
            *out = *GetLocalVarPtr(this, loc);
            break;
        }
        default:
            // Unknown parameter type; the original falls through without
            // writing (leaving the ScriptParams slot untouched).
            break;
        }
    }
}

// @ 00464250 [.text]
// Reads the next parameter WITHOUT advancing m_IP (m_IP is restored).
int32_t CRunningScript::CollectNextParameterWithoutIncreasingPC() {
    uint8_t* const savedIP = m_IP;
    const uint8_t type = *m_IP++;
    int32_t result = -1;
    switch (type) {
    case SCRIPT_PARAM_STATIC_INT_32BITS: // 1
    case SCRIPT_PARAM_STATIC_FLOAT:      // 6
        result = *reinterpret_cast<int32_t*>(m_IP);
        break;
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE: { // 2
        const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
        result = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + off);
        break;
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE: { // 3
        const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
        result = GetPointerToLocalVariable(loc)->iParam;
        break;
    }
    case SCRIPT_PARAM_STATIC_INT_8BITS: // 4
        result = static_cast<int32_t>(static_cast<int8_t>(*m_IP));
        break;
    case SCRIPT_PARAM_STATIC_INT_16BITS: // 5
        result = static_cast<int32_t>(*reinterpret_cast<int16_t*>(m_IP));
        break;
    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY: { // 7
        uint16_t arrBase = 0;
        int32_t arrIdx = 0;
        ReadArrayInformation(0, &arrBase, &arrIdx);
        result = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + arrIdx * 4 + arrBase);
        break;
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY: { // 8
        uint16_t arrBase = 0;
        int32_t arrIdx = 0;
        ReadArrayInformation(0, &arrBase, &arrIdx);
        result = GetPointerToLocalArrayElement(arrBase, static_cast<uint16_t>(arrIdx), 1)->iParam;
        break;
    }
    default:
        break;
    }
    m_IP = savedIP;
    return result;
}

// @ 00464370 [.text]
// Writes ScriptParams[0..count) to the `count` variable destinations at m_IP.
void CRunningScript::StoreParameters(int16_t count) {
    tScriptParam* in = ScriptParams.data();
    for (int16_t i = 0; i < count; i++, in++) {
        const uint8_t type = *m_IP++;
        switch (type) {
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE: { // 2
            const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            *reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + off) = *in;
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE: { // 3
            const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            *GetLocalVarPtr(this, loc) = *in;
            break;
        }
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY: { // 7
            const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
            const uint16_t arrBase = ip[0];
            const uint16_t idxVarLoc = ip[1];
            const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
            m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
            int32_t idx;
            if (idxVarIsGlobal) {
                idx = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
            } else {
                idx = GetLocalVarPtr(this, idxVarLoc)->iParam;
            }
            *reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + idx * 4 + arrBase) = *in;
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY: { // 8
            const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
            const uint16_t arrBase = ip[0];
            const uint16_t idxVarLoc = ip[1];
            const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
            m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
            uint32_t idx;
            if (idxVarIsGlobal) {
                idx = *reinterpret_cast<uint32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
            } else {
                idx = GetLocalVarPtr(this, idxVarLoc)->uParam;
            }
            const uint16_t loc = static_cast<uint16_t>(arrBase + (idx & 0xFFFF));
            *GetLocalVarPtr(this, loc) = *in;
            break;
        }
        default:
            // Static types (1/4/5/6) cannot be stored to; the original
            // ignores them.
            break;
        }
    }
}

// @ 00464500 [.text]
// Reads the argument list of a START_NEW_SCRIPT call into the new script's
// local variables. The list is terminated by a 0x00 type byte.
void CRunningScript::ReadParametersForNewlyStartedScript(CRunningScript* newScript) {
    tScriptParam* out = newScript->m_LocalVars.data();
    uint8_t type = *m_IP++;
    while (type != 0) {
        switch (type) {
        case SCRIPT_PARAM_STATIC_INT_32BITS: // 1
        case SCRIPT_PARAM_STATIC_FLOAT:      // 6
            *out = *reinterpret_cast<tScriptParam*>(m_IP);
            m_IP += sizeof(tScriptParam);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE: { // 2
            const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            *out = *reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + off);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE: { // 3
            const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
            m_IP += 2;
            *out = *GetLocalVarPtr(this, loc);
            break;
        }
        case SCRIPT_PARAM_STATIC_INT_8BITS: // 4
            out->iParam = static_cast<int32_t>(static_cast<int8_t>(*m_IP++));
            break;
        case SCRIPT_PARAM_STATIC_INT_16BITS: { // 5
            const int16_t v = *reinterpret_cast<int16_t*>(m_IP);
            m_IP += 2;
            out->iParam = static_cast<int32_t>(v);
            break;
        }
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY: { // 7
            const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
            const uint16_t arrBase = ip[0];
            const uint16_t idxVarLoc = ip[1];
            const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
            m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
            int32_t idx;
            if (idxVarIsGlobal) {
                idx = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
            } else {
                idx = GetLocalVarPtr(this, idxVarLoc)->iParam;
            }
            *out = *reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + idx * 4 + arrBase);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY: { // 8
            const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
            const uint16_t arrBase = ip[0];
            const uint16_t idxVarLoc = ip[1];
            const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
            m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
            uint32_t idx;
            if (idxVarIsGlobal) {
                idx = *reinterpret_cast<uint32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
            } else {
                idx = GetLocalVarPtr(this, idxVarLoc)->uParam;
            }
            const uint16_t loc = static_cast<uint16_t>(arrBase + (idx & 0xFFFF));
            *out = *GetLocalVarPtr(this, loc);
            break;
        }
        default:
            break;
        }
        type = *m_IP++;
        out++;
    }
}

// ---------------------------------------------------------------------------
// Array / variable pointer helpers
// ---------------------------------------------------------------------------

// @ 0156e350 [.HOODLUM] (original entry stub at 00463cf0)
// Reads an array-access operand: base offset, index-variable location, and
// the current index value. If `updateIp` is set, m_IP advances past it.
void CRunningScript::ReadArrayInformation(int32_t updateIp, uint16_t* outArrVarOffset, int32_t* outArrElemIdx) {
    const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
    *outArrVarOffset = ip[0];
    const uint16_t idxVarLoc = ip[1];
    int32_t idx;
    if (static_cast<int16_t>(ip[2]) < 0) { // index variable is global
        idx = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
    } else { // index variable is local
        idx = GetPointerToLocalVariable(idxVarLoc)->iParam;
    }
    *outArrElemIdx = idx;
    if (updateIp)
        m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
}

// @ 0156f340 [.HOODLUM] (original entry stub at 00464700)
// Returns the absolute ScriptSpace offset of a global variable operand.
// NOTE: the decomp shows a Ghidra artifact (garbage CONCAT22 return) for
// parameter types other than 2/7; those paths return 0 here.
uint16_t CRunningScript::GetIndexOfGlobalVariable() {
    const uint8_t type = *m_IP++;
    if (type == SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE) { // 2
        const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        return off;
    }
    if (type != SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY) { // 7
        // Decomp artifact path (CONCAT22 garbage); not reachable for valid bytecode.
        return 0;
    }
    const uint16_t* ip = reinterpret_cast<uint16_t*>(m_IP);
    const uint16_t arrBase = ip[0];
    const uint16_t idxVarLoc = ip[1];
    const bool idxVarIsGlobal = static_cast<int16_t>(ip[2]) < 0;
    m_IP = reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(ip + 3));
    int32_t idx;
    if (idxVarIsGlobal) {
        idx = *reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + idxVarLoc);
    } else {
        idx = GetLocalVarPtr(this, idxVarLoc)->iParam;
    }
    return static_cast<uint16_t>(arrBase + idx * 4);
}

// @ 0156e2b0 [.HOODLUM] (original entry stub at 00463ca0)
tScriptParam* CRunningScript::GetPointerToLocalVariable(int32_t loc) {
    return GetLocalVarPtr(this, static_cast<uint16_t>(loc));
}

// @ 00463cc0 [.text]
tScriptParam* CRunningScript::GetPointerToLocalArrayElement(int32_t arrVarOffset, uint16_t arrElemIdx, uint8_t arrayEntriesSizeAsParams) {
    const int32_t loc = static_cast<int32_t>(arrElemIdx) * arrayEntriesSizeAsParams + arrVarOffset;
    return GetLocalVarPtr(this, static_cast<uint16_t>(loc));
}

// GetPointerToGlobalVariable / GetPointerToGlobalArrayElement have no named
// decomp entries (inlined in the original); implemented here from the
// ScriptSpace layout used by GetPointerToScriptVariable.
tScriptParam* CRunningScript::GetPointerToGlobalVariable(int32_t varId) {
    return reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + varId);
}

tScriptParam* CRunningScript::GetPointerToGlobalArrayElement(int32_t arrBase, uint16_t arrIdx, uint8_t arrayEntriesSizeAsParams) {
    return reinterpret_cast<tScriptParam*>(
        CTheScripts::ScriptSpace + arrBase + static_cast<int32_t>(arrIdx) * arrayEntriesSizeAsParams * sizeof(tScriptParam));
}

// @ 00464790 [.text]
// Returns a pointer to the variable referenced by the operand at m_IP.
// NOTE: `variableType` is unused in the original (the operand type byte
// fully determines the addressing); kept for signature compatibility.
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType variableType) {
    (void)variableType;
    const uint8_t type = *m_IP++;
    switch (type) {
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:   // 2
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE: // 0xA
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE:  // 0x10
    {
        const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        return reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + off);
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:   // 3
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE: // 0xB
    case SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE:  // 0x11
    {
        const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        return GetLocalVarPtr(this, loc);
    }
    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:      // 7
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY: // 0xC
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY:  // 0x12
    {
        uint16_t arrBase = 0;
        int32_t arrIdx = 0;
        ReadArrayInformation(1, &arrBase, &arrIdx);
        if (type == SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY) // 16-byte elements
            return reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + arrBase + arrIdx * 0x10);
        if (type == SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY) // 8-byte elements
            return reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + arrBase + arrIdx * 8);
        return reinterpret_cast<tScriptParam*>(CTheScripts::ScriptSpace + arrBase + arrIdx * 4);
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:      // 8
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY: // 0xD
    case SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY:  // 0x13
    {
        uint16_t arrBase = 0;
        int32_t arrIdx = 0;
        ReadArrayInformation(1, &arrBase, &arrIdx);
        uint8_t elemSizeAsParams = 1;
        if (type == SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY)
            elemSizeAsParams = 4;
        else if (type == SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY)
            elemSizeAsParams = 2;
        return GetPointerToLocalArrayElement(arrBase, static_cast<uint16_t>(arrIdx), elemSizeAsParams);
    }
    default:
        return nullptr;
    }
}

// @ 00463d50 [.text]
// Reads a text-label (string) operand at m_IP into `buffer`.
void CRunningScript::ReadTextLabelFromScript(char* buffer, uint8_t nBufferLength) {
    const uint8_t type = *m_IP++;
    switch (type) {
    case SCRIPT_PARAM_STATIC_SHORT_STRING: { // 9: 8 inline bytes
        for (int i = 0; i < 8; i++)
            *buffer++ = static_cast<char>(*m_IP++);
        return;
    }
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE: { // 0xA
        const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        strncpy(buffer, reinterpret_cast<char*>(CTheScripts::ScriptSpace + off), 8);
        return;
    }
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE: { // 0xB
        const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        tScriptParam* var = GetPointerToLocalVariable(loc);
        strncpy(buffer, reinterpret_cast<char*>(&var->i8Param), 8);
        return;
    }
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY:   // 0xC
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY: {  // 0x12
        uint16_t arrBase = 0;
        int32_t arrIdx = 0;
        ReadArrayInformation(1, &arrBase, &arrIdx);
        if (type == SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY) {
            strncpy(buffer, reinterpret_cast<char*>(CTheScripts::ScriptSpace + arrIdx * 8 + arrBase), 8);
            return;
        }
        char* src = reinterpret_cast<char*>(CTheScripts::ScriptSpace + arrBase + arrIdx * 0x10);
        if (nBufferLength > 15) {
            strncpy(buffer, src, 0x10);
            return;
        }
        strncpy(buffer, src, nBufferLength);
        return;
    }
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY: // 0xD
    case SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY: { // 0x13
        uint16_t arrBase = 0;
        int32_t arrIdx = 0;
        ReadArrayInformation(1, &arrBase, &arrIdx);
        tScriptParam* elem;
        if (type == SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY) {
            elem = GetPointerToLocalArrayElement(arrBase, static_cast<uint16_t>(arrIdx), 2);
            strncpy(buffer, reinterpret_cast<char*>(&elem->i8Param), 8);
            return;
        }
        elem = GetPointerToLocalArrayElement(arrBase, static_cast<uint16_t>(arrIdx), 4);
        if (nBufferLength >= 0x10) {
            strncpy(buffer, reinterpret_cast<char*>(&elem->i8Param), 0x10);
        } else {
            strncpy(buffer, reinterpret_cast<char*>(&elem->i8Param), nBufferLength);
        }
        return;
    }
    case SCRIPT_PARAM_STATIC_PASCAL_STRING: { // 0xE: length byte + data
        const uint8_t len = *m_IP++;
        for (uint8_t i = 0; i < len; i++)
            *buffer++ = static_cast<char>(*m_IP++);
        // Zero-pad the rest of the buffer.
        if (nBufferLength > len)
            memset(buffer, 0, nBufferLength - len);
        return;
    }
    case SCRIPT_PARAM_STATIC_LONG_STRING: { // 0xF: 16 inline bytes
        if (nBufferLength > 15) {
            for (int i = 0; i < 0x10; i++)
                *buffer++ = static_cast<char>(*m_IP++);
            return;
        }
        uint8_t copied = 0;
        while (copied < nBufferLength) {
            *buffer++ = static_cast<char>(*m_IP++);
            copied++;
        }
        m_IP += static_cast<uint8_t>(0x10 - copied); // skip the rest
        return;
    }
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE: { // 0x10
        const uint16_t off = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        strncpy(buffer, reinterpret_cast<char*>(CTheScripts::ScriptSpace + off),
                nBufferLength < 0x10 ? nBufferLength : 0x10);
        return;
    }
    case SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE: { // 0x11
        const uint16_t loc = *reinterpret_cast<uint16_t*>(m_IP);
        m_IP += 2;
        tScriptParam* var = GetPointerToLocalVariable(loc);
        strncpy(buffer, reinterpret_cast<char*>(&var->i8Param), nBufferLength < 0x10 ? nBufferLength : 0x10);
        return;
    }
    default:
        return;
    }
}

// ---------------------------------------------------------------------------
// Script list management / misc
// ---------------------------------------------------------------------------

// @ 01561990 [.HOODLUM] (original entry stub at 00464c00)
// Inserts this script at the head of `*queueList`.
void CRunningScript::AddScriptToList(CRunningScript** queueList) {
    m_pNext = *queueList;
    m_pPrev = nullptr;
    if (*queueList)
        (*queueList)->m_pPrev = this;
    *queueList = this;
}

// @ 0156dd60 [.HOODLUM] (original entry stub at 00464bd0)
// Unlinks this script from `*queueList`.
void CRunningScript::RemoveScriptFromList(CRunningScript** queueList) {
    if (m_pPrev == nullptr)
        *queueList = m_pNext;
    else
        m_pPrev->m_pNext = m_pNext;
    if (m_pNext)
        m_pNext->m_pPrev = m_pPrev;
}

// @ 00465aa0 [.text]
// Deactivates this script and releases its script-created entities.
// NOTE: the ped/object pool validation and streamed-script refcount parts
// need unported subsystems (CPool globals, CStreamedScripts, CTask);
// the structure below is faithful, the pool-dependent blocks are TODO.
void CRunningScript::ShutdownThisScript() {
    m_IsActive = false;
    // TODO: if (m_IsExternal) CTheScripts::StreamedScripts refcount--.
    //   Needs CStreamedScripts (incomplete type in CTheScripts.h).
    switch (m_ExternalType) {
    case 0:
    case 2:
    case 3:
    case 5: {
        // TODO: validate local var 0 as a ped handle against the ped pool,
        // clear the ped's script flags; if m_ExternalType == 5 assign a
        // CTaskSimpleFinishBrain via CScriptedBrainTaskStore::SetTask.
        // Decomp: src/CRunningScript/ShutdownThisScript_00465aa0.c
        break;
    }
    case 1:
    case 4: {
        // TODO: validate local var 0 as an object handle against the object
        // pool and set its "scripted" flag (bit 0x100000 at +0x140).
        // Decomp: src/CRunningScript/ShutdownThisScript_00465aa0.c
        break;
    }
    default:
        break;
    }
}

// @ 004859d0 [.text]
// Updates m_CondResult from a condition result, honouring m_NotFlag and the
// compound-if AND/OR state machine (see LogicalOpType in the header).
void CRunningScript::UpdateCompareFlag(bool state) {
    if (m_NotFlag)
        state = !state;
    const uint16_t andOr = m_AndOrState;
    if (andOr == 0) {
        m_CondResult = state;
        return;
    }
    bool last;
    if (andOr >= 1 && andOr <= 8) { // ANDS_1..ANDS_8
        m_CondResult = m_CondResult && state;
        last = (andOr == ANDS_1);
    } else if (andOr >= ORS_1 && andOr <= ORS_8) { // ORS_1..ORS_8
        m_CondResult = m_CondResult || state;
        last = (andOr == ORS_1);
    } else {
        return;
    }
    m_AndOrState = last ? 0 : static_cast<uint16_t>(andOr - 1);
}

// @ 015620b0 [.HOODLUM] (original entry stub at 00464da0)
// Sets m_IP: negative values are relative to m_BaseIP, others are absolute
// ScriptSpace offsets.
void CRunningScript::UpdatePC(int32_t newIP) {
    if (newIP < 0)
        m_IP = m_BaseIP - newIP;
    else
        m_IP = CTheScripts::ScriptSpace + newIP;
}

// @ 01564f00 [.HOODLUM] (original entry stub at 00485a50)
// If the player died/was arrested while on a mission, unwinds this script's
// call stack so the mission restarts from the top.
void CRunningScript::DoDeathArrestCheck() {
    if (!m_IsDeathArrestCheckEnabled || CTheScripts::OnAMissionFlag == 0)
        return;
    if (*reinterpret_cast<int32_t*>(CTheScripts::ScriptSpace + CTheScripts::OnAMissionFlag) != 1)
        return;
    // TODO: needs CWorld::Players[CWorld::PlayerInFocus] (player subsystem
    // not ported). Faithful body once available:
    //   CPlayerInfo* p = &CWorld::Players[CWorld::PlayerInFocus];
    //   if (CPlayerInfo::IsRestartingAfterDeath(p) || CPlayerInfo::IsRestartingAfterArrest(p)) {
    //       if (m_StackDepth > 1) m_StackDepth = 1;
    //       m_StackDepth--;
    //       m_IP = m_IPStack[m_StackDepth];
    //   }
    // Decomp: src/CRunningScript/DoDeathArrestCheck_01564f00.c
}

// @ 0156e7b0 [.HOODLUM] (original entry stub at 00464d70)
bool CRunningScript::IsPedDead(CPed* ped) const {
    return ped->m_nPedState == PEDSTATE_DIE ||
           ped->m_nPedState == PEDSTATE_DEAD ||
           ped->m_nPedState == PEDSTATE_DIE_BY_STEALTH;
}

// @ 00485b10 [.text]
int16_t CRunningScript::GetPadState(uint16_t playerIndex, eButtonId buttonId) {
    const CControllerState& state = CPad::GetPad(playerIndex)->NewState;
    switch (buttonId) {
    case BUTTON_LEFT_STICK_X:  return state.LeftStickX;
    case BUTTON_LEFT_STICK_Y:  return state.LeftStickY;
    case BUTTON_RIGHT_STICK_X: return state.RightStickX;
    case BUTTON_RIGHT_STICK_Y: return state.RightStickY;
    case BUTTON_LEFT_SHOULDER1:  return state.LeftShoulder1;
    case BUTTON_LEFT_SHOULDER2:  return state.LeftShoulder2;
    case BUTTON_RIGHT_SHOULDER1: return state.RightShoulder1;
    case BUTTON_RIGHT_SHOULDER2: return state.RightShoulder2;
    case BUTTON_DPAD_UP:    return state.DPadUp;
    case BUTTON_DPAD_DOWN:  return state.DPadDown;
    case BUTTON_DPAD_LEFT:  return state.DPadLeft;
    case BUTTON_DPAD_RIGHT: return state.DPadRight;
    case BUTTON_START:  return state.Start;
    case BUTTON_SELECT: return state.Select;
    case BUTTON_SQUARE:   return state.ButtonSquare;
    case BUTTON_TRIANGLE: return state.ButtonTriangle;
    case BUTTON_CROSS:    return state.ButtonCross;
    case BUTTON_CIRCLE:   return state.ButtonCircle;
    case BUTTON_LEFTSHOCK:  return state.ShockButtonL;
    case BUTTON_RIGHTSHOCK: return state.ShockButtonR;
    default: return 0;
    }
}

// @ 00464f50 [.text]
// Remaps emergency-service ped model variants to the plain cop model when
// the ped type is a cop (models 0x118-0x120 are the emergency variants).
void CRunningScript::GetCorrectPedModelIndexForEmergencyServiceType(ePedType pedType, uint32_t* typeSpecificModelId) {
    switch (*typeSpecificModelId) {
    case 0x118:
    case 0x119:
    case 0x11A:
    case 0x11C:
        if (pedType == PED_TYPE_COP)
            *typeSpecificModelId = 0;
        break;
    case 0x11B:
        if (pedType == PED_TYPE_COP)
            *typeSpecificModelId = 7;
        break;
    case 0x11D:
        if (pedType == PED_TYPE_COP)
            *typeSpecificModelId = 2;
        break;
    case 0x11E:
        if (pedType == PED_TYPE_COP)
            *typeSpecificModelId = 4;
        break;
    case 0x11F:
        if (pedType == PED_TYPE_COP)
            *typeSpecificModelId = 5;
        break;
    default:
        break;
    }
}

// TODO: NOTSA custom-command infrastructure; full type ported with the
// command-parser subsystem. Cannot instantiate the incomplete
// notsa::script::CommandHandlerFunction yet.
notsa::script::CommandHandlerFunction& CRunningScript::CustomCommandHandlerOf(scm::eScriptCommands command) {
    (void)command;
    std::terminate();
}

void CRunningScript::ResetIP() {
    m_IP = m_BaseIP;
}

// ---------------------------------------------------------------------------
// Opcode dispatch
// ---------------------------------------------------------------------------
// Decomp: src/CRunningScript/ProcessCommands0To99_00465e60.c
// Handles opcodes 0-99: the core VM instruction set (control flow, WAIT,
// variable arithmetic, comparisons, GOSUB/RETURN, script control).
// Opcode numbers below are decimal; names are the standard SCM mnemonics.

OpcodeResult CRunningScript::ProcessCommands0To99(int32_t command) {
    tScriptParam* var = nullptr;
    tScriptParam* var2 = nullptr;
    bool state = false;

    switch (command) {
    case 0: // NOP
        return OR_CONTINUE;

    case 1: // WAIT
        CollectParameters(1);
        m_WakeTime = static_cast<int32_t>(CTimer::m_snTimeInMilliseconds) + ScriptParams[0].iParam;
        return OR_WAIT;

    case 2: // GOTO
        CollectParameters(1);
        UpdatePC(static_cast<int32_t>(ScriptParams[0].uParam));
        return OR_CONTINUE;

    case 3: // SHAKE_CAM
        CollectParameters(1);
        CamShakeNoPos(&TheCamera, static_cast<float>(ScriptParams[0].iParam) * 0.001f);
        return OR_CONTINUE;

    case 4: // SET_VAR_INT: $var = int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->iParam = ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 5: // SET_VAR_FLOAT: $var = float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        *var = ScriptParams[0];
        return OR_CONTINUE;

    case 6: // SET_LVAR_INT: @var = int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        *var = ScriptParams[0];
        return OR_CONTINUE;

    case 7: // SET_LVAR_FLOAT: @var = float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->iParam = ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 8: // ADD_VAL_TO_INT_VAR: $var += int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->iParam = var->iParam + ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 9: // ADD_VAL_TO_FLOAT_VAR: $var += float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->fParam = var->fParam + ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 10: // ADD_VAL_TO_INT_LVAR: @var += int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->iParam = var->iParam + ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 11: // ADD_VAL_TO_FLOAT_LVAR: @var += float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->fParam = var->fParam + ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 12: // SUB_VAL_FROM_INT_VAR: $var -= int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->iParam = var->iParam - ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 13: // SUB_VAL_FROM_FLOAT_VAR: $var -= float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->fParam = var->fParam - ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 14: // SUB_VAL_FROM_INT_LVAR: @var -= int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->iParam = var->iParam - ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 15: // SUB_VAL_FROM_FLOAT_LVAR: @var -= float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->fParam = var->fParam - ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 16: // MULT_INT_VAR_BY_VAL: $var *= int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->uParam = var->uParam * ScriptParams[0].uParam;
        return OR_CONTINUE;

    case 17: // MULT_FLOAT_VAR_BY_VAL: $var *= float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->fParam = var->fParam * ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 18: // MULT_INT_LVAR_BY_VAL: @var *= int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->uParam = var->uParam * ScriptParams[0].uParam;
        return OR_CONTINUE;

    case 19: // MULT_FLOAT_LVAR_BY_VAL: @var *= float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->fParam = var->fParam * ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 20: // DIV_INT_VAR_BY_VAL: $var /= int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->iParam = var->iParam / ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 21: // DIV_FLOAT_VAR_BY_VAL: $var /= float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        var->fParam = var->fParam / ScriptParams[0].fParam;
        return OR_CONTINUE;

    case 22: // DIV_INT_LVAR_BY_VAL: @var /= int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->iParam = var->iParam / ScriptParams[0].iParam;
        return OR_CONTINUE;

    case 23: // DIV_FLOAT_LVAR_BY_VAL: @var /= float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        var->fParam = var->fParam / ScriptParams[0].fParam;
        return OR_CONTINUE;

    // --- Integer greater-than comparisons ---
    case 24: // IS_INT_VAR_GREATER_THAN_NUMBER: $var > int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        UpdateCompareFlag(ScriptParams[0].iParam < var->iParam);
        return OR_CONTINUE;

    case 25: // IS_INT_LVAR_GREATER_THAN_NUMBER: @var > int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        UpdateCompareFlag(ScriptParams[0].iParam < var->iParam);
        return OR_CONTINUE;

    case 26: // IS_INT_VAR_GREATER_THAN_INT_VAR: $a > $b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        UpdateCompareFlag(var->iParam < ScriptParams[0].iParam);
        return OR_CONTINUE;

    case 27: // IS_INT_LVAR_GREATER_THAN_INT_LVAR: @a > @b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_LOCAL);
        UpdateCompareFlag(var->iParam < ScriptParams[0].iParam);
        return OR_CONTINUE;

    case 28: // IS_INT_VAR_GREATER_THAN_FLOAT_VAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var2->iParam < var->iParam;
        break;

    case 29: // IS_INT_LVAR_GREATER_THAN_FLOAT_VAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var2->iParam < var->iParam;
        break;

    case 30: // IS_INT_VAR_GREATER_THAN_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var2->iParam < var->iParam;
        break;

    case 31: // IS_INT_LVAR_GREATER_THAN_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var2->iParam < var->iParam;
        break;

    // --- Float greater-than comparisons ---
    // NOTE: the original uses bit tricks ((a<b)<<8 | (a==b)<<14)==0 which
    // evaluate TRUE when either operand is NaN. Replicated below.
    case 32: // IS_FLOAT_VAR_GREATER_THAN_NUMBER: $var > float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        state = !(var->fParam < ScriptParams[0].fParam) && !(var->fParam == ScriptParams[0].fParam);
        break;

    case 33: // IS_FLOAT_LVAR_GREATER_THAN_NUMBER: @var > float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        state = !(var->fParam < ScriptParams[0].fParam) && !(var->fParam == ScriptParams[0].fParam);
        break;

    case 34: // IS_FLOAT_VAR_GREATER_THAN_FLOAT_VAR: $a > $b
        // NOTE: unlike its siblings, the original emits a PLAIN less-than
        // here (no NaN-true quirk); replicated faithfully.
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var->fParam < ScriptParams[0].fParam; // ($b < $a)
        break;

    case 35: // IS_FLOAT_LVAR_GREATER_THAN_FLOAT_VAR: @a > @b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_LOCAL);
        state = !(ScriptParams[0].fParam < var->fParam) && !(ScriptParams[0].fParam == var->fParam);
        break;

    case 36: // IS_FLOAT_VAR_GREATER_THAN_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = !(var->fParam < var2->fParam) && !(var->fParam == var2->fParam);
        break;

    case 37: // IS_FLOAT_LVAR_GREATER_THAN_FLOAT_LVAR
        // NOTE: plain less-than in the original (no NaN-true quirk); faithful.
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var2->fParam < var->fParam; // (@b < @a)
        break;

    case 38: // IS_FLOAT_VAR_GREATER_THAN_INT_VAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = !(var->fParam < var2->fParam) && !(var->fParam == var2->fParam);
        break;

    case 39: // IS_FLOAT_LVAR_GREATER_THAN_INT_VAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = !(var->fParam < var2->fParam) && !(var->fParam == var2->fParam);
        break;

    // --- Integer greater-or-equal comparisons ---
    case 40: // IS_INT_VAR_GREATER_OR_EQUAL_TO_NUMBER: $var >= int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        state = ScriptParams[0].iParam <= var->iParam;
        break;

    case 41: // IS_INT_LVAR_GREATER_OR_EQUAL_TO_NUMBER: @var >= int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        state = ScriptParams[0].iParam <= var->iParam;
        break;

    case 42: // IS_INT_VAR_GREATER_OR_EQUAL_TO_INT_VAR: $a >= $b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var->iParam <= ScriptParams[0].iParam;
        break;

    case 43: // IS_INT_LVAR_GREATER_OR_EQUAL_TO_INT_LVAR: @a >= @b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_LOCAL);
        state = var->iParam <= ScriptParams[0].iParam;
        break;

    case 44: // IS_INT_VAR_GREATER_OR_EQUAL_TO_FLOAT_VAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var2->iParam <= var->iParam;
        break;

    case 45: // IS_INT_LVAR_GREATER_OR_EQUAL_TO_FLOAT_VAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var2->iParam <= var->iParam;
        break;

    case 46: // IS_INT_VAR_GREATER_OR_EQUAL_TO_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var2->iParam <= var->iParam;
        break;

    case 47: // IS_INT_LVAR_GREATER_OR_EQUAL_TO_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var2->iParam <= var->iParam;
        break;

    // --- Float greater-or-equal comparisons (same NaN-true quirk as above) ---
    case 48: // IS_FLOAT_VAR_GREATER_OR_EQUAL_TO_NUMBER: $var >= float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        state = ScriptParams[0].fParam <= var->fParam;
        break;

    case 49: // IS_FLOAT_LVAR_GREATER_OR_EQUAL_TO_NUMBER: @var >= float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        state = ScriptParams[0].fParam <= var->fParam;
        break;

    case 50: // IS_FLOAT_VAR_GREATER_OR_EQUAL_TO_FLOAT_VAR: $a >= $b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var->fParam <= ScriptParams[0].fParam;
        break;

    case 51: // IS_FLOAT_LVAR_GREATER_OR_EQUAL_TO_FLOAT_VAR: @a >= @b
        CollectParameters(1);
        var = GetPointerToScriptVariable(VAR_LOCAL);
        state = var->fParam <= ScriptParams[0].fParam;
        break;

    case 52: // IS_FLOAT_VAR_GREATER_OR_EQUAL_TO_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var2->fParam <= var->fParam;
        break;

    case 53: // IS_FLOAT_LVAR_GREATER_OR_EQUAL_TO_FLOAT_LVAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var->fParam <= var2->fParam;
        break;

    case 54: // IS_FLOAT_VAR_GREATER_OR_EQUAL_TO_INT_VAR
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var2->fParam <= var->fParam;
        break;

    case 55: // IS_FLOAT_LVAR_GREATER_OR_EQUAL_TO_INT_VAR
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var2->fParam <= var->fParam;
        break;

    // --- Integer equality comparisons ---
    case 56: // IS_INT_VAR_EQUAL_TO_NUMBER: $var == int
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        state = var->uParam == ScriptParams[0].uParam;
        break;

    case 57: // IS_INT_LVAR_EQUAL_TO_NUMBER: @var == int
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        state = var->uParam == ScriptParams[0].uParam;
        break;

    case 58: // IS_INT_VAR_EQUAL_TO_INT_VAR: $a == $b
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var->uParam == var2->uParam;
        break;

    case 59: // IS_INT_LVAR_EQUAL_TO_INT_LVAR: @a == @b
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var->uParam == var2->uParam;
        break;

    case 60: // IS_INT_VAR_EQUAL_TO_INT_LVAR (mixed)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = var->uParam == var2->uParam;
        break;

    // --- Float equality comparisons ---
    // NOTE: cases 66/67/69/70 use a POPCOUNT bit trick that evaluates TRUE
    // when either operand is NaN (faithful to the original x87-era codegen).
    case 66: // IS_FLOAT_VAR_EQUAL_TO_NUMBER: $var == float
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        CollectParameters(1);
        state = std::isnan(var->fParam) || std::isnan(ScriptParams[0].fParam) ||
                (var->fParam == ScriptParams[0].fParam);
        break;

    case 67: // IS_FLOAT_LVAR_EQUAL_TO_NUMBER: @var == float
        var = GetPointerToScriptVariable(VAR_LOCAL);
        CollectParameters(1);
        state = std::isnan(var->fParam) || std::isnan(ScriptParams[0].fParam) ||
                (var->fParam == ScriptParams[0].fParam);
        break;

    case 68: // IS_FLOAT_VAR_EQUAL_TO_FLOAT_VAR: $a == $b
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        state = var->fParam == var2->fParam;
        break;

    case 69: // IS_FLOAT_LVAR_EQUAL_TO_FLOAT_LVAR: @a == @b
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = std::isnan(var->fParam) || std::isnan(var2->fParam) ||
                (var->fParam == var2->fParam);
        break;

    case 70: // IS_FLOAT_VAR_EQUAL_TO_FLOAT_LVAR (mixed)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        state = std::isnan(var->fParam) || std::isnan(var2->fParam) ||
                (var->fParam == var2->fParam);
        break;

    // --- Script control flow ---
    case 77: // GOTO_IF_FALSE
        CollectParameters(1);
        if (!m_CondResult)
            UpdatePC(static_cast<int32_t>(ScriptParams[0].uParam));
        return OR_CONTINUE;

    case 78: // TERMINATE_THIS_SCRIPT
        if (m_ThisMustBeTheOnlyMissionRunning)
            CTheScripts::bAlreadyRunningAMissionScript = false;
        RemoveScriptFromList(&CTheScripts::pActiveScripts);
        AddScriptToList(&CTheScripts::pIdleScripts);
        ShutdownThisScript();
        return OR_WAIT;

    case 79: { // START_NEW_SCRIPT
        CollectParameters(1);
        // NOTE: the decomp has a peculiar `if (ScriptParams[0].iParam < 0)
        // tVar4 = <commandId>` fallback; the negative path is not reachable
        // from valid main.scm bytecode (labels are absolute ScriptSpace
        // offsets) and is preserved here literally.
        int32_t label = ScriptParams[0].iParam;
        if (label < 0)
            label = command;
        CRunningScript* newScript =
            CTheScripts::StartNewScript(CTheScripts::ScriptSpace + label);
        ReadParametersForNewlyStartedScript(newScript);
        return OR_CONTINUE;
    }

    case 80: // GOSUB
        CollectParameters(1);
        m_IPStack[m_StackDepth] = m_IP;
        m_StackDepth++;
        UpdatePC(static_cast<int32_t>(ScriptParams[0].uParam));
        return OR_CONTINUE;

    case 81: // RETURN
        m_StackDepth--;
        m_IP = m_IPStack[m_StackDepth];
        return OR_CONTINUE;

    case 82: // (unnamed in the original; consumes 6 params, no other effect)
        CollectParameters(6);
        return OR_CONTINUE;

    case 83: // CREATE_PLAYER
        // TODO: needs CStreaming, CPlayerPed, CWorld, CPlaceable, CTaskManager.
        // Decomp: src/CRunningScript/ProcessCommands0To99_00465e60.c, case 0x53.
        CollectParameters(6); // keep m_IP in sync; full body TODO
        return OR_CONTINUE;

    // --- Variable-to-variable arithmetic ---
    case 88: // ADD_VAL_TO_INT_VAR ($a += $b)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        var->iParam = var->iParam + var2->iParam;
        return OR_CONTINUE;

    case 89: // ADD_VAL_TO_FLOAT_VAR ($a += $b)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        var->fParam = var->fParam + var2->fParam;
        return OR_CONTINUE;

    case 90: // ADD_VAL_TO_INT_LVAR (@a += @b)
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        var->iParam = var->iParam + var2->iParam;
        return OR_CONTINUE;

    case 91: // ADD_VAL_TO_FLOAT_LVAR (@a += @b)
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        var->fParam = var->fParam + var2->fParam;
        return OR_CONTINUE;

    case 92: // ADD_VAL_TO_INT_LVAR (mixed: @a += $b)
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        var->iParam = var->iParam + var2->iParam;
        return OR_CONTINUE;

    case 93: // ADD_VAL_TO_FLOAT_LVAR (mixed: @a += $b)
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        var->fParam = var->fParam + var2->fParam;
        return OR_CONTINUE;

    case 94: // ADD_VAL_TO_INT_VAR (mixed: $a += @b)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        var->iParam = var->iParam + var2->iParam;
        return OR_CONTINUE;

    case 95: // ADD_VAL_TO_FLOAT_VAR (mixed: $a += @b)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        var->fParam = var->fParam + var2->fParam;
        return OR_CONTINUE;

    case 96: // SUB_VAL_FROM_INT_VAR ($a -= $b)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        var->iParam = var->iParam - static_cast<int32_t>(var2->uParam);
        return OR_CONTINUE;

    case 97: // SUB_VAL_FROM_FLOAT_VAR ($a -= $b)
        var = GetPointerToScriptVariable(VAR_GLOBAL);
        var2 = GetPointerToScriptVariable(VAR_GLOBAL);
        var->fParam = var->fParam - var2->fParam;
        return OR_CONTINUE;

    case 98: // SUB_VAL_FROM_INT_LVAR (@a -= @b)
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        var->iParam = var->iParam - static_cast<int32_t>(var2->uParam);
        return OR_CONTINUE;

    case 99: // SUB_VAL_FROM_FLOAT_LVAR (@a -= @b)
        var = GetPointerToScriptVariable(VAR_LOCAL);
        var2 = GetPointerToScriptVariable(VAR_LOCAL);
        var->fParam = var->fParam - var2->fParam;
        return OR_CONTINUE;

    default:
        // Opcodes 61-65, 84-87 have no handler in the original jump table;
        // the decomp falls into `default: return 0xff` (OR_INTERRUPT),
        // which stops this script's command loop for the frame.
        return OR_INTERRUPT;
    }

    // Comparison opcodes reach here via `break` with `state` set.
    UpdateCompareFlag(state);
    return OR_CONTINUE;
}

// ---------------------------------------------------------------------------
// Remaining opcode dispatch blocks (TODO)
// ---------------------------------------------------------------------------
// Each block below handles 100 opcodes and is a direct port of the matching
// decomp file. They are stubbed as OR_CONTINUE until ported; the table at
// the top of this file already references them so the dispatch structure is
// complete.

// @ 00466de0 — opcodes 100-199 (e.g. 100: SET_CHAR_COORDINATES,
//   101: IS_PLAYER_IN_AREA_2D, 214: ANDOR lives in the 200-block, ...)
OpcodeResult CRunningScript::ProcessCommands100To199(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands100To199_00466de0.c
    return OR_CONTINUE;
}

// @ 00469390 — opcodes 200-299 (includes 214/0xD6: ANDOR,
//   215/0xD7: START_NEW_SCRIPT, 216/0xD8: mission cleanup call)
OpcodeResult CRunningScript::ProcessCommands200To299(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands200To299_00469390.c
    return OR_CONTINUE;
}

// @ 0047c100 — opcodes 300-399
OpcodeResult CRunningScript::ProcessCommands300To399(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands300To399_0047c100.c
    return OR_CONTINUE;
}

// @ 0047d210 — opcodes 400-499
OpcodeResult CRunningScript::ProcessCommands400To499(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands400To499_0047d210.c
    return OR_CONTINUE;
}

// @ 0047e090 — opcodes 500-599
OpcodeResult CRunningScript::ProcessCommands500To599(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands500To599_0047e090.c
    return OR_CONTINUE;
}

// @ 0047f370 — opcodes 600-699
OpcodeResult CRunningScript::ProcessCommands600To699(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands600To699_0047f370.c
    return OR_CONTINUE;
}

// @ 0047fa30 — opcodes 700-799
OpcodeResult CRunningScript::ProcessCommands700To799(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands700To799_0047fa30.c
    return OR_CONTINUE;
}

// @ 00481300 — opcodes 800-899
OpcodeResult CRunningScript::ProcessCommands800To899(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands800To899_00481300.c
    return OR_CONTINUE;
}

// @ 00483bd0 — opcodes 900-999
OpcodeResult CRunningScript::ProcessCommands900To999(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands900To999_00483bd0.c
    return OR_CONTINUE;
}

// @ 00489500 — opcodes 1000-1099
OpcodeResult CRunningScript::ProcessCommands1000To1099(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1000To1099_00489500.c
    return OR_CONTINUE;
}

// @ 0048a320 — opcodes 1100-1199
OpcodeResult CRunningScript::ProcessCommands1100To1199(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1100To1199_0048a320.c
    return OR_CONTINUE;
}

// @ 0048b590 — opcodes 1200-1299
OpcodeResult CRunningScript::ProcessCommands1200To1299(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1200To1299_0048b590.c
    return OR_CONTINUE;
}

// @ 0048cdd0 — opcodes 1300-1399
OpcodeResult CRunningScript::ProcessCommands1300To1399(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1300To1399_0048cdd0.c
    return OR_CONTINUE;
}

// @ 0048eaa0 — opcodes 1400-1499
OpcodeResult CRunningScript::ProcessCommands1400To1499(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1400To1499_0048eaa0.c
    return OR_CONTINUE;
}

// @ 00490db0 — opcodes 1500-1599
OpcodeResult CRunningScript::ProcessCommands1500To1599(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1500To1599_00490db0.c
    return OR_CONTINUE;
}

// @ 00493fe0 — opcodes 1600-1699
OpcodeResult CRunningScript::ProcessCommands1600To1699(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1600To1699_00493fe0.c
    return OR_CONTINUE;
}

// @ 00496e00 — opcodes 1700-1799
OpcodeResult CRunningScript::ProcessCommands1700To1799(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1700To1799_00496e00.c
    return OR_CONTINUE;
}

// @ 0046d050 — opcodes 1800-1899
OpcodeResult CRunningScript::ProcessCommands1800To1899(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1800To1899_0046d050.c
    return OR_CONTINUE;
}

// @ 0046b460 — opcodes 1900-1999
OpcodeResult CRunningScript::ProcessCommands1900To1999(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands1900To1999_0046b460.c
    return OR_CONTINUE;
}

// @ 00472310 — opcodes 2000-2099
OpcodeResult CRunningScript::ProcessCommands2000To2099(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2000To2099_00472310.c
    return OR_CONTINUE;
}

// @ 00470a90 — opcodes 2100-2199
OpcodeResult CRunningScript::ProcessCommands2100To2199(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2100To2199_00470a90.c
    return OR_CONTINUE;
}

// @ 00474900 — opcodes 2200-2299
OpcodeResult CRunningScript::ProcessCommands2200To2299(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2200To2299_00474900.c
    return OR_CONTINUE;
}

// @ 004762d0 — opcodes 2300-2399
OpcodeResult CRunningScript::ProcessCommands2300To2399(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2300To2399_004762d0.c
    return OR_CONTINUE;
}

// @ 00478000 — opcodes 2400-2499
OpcodeResult CRunningScript::ProcessCommands2400To2499(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2400To2499_00478000.c
    return OR_CONTINUE;
}

// @ 0047a760 — opcodes 2500-2599
OpcodeResult CRunningScript::ProcessCommands2500To2599(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2500To2599_0047a760.c
    return OR_CONTINUE;
}

// @ 00479da0 — opcodes 2600-2699
OpcodeResult CRunningScript::ProcessCommands2600To2699(int32_t command) {
    (void)command;
    // TODO: port from src/CRunningScript/ProcessCommands2600To2699_00479da0.c
    return OR_CONTINUE;
}
