// CRunningScript - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Scripts/RunningScript.h
// One running script thread: instruction pointer, call stack, local variables,
// timers, and the opcode command implementations.
//
// Adaptations: stripped plugin-sdk header block, InjectHooks() and
// InjectCustomCommandHooks() (plugin-sdk/NOTSA hooking), StaticRef globals
// (now plain static members; original 1.0 US addresses kept as comments and
// re-resolved in CRunningScript.cpp for the clean-room build), VALIDATE_SIZE ->
// guarded static_assert. OpcodeResult (from Scripts/OpcodeResult.h) and
// tScriptParam (from Scripts/ScriptParam.h) are defined locally/included.
// The __thiscall on CommandHandlerFn_t is dropped (MSVC x86 member functions
// are __thiscall by default). strcpy_s/strncpy_s are MSVC-only; TODO:
// portability wrapper when the project targets other compilers.

#pragma once

#include "CTheScripts.h" // CTheScripts::ScriptSpace / LocalVariablesForCurrentMission
#include "CVector.h"     // CVector, CVector2D
#include "CRect.h"       // CRect
#include "ScriptParam.h" // tScriptParam

#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>

// Local VERIFY shim: gta-reversed's VERIFY halts in debug builds on failure.
// TODO: move to a shared debug header when more subsystems use it.
#ifndef VERIFY
#define VERIFY(x) assert(x)
#endif

// From gta-reversed source/game_sa/Scripts/OpcodeResult.h.
// OR_IMPLEMENTED_YET is NOTSA.
enum OpcodeResult : int8_t {
    OR_CONTINUE = 0,
    OR_WAIT = 1,
    OR_INTERRUPT = -1,
    OR_IMPLEMENTED_YET = -2 // NOTSA
};

// ---- Forward declarations (ported in later subsystems) ----
#include "ePedType.h" // was opaque decl; full port landed with ped batch
class CPed;
class CTask;
class CEntity;

// NOTSA custom-command infrastructure (from CommandParser); the full type is
// ported with the command-parser subsystem.
namespace notsa::script { struct CommandHandlerFunction; }

enum eScriptParameterType : uint8_t {
    SCRIPT_PARAM_END_OF_ARGUMENTS, //< Special type used for vararg stuff

    SCRIPT_PARAM_STATIC_INT_32BITS,
    SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE, //< Global int32 variable
    SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE, //< Local int32 variable
    SCRIPT_PARAM_STATIC_INT_8BITS,
    SCRIPT_PARAM_STATIC_INT_16BITS,
    SCRIPT_PARAM_STATIC_FLOAT,

    // Types below are only available in GTA SA

    // Number arrays
    SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY, //< Global array of numbers (always int32)
    SCRIPT_PARAM_LOCAL_NUMBER_ARRAY, //< Local array of numbers (always int32)

    SCRIPT_PARAM_STATIC_SHORT_STRING, //< Static 8 byte string

    SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE, //< Local 8 byte string
    SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE, //< Local 8 byte string

    SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY, //< Global 8 byte string array
    SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY,  //< Local 8 byte string array

    SCRIPT_PARAM_STATIC_PASCAL_STRING, //< Variable-length string (1 byte (unsigned) length + string data)
    SCRIPT_PARAM_STATIC_LONG_STRING,    //< 16 byte string

    SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE, //< Global 16 byte string
    SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE, //< Local 16 byte string

    SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY, //< Global array of 16 byte strings
    SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY, //< Local array of 16 byte strings
};

enum eScriptVariableType : uint8_t {
    VAR_LOCAL  = 1,
    VAR_GLOBAL = 2
};

enum eButtonId : uint16_t {
    BUTTON_LEFT_STICK_X,
    BUTTON_LEFT_STICK_Y,
    BUTTON_RIGHT_STICK_X,
    BUTTON_RIGHT_STICK_Y,
    BUTTON_LEFT_SHOULDER1,
    BUTTON_LEFT_SHOULDER2,
    BUTTON_RIGHT_SHOULDER1,
    BUTTON_RIGHT_SHOULDER2,
    BUTTON_DPAD_UP,
    BUTTON_DPAD_DOWN,
    BUTTON_DPAD_LEFT,
    BUTTON_DPAD_RIGHT,
    BUTTON_START,
    BUTTON_SELECT,
    BUTTON_SQUARE,
    BUTTON_TRIANGLE,
    BUTTON_CROSS,
    BUTTON_CIRCLE,
    BUTTON_LEFTSHOCK,
    BUTTON_RIGHTSHOCK,
};

