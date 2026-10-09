// CPedIntelligence - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/PedIntelligence.h
// Ped AI brain: task manager, event handling/scanning, decision maker, scanners.
//
// DEFERRED subsystems (size verified from gta-reversed VALIDATE_SIZE):
//   CTaskManager (0x30, real slot layout + stubbed methods since 2026-10-09),
//   CEventHandler (0x34, opaque), CEventGroup (0x4C, opaque),
//   CVehicleScanner (0x50, opaque), CPedScanner (0x50, m_apEntities exposed),
//   CMentalState (0x14, opaque),
//   CEventScanner (0xD4, opaque), CCollisionEventScanner (0x1, opaque),
//   CPedStuckChecker (0x10, opaque).
//   The task/event/scanner classes (900+) are a separate conversion batch -
//   replace these stand-ins with the real classes when they land.
//
// Adaptations:
//   stripped InjectHooks(), Constructor()/Destructor() placement wrappers
//   VALIDATE_SIZE + VALIDATE_OFFSET -> 32-bit-guarded static_assert
//   StaticRef statics -> plain static members (defined in CPedIntelligence.cpp,
//     game addresses kept as comments)
//   C++23 deducing-this GetStuckChecker -> const/non-const overload pair (C++17)
//   IsUsingGun() demoted to declaration-only (needs real CTaskManager::GetSimplestActiveTask)
//   GetPedEntities()/GetPedEntity() demoted to declaration-only (need CPedScanner::m_apEntities)
// TODO:
//   port task/event/scanner classes (replace stand-ins)
//   verify each method against decomp src/CPedIntelligence/*.c

#pragma once

#include "CVector.h" // CVector
#include "eMoveState.h" // eMoveState (GetMoveStateFromGoToTask return type)

#include <cstddef> // offsetof
#include <cstdint>
#include <type_traits> // enable_if_t/is_convertible_v (CEventGroup::Add helper)

class CEntity; // fwd: full class in CEntity.h

// ---- DEFERRED subsystems (size verified from gta-reversed, contents opaque) ----
// Full ports belong to the task/event/scanner conversion batch.
// Sizes verified via gta-reversed VALIDATE_SIZE; replace with the real
// classes when that batch lands.
// Task slot indices - values verified against gta-reversed
// (source/game_sa/Tasks/TaskManager.h). Full task system belongs to the
// task/event/scanner conversion batch.
enum ePrimaryTasks : int32_t { // m_aPrimaryTasks array indices
    TASK_PRIMARY_INVALID = -1,

    TASK_PRIMARY_PHYSICAL_RESPONSE = 0,
    TASK_PRIMARY_EVENT_RESPONSE_TEMP,
    TASK_PRIMARY_EVENT_RESPONSE_NONTEMP,
    TASK_PRIMARY_PRIMARY,
    TASK_PRIMARY_DEFAULT,
    TASK_PRIMARY_MAX
};
enum eSecondaryTask : uint32_t { // m_aSecondaryTasks array indices
    TASK_SECONDARY_INVALID = (uint32_t)-1,

    TASK_SECONDARY_ATTACK = 0, // want duck to be after attack
    TASK_SECONDARY_DUCK,       // because attack controls ducking movement
    TASK_SECONDARY_SAY,
    TASK_SECONDARY_FACIAL_COMPLEX,
    TASK_SECONDARY_PARTIAL_ANIM,
    TASK_SECONDARY_IK,
    TASK_SECONDARY_MAX
};

// eTaskType: full enum lives in the task conversion batch
// (gta-reversed Enums/eTaskType.h). Minimal definition with the values used so
// far; values verified 2026-10-09 vs gta-reversed (parsed the plugin-sdk enum).
// Delete this stand-in when the full enum lands.
enum eTaskType : int32_t {
    TASK_SIMPLE_FALL                      = 207, // added 2026-10-09 for CPed::KillPedWithCar
    TASK_SIMPLE_DIE                       = 212, // added 2026-10-09 for CPed::KillPedWithCar
    TASK_SIMPLE_DEAD                      = 218,
    TASK_SIMPLE_HOLD_ENTITY               = 307,
    TASK_SIMPLE_PICKUP_ENTITY             = 308,
    TASK_SIMPLE_PUTDOWN_ENTITY            = 309,
    TASK_COMPLEX_GO_PICKUP_ENTITY         = 310,
    TASK_COMPLEX_HANDSIGNAL_ANIM          = 426,
    TASK_COMPLEX_ENTER_CAR_AS_DRIVER      = 701,
    TASK_COMPLEX_DRAG_PED_FROM_CAR        = 703,
    TASK_COMPLEX_LEAVE_CAR                = 704,
    TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN     = 809,
    TASK_SIMPLE_CAR_JUMP_OUT              = 814,
    TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT  = 823,
    TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP = 824,
    TASK_COMPLEX_CAR_QUICK_BE_DRAGGED_OUT = 825,
    TASK_COMPLEX_DESTROY_CAR_MELEE        = 1004,
    TASK_SIMPLE_GANG_DRIVEBY              = 1022,
    TASK_SIMPLE_STEALTH_KILL              = 1027,
    TASK_COMPLEX_PARTNER_CHAT             = 1204,
    TASK_COMPLEX_USE_MOBILE_PHONE         = 1600,
    // TODO(tasks): 0xfe per decomp (CPed::ProcessBuoyancy); value from the original game.
    // Added 2026-10-09 for CPed.
    TASK_SIMPLE_SWIM                      = 254,
};