enum {
    MAX_STACK_DEPTH = 8,
    MAX_LOCAL_VARS  = 32,
    MAX_NUM_TIMERS      = 2
};

constexpr auto SHORT_STRING_SIZE           = 8;
constexpr auto LONG_STRING_SIZE            = 16;
constexpr auto COMMANDS_CHAR_BUFFER_SIZE   = 64;
constexpr auto COMMANDS_CHAR_BUFFERS_COUNT = 16;

namespace scm {
constexpr size_t MAX_STRING_SIZE = 255 - 1; // Maximum string length

// eScriptCommands (the full opcode enum) is not ported yet; the opaque
// declaration keeps the Instruction bitfield below compiling.
// TODO: port the full opcode enum with the command-parser subsystem.
enum eScriptCommands : uint16_t;

using ShortString = char[SHORT_STRING_SIZE];
using LongString = char[LONG_STRING_SIZE];

/*!
 * @notsa
 * @brief Ref to a string (or text label) inside the script.
 * @brief It's exactly like `string_view`, *but* the data it points to is mutable.
 * @brief If you don't need mutability, use `string_view`
 */
struct StringRef {
    StringRef() = default;
    StringRef(char* str, size_t len, size_t cap) :
        Data{ str },
        Length{ (uint8_t)(len) },
        Cap{ (uint8_t)(cap) }
    {
        assert(len < MAX_STRING_SIZE);
        assert(cap < MAX_STRING_SIZE + 1);
    }
    explicit StringRef(ShortString& str) :
        StringRef{ &str[0], strlen(str), sizeof(str) }
    {
    }
    explicit StringRef(LongString& str) :
        StringRef{ &str[0], strlen(str), sizeof(str) }
    {
    }

    /* support for `notsa::ci_string_view`, `std::string_view` */
    template<typename Traits>
    operator std::basic_string_view<char, Traits>() const {
        return { Data, Length };
    }

    constexpr bool IsNullTerminated() const {
        return Data[Length] == '\0';
    }

public:
    char*   Data{};   //!< Pointer to the string (This points to a memory location inside the script, so be careful)
    uint8_t Length{}; //!< Length of the string (not including the null terminator)
    uint8_t Cap{};    //!< Buffer capacity
};

/** See https://gtamods.com/wiki/SCM_Instruction#Arrays */
struct ArrayAccess {
    enum class ElementType : uint8_t {
        INT,
        FLOAT,
        STRING_SHORT,
        STRING_LONG,
    };
    template<typename T>
    static constexpr auto GetElementTypeOf() {
        if constexpr (std::is_integral_v<T>) {
            return scm::ArrayAccess::ElementType::INT;
        } else if constexpr (std::is_same_v<T, float>) {
            return scm::ArrayAccess::ElementType::FLOAT;
        } else if constexpr (std::is_same_v<T, ShortString>) {
            return scm::ArrayAccess::ElementType::STRING_SHORT;
        } else if constexpr (std::is_same_v<T, LongString>) {
            return scm::ArrayAccess::ElementType::STRING_LONG;
        }
    }

    /** Array base offset (Address of the first value) */
    uint16_t ArrayBase;
    /** Location of the index variable used to access the array (May be a global or local variable, see `IdxVarIsGlobal`) */
    uint16_t IdxVarLoc;
    /** Array total size (length) (NOTE: Yes, it's unsigned!) */
    uint8_t ArraySize;
    /** Array elements type */
    ElementType ElemType: 7;
    /** Index is a global variable (true) or a local variable (false) */
    bool IdxVarIsGlobal: 1;
};

/** Represents an instruction, see https://gtamods.com/wiki/SCM_Instruction#Instruction_format */
struct Instruction {
    /** The command */
    eScriptCommands Command : 15;
    /** Whenever the boolean return value of the command should be negated */
    uint16_t NotFlag : 1;
};

/** Location of a variable */
using VarLoc = uint16_t;
}; // namespace scm

class CRunningScript {
public:
    /*!
     * Needed for compound if statements.
     * Basically, an `if` translates to:
     * - `COMMAND_ANDOR` followed by a parameter that encodes:
     *   1. the number of conditions = `n` (max 8)
     *   2. logical operation between conditions (AND/OR, hence the command name)
     * - `n` commands that update the conditional flag
     *
     * For instance `if ($A > 0 && $B > 0 && $C > 0)` would generate:
     *
     * ```
     * COMMAND_ANDOR                          ANDS_2 // (three conditions joined by AND)
     * COMMAND_IS_INT_VAR_GREATER_THAN_NUMBER $A 0
     * COMMAND_IS_INT_VAR_GREATER_THAN_NUMBER $B 0
     * COMMAND_IS_INT_VAR_GREATER_THAN_NUMBER $C 0
     * ```
     *
     * Each time a condition is tested, the result is AND/OR'd with the previous
     * result and the ANDOR state is decremented until it reaches the lower bound,
     * meaning that all conditions were tested.
     */
    enum LogicalOpType {
        ANDOR_NONE = 0,
        ANDS_1 = 1,
        ANDS_2,
        ANDS_3,
        ANDS_4,
        ANDS_5,
        ANDS_6,
        ANDS_7,
        ANDS_8,
        ORS_1 = 21,
        ORS_2,
        ORS_3,
        ORS_4,
        ORS_5,
        ORS_6,
        ORS_7,
        ORS_8
    };

public:
    CRunningScript*                                           m_pNext, *m_pPrev;    //< Linked list shit
    scm::ShortString                                          m_szName;             //< Name of the script
    uint8_t*                                                  m_BaseIP;             //< Base instruction pointer
    uint8_t*                                                  m_IP;                 //< current instruction pointer
    std::array<uint8_t*, MAX_STACK_DEPTH>                     m_IPStack;            //< Stack of instruction pointers (Usually saved on function call, then popped on return)
    uint16_t                                                  m_StackDepth;         //< Depth (size) of the stack
    std::array<tScriptParam, MAX_LOCAL_VARS + MAX_NUM_TIMERS> m_LocalVars;          //< This script's local variables and timers
    bool                                                      m_IsActive;           //< Is the script active (Unsure)
    bool                                                      m_CondResult;         //< (See `COMMAND_GOTO_IF_FALSE`) (Unsure)
    bool                                                      m_UsesMissionCleanup; //< If mission cleanup is needed after this script has finished
    bool                                                      m_IsExternal;
    bool                                                      m_IsTextBlockOverride;
    int8_t                                                    m_ExternalType;
    int32_t                                                   m_WakeTime;   //< Used for sleep-like commands (like `COMMAND_WAIT`) - The script halts execution until the time is reached
    uint16_t                                                  m_AndOrState; //< Next logical OP type (See `COMMAND_ANDOR`)
    bool                                                      m_NotFlag;    //< Boolean value returned by the called command should be negated
    bool                                                      m_IsDeathArrestCheckEnabled;
    bool                                                      m_DoneDeathArrest;
    int32_t                                                   m_SceneSkipIP;                     //< IP to use to skip the cutscene (?)
    bool                                                      m_ThisMustBeTheOnlyMissionRunning; //< Is (this script) a mission script

public:
    using CommandHandlerFn_t    = OpcodeResult(CRunningScript::*)(int32_t);
    using CommandHandlerTable_t = std::array<CommandHandlerFn_t, 27>;

    // Static data: gta-reversed bound s_OriginalCommandHandlerTable and
    // ScriptParams to fixed GTA SA 1.0 addresses via StaticRef<T>(addr).
    // Replaced with plain static members; original addresses kept as comments.
    // Definitions in CRunningScript.cpp. TODO: re-resolve for the clean-room build.
    static CommandHandlerTable_t s_OriginalCommandHandlerTable; // 0x8A6168
    static std::array<tScriptParam, 32> ScriptParams;           // 0xA43C78

    // NOTSA: We need to provide our own string buffer for arguments parser - the initial assumption that we can use string_view pointed at instruction pointer didn't work in the end.
    // it's not always null terminated so we have to provide our own buffer for the text. I'm eyeballing the max size so this needs to be enough for now. I can't find any command
    // using as many string at once, so 16 buffers is plenty. The most i can find is SET_MENU_COLUMN which reads 13 different strings
    static std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> ScriptArgCharBuffers;
    static uint8_t ScriptArgCharNextFreeBuffer;

public:
    void Init();

    void PlayAnimScriptCommand(int32_t commandId);