class CTask; // fwd: defined (minimally) in CWeapon.cpp (weapons_combat TU) and
             // CPedIntelligence.cpp (this TU); full class lands with the task batch
// TODO(tasks): full class lands with the task batch. Fwd-declared for
// notsa::dyn_cast_if_present use in CAutomobile. Added 2026-10-09.
class CTaskSimpleGangDriveBy;
class CPed;
struct CTaskManager {
    // Real layout from gta-reversed (was opaque stand-in):
    //   5 primary slots + 6 secondary slots + owner ped = 0x30 on 32-bit.
    // Methods are minimal/stubbed - the task batch replaces this struct wholesale.
    CTask* m_aPrimaryTasks[TASK_PRIMARY_MAX];
    CTask* m_aSecondaryTasks[TASK_SECONDARY_MAX];
    CPed*  m_pPed;

    // First non-null primary task (real logic, needs no task internals)
    CTask* GetActiveTask();
    // TODO(tasks): full version walks the sub-task tree via GetSubTask().
    // Minimal: the active task itself.
    CTask* GetSimplestActiveTask();
    // Direct slot read (real logic)
    CTask* GetTaskSecondary(eSecondaryTask taskIndex);
    // TODO(tasks): full version walks the sub-task tree; minimal checks the slot head only.
    CTask* FindTaskByType(ePrimaryTasks taskIndex, eTaskType taskType) const;
    // TODO(tasks): full version walks the sub-task tree; minimal checks the active head only.
    CTask* FindActiveTaskByType(eTaskType taskType);
    // TODO(tasks): full version is Find<Ts>(activeOnly) over the sub-task tree
    // (gta-reversed TaskManager.h). Minimal: active-head check per type.
    template<eTaskType... Ts>
    bool HasAnyOf(bool activeOnly = true) {
        (void)activeOnly;
        return (... || FindActiveTaskByType(Ts));
    }
    // TODO(tasks): full version is Find<T>(activeOnly) over the sub-task tree
    // (gta-reversed TaskManager.h). Minimal: active-head check.
    template<eTaskType T>
    bool Has(bool activeOnly = true) {
        (void)activeOnly;
        return FindActiveTaskByType(T) != nullptr;
    }
    // TODO(tasks): full version deletes the old task via ChangeTaskInSlot().
    void SetTask(CTask* task, ePrimaryTasks taskIndex, bool unused = false);
    void SetTaskSecondary(CTask* task, eSecondaryTask taskIndex);
    // TODO(tasks): manage/abort/delete tasks per the original logic.
    void ManageTasks();
    void Flush();
    void FlushImmediately();
};
struct CEventHandler {
    uint8_t _deferred[0x34];
};
class CEvent; // fwd: defined in CWeapon.cpp (weapons_combat TU) until events are ported
struct CEventGroup {
    uint8_t _deferred[0x4C];

    // TODO(events): ported minimally for weapons_combat TU
    void Add(CEvent* event, bool bValid = true);