    void LocateCarCommand(int32_t commandId);
    void LocateCharCommand(int32_t commandId);
    void LocateObjectCommand(int32_t commandId);
    void LocateCharCarCommand(int32_t commandId);
    void LocateCharCharCommand(int32_t commandId);
    void LocateCharObjectCommand(int32_t commandId);

    void CarInAreaCheckCommand(int32_t commandId);
    void CharInAreaCheckCommand(int32_t commandId);
    void ObjectInAreaCheckCommand(int32_t commandId);

    void CharInAngledAreaCheckCommand(int32_t commandId);
    void FlameInAngledAreaCheckCommand(int32_t commandId);
    void ObjectInAngledAreaCheckCommand(int32_t commandId);

    void    CollectParameters(int16_t count);
    int32_t CollectNextParameterWithoutIncreasingPC();
    void    StoreParameters(int16_t count);

    void ReadArrayInformation(int32_t updateIp, uint16_t* outArrVarOffset, int32_t* outArrElemIdx);
    void ReadParametersForNewlyStartedScript(CRunningScript* newScript);
    void ReadTextLabelFromScript(char* buffer, uint8_t nBufferLength);
    void GetCorrectPedModelIndexForEmergencyServiceType(ePedType pedType, uint32_t* typeSpecificModelId);
    int16_t GetPadState(uint16_t playerIndex, eButtonId buttonId);

    tScriptParam* GetPointerToLocalVariable(int32_t loc);
    tScriptParam* GetPointerToLocalArrayElement(int32_t arrVarOffset, uint16_t arrElemIdx, uint8_t arrayEntriesSizeAsParams);
    tScriptParam* GetPointerToScriptVariable(eScriptVariableType variableType);

    tScriptParam* GetPointerToGlobalArrayElement(int32_t arrBase, uint16_t arrIdx, uint8_t arrayEntriesSizeAsParams);
    tScriptParam* GetPointerToGlobalVariable(int32_t varId);
    uint16_t      GetIndexOfGlobalVariable();

    void DoDeathArrestCheck(); // original name DoDeatharrestCheck

    void SetCharCoordinates(CPed& ped, CVector posn, bool warpGang, bool offset);
    void GivePedScriptedTask(int32_t pedHandle, CTask* task, int32_t opcode);
    void GivePedScriptedTask(CPed* ped, CTask* task, int32_t opcode); // NOTSA overload

    void AddScriptToList(CRunningScript** queueList);
    void RemoveScriptFromList(CRunningScript** queueList);
    void ShutdownThisScript();

    bool IsPedDead(CPed* ped) const;
    bool ThisIsAValidRandomPed(ePedType pedType, bool civilian, bool gang, bool criminal);
    void ScriptTaskPickUpObject(int32_t commandId);

    void UpdateCompareFlag(bool state);
    void UpdatePC(int32_t newIP);

    OpcodeResult ProcessOneCommand();
    OpcodeResult Process();

    void SetName(const char* name)      { strcpy_s(m_szName, name); }
    void SetName(std::string_view name) { assert(name.size() < sizeof(m_szName)); strncpy_s(m_szName, name.data(), name.size()); }
    void SetBaseIp(uint8_t* ip)         { assert(ip); m_BaseIP = ip; }
    void SetCurrentIp(uint8_t* ip)      { assert(ip); m_IP = ip; }
    void SetActive(bool active)         { m_IsActive = active; }
    void SetExternal(bool external)     { m_IsExternal = external; }

    //! Highlight an important area 2D
    void HighlightImportantArea(CVector2D from, CVector2D to, float z = -100.f);

    //! Highlight an important area 2D
    void HighlightImportantArea(CRect area, float z = -100.f);

    //! Highlight an important area 3D
    void HighlightImportantArea(CVector from, CVector to);

    //! Refer to `IsPositionWithinQuad2D` for information on how this works
    void HighlightImportantAngledArea(uint32_t id, CVector2D a, CVector2D b, CVector2D c, CVector2D d);

    //! Get value at IP
    template<typename T>
    T& GetAtIPAs(bool updateIP = true, size_t sz = sizeof(T)) {
        T& ret = *reinterpret_cast<T*>(m_IP);
        if (updateIP) {
            m_IP += sz;
        }
        return ret;
    }

    //! Get local variable
    template<typename T>
    T& GetLocal(scm::VarLoc loc) {
        return reinterpret_cast<T&>(m_ThisMustBeTheOnlyMissionRunning ? CTheScripts::LocalVariablesForCurrentMission[loc] : m_LocalVars[loc]);
    }