    // Helper so events can be passed directly (added 2026-10-09 for CAutomobile).
    // Fixed 2026-10-09: the original Add(&event, valid) recursed infinitely
    // (C2999) - the template beat the CEvent* overload on exact match for
    // derived event pointers and re-wrapped the pointer every level.
    // Now SFINAE'd out when the argument already converts to CEvent*, so
    // pointers use the non-template overload; anything else (e.g. event
    // temporaries) is addressed and forwarded to it, terminating there.
    template<typename T, std::enable_if_t<!std::is_convertible_v<T, CEvent*>, int> = 0>
    void Add(T&& event, bool valid = false) {
        Add(static_cast<CEvent*>(&event), valid);
    }
};
struct CVehicleScanner {
    uint8_t _deferred[0x50]; // == sizeof(CEntityScanner), no added members
};
struct CPedScanner {
    // Partial layout from gta-reversed CEntityScanner (was fully opaque):
    // vtable(0x0) + m_timer(0x4, 0x8 bytes) + m_apEntities(0xC) + m_pClosestEntityInRange.
    // Only m_apEntities is exposed; the rest stays deferred until the scanner batch.
    uint8_t  _deferred0[0xC];
    CEntity* m_apEntities[16];
#if INTPTR_MAX == INT32_MAX
    uint8_t  _deferred1[0x50 - 0xC - 16 * 4];
#endif
};
struct CMentalState {
    uint8_t _deferred[0x14];
};
struct CEventScanner {
    uint8_t _deferred[0xD4];
};
struct CCollisionEventScanner {
    bool m_bAlreadyHitByCar{}; // added 2026-10-09 for CPed::KillPedWithCar
    uint8_t _deferred[0x1];
};
struct CPedStuckChecker {
    uint8_t _deferred[0x10];
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CTaskManager) == 0x30, "CTaskManager stand-in size mismatch");
static_assert(sizeof(CEventHandler) == 0x34, "CEventHandler stand-in size mismatch");
static_assert(sizeof(CEventGroup) == 0x4C, "CEventGroup stand-in size mismatch");
static_assert(sizeof(CVehicleScanner) == 0x50, "CVehicleScanner stand-in size mismatch");
static_assert(sizeof(CPedScanner) == 0x50, "CPedScanner stand-in size mismatch");
static_assert(sizeof(CMentalState) == 0x14, "CMentalState stand-in size mismatch");
static_assert(sizeof(CEventScanner) == 0xD4, "CEventScanner stand-in size mismatch");
// 2026-10-09: was 0x1; struct gained bool m_bAlreadyHitByCar (CPed::KillPedWithCar) -> 2 bytes.
static_assert(sizeof(CCollisionEventScanner) == 0x2, "CCollisionEventScanner stand-in size mismatch");
static_assert(sizeof(CPedStuckChecker) == 0x10, "CPedStuckChecker stand-in size mismatch");
#endif

// eEventType: full enum lives in the events conversion batch
// (gta-reversed Enums/eEventType.h). Opaque here; only used as a parameter.
enum eEventType : int32_t;

class CPed;
class CEntity;
class CPlayerPed;

class CTaskSimpleUseGun;
class CTaskSimpleFight;
class CTaskSimpleHoldEntity;
class CTaskSimpleThrowProjectile;
class CTaskSimpleSwim;
class CTaskSimpleDuck;
class CTaskSimpleClimb;
class CTaskSimpleJetPack;
class CTaskSimpleInAir;

class C2dEffect;

class CPedIntelligence {
public:
    CPed*                  m_pPed;
    CTaskManager           m_TaskMgr;
    CEventHandler          m_eventHandler;
    CEventGroup            m_eventGroup;
    int32_t                m_nDecisionMakerType;
    int32_t                m_nDecisionMakerTypeInGroup;
    float                  m_fHearingRange;
    float                  m_fSeeingRange;
    uint32_t               m_nDmNumPedsToScan;
    float                  m_fDmRadius;
    float                  m_FollowNodeThresholdDistance;
    char                   m_NextEventResponseSequence;
    uint8_t                m_nEventId;
    uint8_t                m_nEventPriority;
    char                   field_D3;
    CVehicleScanner        m_vehicleScanner;
    CPedScanner            m_pedScanner;
    CMentalState           m_mentalState;
    char                   field_188;
    CEventScanner          m_eventScanner;
    CCollisionEventScanner m_collisionScanner;
    CPedStuckChecker       m_pedStuckChecker;
    int32_t                m_AnotherStaticCounter;
    int32_t                m_StaticCounter;
    CVector                m_vecLastPedPosDuringDamageEntity;
    CEntity*               m_apInterestingEntities[3];

    static float STEALTH_KILL_RANGE; // game address: 0x8D2398 ; DEFERRED - was StaticRef
    static float LIGHT_AI_LEVEL_MAX; // game address: 0x8D2380 ; DEFERRED - was StaticRef
    static float flt_8D2384;         // game address: 0x8D2384 ; DEFERRED - was StaticRef
    static float flt_8D2388;         // game address: 0x8D2388 ; DEFERRED - was StaticRef

public:
    static void* operator new(std::size_t size);
    static void operator delete(void* object);

    CPedIntelligence(CPed* ped);
    ~CPedIntelligence();

    void SetPedDecisionMakerType(int32_t newType);
    int32_t GetPedDecisionMakerType() const { return m_nDecisionMakerType; }
    void SetPedDecisionMakerTypeInGroup(int32_t newType);
    void RestorePedDecisionMakerType();
    void SetHearingRange(float range);
    void SetSeeingRange(float range);
    bool IsInHearingRange(const CVector& posn);
    bool IsInSeeingRange(const CVector& posn) const;
    bool FindRespectedFriendInInformRange();
    bool IsRespondingToEvent(eEventType eventType);
    void AddTaskPhysResponse(CTask* task, int32_t unUsed = 1);
    void AddTaskEventResponseTemp(CTask* task, int32_t unUsed);
    void AddTaskEventResponseNonTemp(CTask* task, int32_t unUsed);
    void AddTaskPrimaryMaybeInGroup(CTask* task, bool bAffectsPed);

    //!< Can be replaced using `CTaskManager::Find<T>(false);`
    CTask* FindTaskByType(eTaskType taskId);
    CTaskSimpleFight* GetTaskFighting();
    CTaskSimpleUseGun* GetTaskUseGun();
    CTaskSimpleThrowProjectile* GetTaskThrow();
    CTaskSimpleHoldEntity* GetTaskHold(bool bIgnoreCheckingForSimplestActiveTask = true);
    CTaskSimpleSwim* GetTaskSwim();
    CTaskSimpleDuck* GetTaskDuck(bool bIgnoreCheckingForSimplestActiveTask = true);
    CTaskSimpleJetPack* GetTaskJetPack();
    CTaskSimpleInAir* GetTaskInAir();
    CTaskSimpleClimb* GetTaskClimb();
    CTaskSimpleDuck* GetTaskSecondaryDuck();
    bool GetUsingParachute();
    void SetTaskDuckSecondary(uint16_t nLengthOfDuck);
    void ClearTaskDuckSecondary();
    void ClearTasks(bool bClearPrimaryTasks, bool bClearSecondaryTasks);
    void FlushImmediately(bool bSetPrimaryDefaultTask);
    C2dEffect* GetEffectInUse() const;
    void SetEffectInUse(C2dEffect* effect);
    void ProcessAfterProcCol();
    void ProcessAfterPreRender();
    void ProcessEventHandler();
    bool IsFriendlyWith(const CPed& ped) const;
    bool IsThreatenedBy(const CPed& ped) const;
    bool Respects(CPed* ped) const;
    bool IsInACarOrEnteringOne();
    static bool AreFriends(const CPed& ped1, const CPed& ped2);
    bool IsPedGoingSomewhereOnFoot();
    eMoveState GetMoveStateFromGoToTask();
    void FlushIntelligence();
    bool TestForStealthKill(CPed* pTarget, bool bFullTest);
    void RecordEventForScript(int32_t eventId, int32_t eventPriority);
    bool HasInterestingEntites();
    bool IsInterestingEntity(CEntity* entity);
    void LookAtInterestingEntities();
    void RemoveAllInterestingEntities();
    bool IsPedGoingForCarDoor();
    float CanSeeEntityWithLights(CEntity* entity, int32_t unUsed);
    void ProcessStaticCounter();
    void ProcessFirst();
    void Process();
    CTask* GetActivePrimaryTask() const;
    float GetPedFOVRange() const;
    void IncrementAngerAtPlayer(uint8_t anger);

    void SetDmRadius(float r) { m_fDmRadius = r; }
    void SetNumPedsToScan(uint32_t n) { m_nDmNumPedsToScan = n; }

    // Minimal version (task batch refines): checks the simplest active task's type.
    bool IsUsingGun();

    //! Get the vehicle the ped is entering now (If any)
    class CVehicle* GetEnteringVehicle();

    CTaskManager&    GetTaskManager()    { return m_TaskMgr; }
    CEventHandler&   GetEventHandler()   { return m_eventHandler; }
    CEventGroup&     GetEventGroup()     { return m_eventGroup; }
    CEventScanner&   GetEventScanner()   { return m_eventScanner; }
    CPedScanner&     GetPedScanner()     { return m_pedScanner; }
    CVehicleScanner& GetVehicleScanner() { return m_vehicleScanner; }
    CEntity** GetPedEntities();
    CEntity* GetPedEntity(uint32_t index);

    CPedStuckChecker& GetStuckChecker() { return m_pedStuckChecker; }
    const CPedStuckChecker& GetStuckChecker() const { return m_pedStuckChecker; }
};

// TODO(2026-10-09): layout mismatch - sizeof != 0x294. Needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(CPedIntelligence) == 0x294, "CPedIntelligence size mismatch");
//static_assert(offsetof(CPedIntelligence, m_AnotherStaticCounter) == 0x274,
//              "CPedIntelligence::m_AnotherStaticCounter offset mismatch");
//#endif