    //! Get value from local array
    template<typename T>
    T& GetArrayLocal(scm::VarLoc base, size_t idx, size_t elemSizeInDWords = std::min<size_t>(1, sizeof(T) / sizeof(int32_t))) {
        return GetLocal<T>((scm::VarLoc)(base + idx * elemSizeInDWords));
    }

    //! Get global variable
    template<typename T>
    T& GetGlobal(scm::VarLoc loc) {
        return reinterpret_cast<T&>(CTheScripts::ScriptSpace[loc]);
    }

    //! Get value from global array
    template<typename T>
    T& GetArrayGlobal(scm::VarLoc base, size_t idx, size_t elemSizeInDWords = std::max<size_t>(1, sizeof(T) / sizeof(int32_t))) {
        return GetGlobal<T>((scm::VarLoc)(base + idx * elemSizeInDWords * sizeof(int32_t)));
    }

    //! Perform array access (Increments IP)
    template<typename T>
    T& GetAtIPFromArray(bool isGlobalArray) {
        const auto op = GetAtIPAs<scm::ArrayAccess>();
        VERIFY(op.ElemType == scm::ArrayAccess::GetElementTypeOf<T>());
        const auto idx = op.IdxVarIsGlobal
            ? GetGlobal<int32_t>(op.IdxVarLoc)
            : GetLocal<int32_t>(op.IdxVarLoc);
        VERIFY(idx >= 0 && idx < op.ArraySize);
        return isGlobalArray
            ? GetArrayGlobal<T>(op.ArrayBase, (scm::VarLoc)(idx))
            : GetArrayLocal<T>(op.ArrayBase, (scm::VarLoc)(idx));
    }

    //! Return the custom command handler of a function (or null) as a reference
    // TODO: NOTSA custom-command infrastructure; full type ported with the command-parser subsystem.
    static notsa::script::CommandHandlerFunction& CustomCommandHandlerOf(scm::eScriptCommands command); // Returning a ref here for convenience (instead of having to make a `Set` function too)

private:
    void ResetIP();

    // Opcode dispatch blocks: s_OriginalCommandHandlerTable[command / 100]
    // selects one of these; each handles 100 opcodes. Only
    // ProcessCommands0To99 is implemented (core VM opcodes); the rest are
    // TODO stubs in CRunningScript.cpp.
    OpcodeResult ProcessCommands0To99(int32_t command);
    OpcodeResult ProcessCommands100To199(int32_t command);
    OpcodeResult ProcessCommands200To299(int32_t command);
    OpcodeResult ProcessCommands300To399(int32_t command);
    OpcodeResult ProcessCommands400To499(int32_t command);
    OpcodeResult ProcessCommands500To599(int32_t command);
    OpcodeResult ProcessCommands600To699(int32_t command);
    OpcodeResult ProcessCommands700To799(int32_t command);
    OpcodeResult ProcessCommands800To899(int32_t command);
    OpcodeResult ProcessCommands900To999(int32_t command);
    OpcodeResult ProcessCommands1000To1099(int32_t command);
    OpcodeResult ProcessCommands1100To1199(int32_t command);
    OpcodeResult ProcessCommands1200To1299(int32_t command);
    OpcodeResult ProcessCommands1300To1399(int32_t command);
    OpcodeResult ProcessCommands1400To1499(int32_t command);
    OpcodeResult ProcessCommands1500To1599(int32_t command);
    OpcodeResult ProcessCommands1600To1699(int32_t command);
    OpcodeResult ProcessCommands1700To1799(int32_t command);
    OpcodeResult ProcessCommands1800To1899(int32_t command);
    OpcodeResult ProcessCommands1900To1999(int32_t command);
    OpcodeResult ProcessCommands2000To2099(int32_t command);
    OpcodeResult ProcessCommands2100To2199(int32_t command);
    OpcodeResult ProcessCommands2200To2299(int32_t command);
    OpcodeResult ProcessCommands2300To2399(int32_t command);
    OpcodeResult ProcessCommands2400To2499(int32_t command);
    OpcodeResult ProcessCommands2500To2599(int32_t command);
    OpcodeResult ProcessCommands2600To2699(int32_t command);
};

#if INTPTR_MAX == INT32_MAX
// TODO: verify once CTheScripts layout and tScriptParam are final.
static_assert(sizeof(CRunningScript) == 0xE0, "CRunningScript layout changed");
#endif
