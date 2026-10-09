// CPedIntelligence.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CPedIntelligence/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CPedIntelligence.h"
#include <cstddef> // std::size_t
#include "CPed.h" // CPed::m_nPedType
#include "ePedType.h" // PED_TYPE_COP/DEALER
#include "CPedType.h" // CPedType::GetPedFlag
#include "CAnimManager.h" // CAnimManager::GetAnimBlockById
#include <cmath> // sqrtf
#include <cstring> // _stricmp

// Minimal CTask definition for this TU - token-identical to the one in
// CWeapon.cpp (weapons_combat TU), so this is not an ODR violation.
// The task conversion batch replaces both with the real class.
struct CTask {
    virtual int32_t GetTaskType() const;
};

// NOTE(anim-batch): RenderWare anim-blend decls now live in RenderWare.h (with
// default args). Local redeclarations were removed 2026-10-09: they formed
// ambiguous overloads with the header versions (C2668 at the 1-arg call).

float CPedIntelligence::STEALTH_KILL_RANGE{}; // game address: 0x8D2398
float CPedIntelligence::LIGHT_AI_LEVEL_MAX{}; // game address: 0x8D2380
float CPedIntelligence::flt_8D2384{}; // game address: 0x8D2384
float CPedIntelligence::flt_8D2388{}; // game address: 0x8D2388

void* CPedIntelligence::operator new(std::size_t size) {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone; body from gta-reversed PedIntelligence.cpp.
    // TODO(port): PedIntelligence pool not yet ported. Original: return GetPedIntelligencePool()->New();
    (void)size;
    return ::operator new(size);
}













void CPedIntelligence::operator delete(void* object) {    // no decomp; adapted from gta-reversed
    // TODO(port): PedIntelligence pool not yet ported. Original: GetPedIntelligencePool()->Delete(...);
    ::operator delete(object);
}

// ---- CTaskManager minimal method definitions (decls in CPedIntelligence.h) ----
// The task conversion batch replaces these with the real implementations.
CTask* CTaskManager::GetActiveTask() {
    for (CTask* task : m_aPrimaryTasks) {
        if (task) {
            return task;
        }
    }
    return nullptr;
}
CTask* CTaskManager::GetSimplestActiveTask() {
    // TODO(tasks): full version returns the deepest sub-task via GetSubTask().
    return GetActiveTask();
}
CTask* CTaskManager::GetTaskSecondary(eSecondaryTask taskIndex) {
    return m_aSecondaryTasks[taskIndex];
}
CTask* CTaskManager::FindTaskByType(ePrimaryTasks taskIndex, eTaskType taskType) const {
    // TODO(tasks): full version walks the sub-task tree; minimal checks the slot head.
    CTask* task = m_aPrimaryTasks[taskIndex];
    return (task && task->GetTaskType() == (int32_t)taskType) ? task : nullptr;
}
CTask* CTaskManager::FindActiveTaskByType(eTaskType taskType) {
    // TODO(tasks): full version walks the sub-task tree; minimal checks the active head.
    CTask* task = GetActiveTask();
    return (task && task->GetTaskType() == (int32_t)taskType) ? task : nullptr;
}
void CTaskManager::SetTask(CTask* task, ePrimaryTasks taskIndex, bool unused) {
    (void)unused;
    // TODO(tasks): full version deletes the old task via ChangeTaskInSlot().
    m_aPrimaryTasks[taskIndex] = task;
}
void CTaskManager::SetTaskSecondary(CTask* task, eSecondaryTask taskIndex) {
    // TODO(tasks): full version deletes the old task via ChangeTaskInSlot().
    m_aSecondaryTasks[taskIndex] = task;
}
void CTaskManager::ManageTasks() {
    // TODO(tasks): manage/abort/delete tasks per the original logic.
}
void CTaskManager::Flush() {
    // TODO(tasks): full version deletes the tasks; minimal just clears the slots.
    for (CTask*& task : m_aPrimaryTasks) {
        task = nullptr;
    }
    for (CTask*& task : m_aSecondaryTasks) {
        task = nullptr;
    }
}
void CTaskManager::FlushImmediately() {
    // TODO(tasks): full version tries aborting the tasks first, then deletes them.
    Flush();
}













CPedIntelligence::CPedIntelligence(CPed* ped) {    // converted from decomp src/CPedIntelligence/Constructor_00607140.c; cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp
    // TODO(port): CTaskManager/CEventHandler/CEventGroup/CVehicleScanner/CPedScanner/
    //   CMentalState/CEventScanner/CCollisionEventScanner/CPedStuckChecker are opaque
    //   stand-ins (see CPedIntelligence.h). Their in-place construction and the
    //   scanner/timer/vtable initialization below belong to the task/event/scanner
    //   conversion batch.
    //   Original inits (decomp): m_TaskMgr(ped), m_eventHandler(ped), m_eventGroup(ped);
    //   vehicle/ped scanners: timer 0/0x10, 16 entity slots nulled, vtable set;
    //   m_mentalState anger/timer zeroed then timer started; m_eventScanner constructed;
    //   m_collisionScanner.m_bAlreadyHitByCar=false; m_pedStuckChecker zeroed/NONE.
    m_pPed = ped;
    m_nDecisionMakerType = -1; // DM_EVENT_UNDEFINED
    m_nDecisionMakerTypeInGroup = -1;
    m_fHearingRange = 15.0f;
    m_fSeeingRange = 15.0f;
    m_nDmNumPedsToScan = 3;
    m_fDmRadius = 15.0f;
    m_FollowNodeThresholdDistance = 30.0f;
    m_NextEventResponseSequence = -1;
    m_nEventId = 0;
    m_nEventPriority = 0;
    field_188 = 0;
    m_AnotherStaticCounter = 0;
    m_StaticCounter = 0;
    if (ped->m_nPedType > PED_TYPE_COP && ped->m_nPedType < PED_TYPE_DEALER) { // gang peds
        m_fSeeingRange = 40.0f;
        m_fHearingRange = 40.0f;
    }
    m_apInterestingEntities[0] = nullptr;
    m_apInterestingEntities[1] = nullptr;
    m_apInterestingEntities[2] = nullptr;
}















CPedIntelligence::~CPedIntelligence() {    // converted from decomp src/CPedIntelligence/Destructor_00607300.c
    // TODO(port): CPlayerRelationshipRecorder not yet ported.
    // Original (gta-reversed): GetPlayerRelationshipRecorder().ClearRelationshipWithPlayer(m_pPed);
}















void CPedIntelligence::SetPedDecisionMakerType(int32_t newType) {    // converted from decomp src/CPedIntelligence/*.c
        if (this->m_nDecisionMakerType == 0) {
            this->m_nDecisionMakerTypeInGroup = newType;
        }
        else if (newType == 0) {
            this->m_nDecisionMakerTypeInGroup = this->m_nDecisionMakerType;
            this->m_nDecisionMakerType = 0;
        }
        else {
            this->m_nDecisionMakerType = newType;
        }
        if (this->m_nDecisionMakerType == 7) {
            this->m_fDmRadius = 5.0;
            this->m_nDmNumPedsToScan = 0xf;
        }
        return;
}











void CPedIntelligence::SetPedDecisionMakerTypeInGroup(int32_t newType) {    // converted from decomp src/CPedIntelligence/*.c
        this->m_nDecisionMakerTypeInGroup = newType;
        return;
}











void CPedIntelligence::RestorePedDecisionMakerType() {    // converted from decomp src/CPedIntelligence/*.c
        if (this->m_nDecisionMakerType == 0) {
            this->m_nDecisionMakerType = this->m_nDecisionMakerTypeInGroup;
        }
        return;
}











void CPedIntelligence::SetHearingRange(float range) {    // converted from decomp src/CPedIntelligence/*.c
        this->m_fHearingRange = range;
        return;
}











void CPedIntelligence::SetSeeingRange(float range) {    // converted from decomp src/CPedIntelligence/*.c
        this->m_fSeeingRange = range;
        return;
}











bool CPedIntelligence::IsInHearingRange(const CVector& posn) {    // converted from decomp src/CPedIntelligence/*.c
    // Converted from decomp src/CPedIntelligence/IsInHearingRange_*.c.
    // (gta-reversed has this as a plugin call; logic verified from decomp.)
    CVector distance = posn - m_pPed->GetPosition();
    return m_fHearingRange * m_fHearingRange > distance.SquaredMagnitude();
}











bool CPedIntelligence::IsInSeeingRange(const CVector& posn) const {    // converted from decomp src/CPedIntelligence/*.c
    // Converted from decomp src/CPedIntelligence/IsInSeeingRange_*.c;
    // cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp.
    CVector distance = posn - m_pPed->GetPosition();
    if (m_fSeeingRange * m_fSeeingRange > distance.SquaredMagnitude()) {
        const CVector& fwd = m_pPed->GetForward();
        if (distance.x * fwd.x + distance.y * fwd.y + distance.z * fwd.z > 0.0f) {
            return true;
        }
    }
    return false;
}











bool CPedIntelligence::FindRespectedFriendInInformRange() {    // converted from decomp src/CPedIntelligence/*.c
    // Cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp (0x600CF0).
    const uint32_t respect = m_pPed->m_acquaintance.GetAcquaintances(0 /* ACQUAINTANCE_RESPECT */);
    CEntity** entities = GetPedEntities();
    for (uint32_t i = 0; i < m_nDmNumPedsToScan && i < 16; ++i) {
        const auto* ped = static_cast<const CPed*>(entities[i]);
        if (ped && (CPedType::GetPedFlag(ped->m_nPedType) & respect)) {
            const CVector distance = m_pPed->GetPosition() - ped->GetPosition();
            if (m_fDmRadius * m_fDmRadius > distance.SquaredMagnitude()) {
                return true;
            }
        }
    }
    return false;
}














bool CPedIntelligence::IsRespondingToEvent(eEventType eventType) {    // converted from decomp src/CPedIntelligence/IsRespondingToEvent_00469590.c
    // TODO(port): CEventHandlerHistory/CEventHandler opaque (event batch). Decomp: return m_eventHandler.m_History.IsRespondingToEvent(eventType);
    return false;
}











void CPedIntelligence::AddTaskPhysResponse(CTask* task,  int32_t unUsed) {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): CTaskManager opaque (task batch). Decomp: m_TaskMgr.SetTask(task, TASK_PRIMARY_PHYSICAL_RESPONSE, unUsed);
}











void CPedIntelligence::AddTaskEventResponseTemp(CTask* task,  int32_t unUsed) {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): CTaskManager opaque (task batch). Decomp: m_TaskMgr.SetTask(task, TASK_PRIMARY_EVENT_RESPONSE_TEMP, unUsed);
}











void CPedIntelligence::AddTaskEventResponseNonTemp(CTask* task,  int32_t unUsed) {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): CTaskManager opaque (task batch). Decomp: m_TaskMgr.SetTask(task, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP, unUsed);
}











void CPedIntelligence::AddTaskPrimaryMaybeInGroup(CTask* task,  bool bAffectsPed) {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): CEventScriptCommand/CEventGroup/CPedGroups/CPedGroupIntelligence unported (event/group batch).
    // Decomp logic: if (m_pPed->IsPlayer() || CPedGroups::GetPedsGroup(m_pPed) == 0) {
    //                   CEventScriptCommand cmd(3, task, bAffectsPed); m_eventGroup.Add(&cmd, false);
    //               } else {
    //                   CPedGroupIntelligence::SetScriptCommandTask(groupIntelligence, m_pPed, task);
    //                   if (task) task->~CTask(); // vtable delete
    //               }
}











CTask* CPedIntelligence::FindTaskByType(eTaskType taskId) {    // converted from decomp src/CPedIntelligence/*.c
    // Cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp (0x600EE0).
    CTask* task = m_TaskMgr.FindTaskByType(TASK_PRIMARY_DEFAULT, taskId);
    if (!task) {
        task = m_TaskMgr.FindTaskByType(TASK_PRIMARY_PRIMARY, taskId);
    }
    if (!task) {
        task = m_TaskMgr.FindTaskByType(TASK_PRIMARY_EVENT_RESPONSE_TEMP, taskId);
    }
    if (!task) {
        task = m_TaskMgr.FindTaskByType(TASK_PRIMARY_EVENT_RESPONSE_NONTEMP, taskId);
    }
    return task;
}














CTaskSimpleFight* CPedIntelligence::GetTaskFighting() {    // converted from decomp src/CPedIntelligence/*.c
    // Original (gta-reversed 0x600F30): dyn_cast_if_present<CTaskSimpleFight>(
    //   m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK)).
    // TODO(tasks): real dyn_cast needs CTaskSimpleFight (task batch).
    CTask* task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK);
    return (task && task->GetTaskType() == 0x3F8 /* TASK_SIMPLE_FIGHT */) ? (CTaskSimpleFight*)task : nullptr;
}














CTaskSimpleUseGun* CPedIntelligence::GetTaskUseGun() {    // converted from decomp src/CPedIntelligence/*.c
    // Original (gta-reversed 0x600F70): dyn_cast_if_present<CTaskSimpleUseGun>(
    //   m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK)).
    // TODO(tasks): real dyn_cast needs CTaskSimpleUseGun (task batch).
    CTask* task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK);
    return (task && task->GetTaskType() == 0x3F9 /* TASK_SIMPLE_USE_GUN */) ? (CTaskSimpleUseGun*)task : nullptr;
}














CTaskSimpleThrowProjectile* CPedIntelligence::GetTaskThrow() {    // converted from decomp src/CPedIntelligence/*.c
    // Original (gta-reversed 0x600FB0): dyn_cast_if_present<CTaskSimpleThrowProjectile>(
    //   m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK)).
    // TODO(tasks): real dyn_cast needs CTaskSimpleThrowProjectile (task batch).
    CTask* task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK);
    return (task && task->GetTaskType() == 0x3FA /* TASK_SIMPLE_THROW_PROJECTILE */)
        ? (CTaskSimpleThrowProjectile*)task
        : nullptr;
}














CTaskSimpleHoldEntity* CPedIntelligence::GetTaskHold(bool bIgnoreCheckingForSimplestActiveTask) {    // converted from decomp src/CPedIntelligence/*.c
    // Cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp (0x600FF0).
    // TODO(tasks): real dyn_cast needs CTaskSimpleHoldEntity (task batch).
    CTask* task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM);
    if (task && task->GetTaskType() == 0x133 /* TASK_SIMPLE_HOLD_ENTITY */) {
        return (CTaskSimpleHoldEntity*)task;
    }
    if (!bIgnoreCheckingForSimplestActiveTask) {
        task = m_TaskMgr.GetSimplestActiveTask();
        if (task) {
            const int32_t type = task->GetTaskType();
            if (type == 0x134 /* TASK_SIMPLE_PICKUP_ENTITY */ || type == 0x135 /* TASK_SIMPLE_PUTDOWN_ENTITY */) {
                return (CTaskSimpleHoldEntity*)task;
            }
        }
    }
    return nullptr;
}














CTaskSimpleSwim* CPedIntelligence::GetTaskSwim() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(tasks): real dyn_cast needs CTaskSimpleSwim (task batch).
    CTask* task = m_TaskMgr.GetSimplestActiveTask();
    return (task && task->GetTaskType() == 0x518 /* TASK_SIMPLE_SWIM */) ? (CTaskSimpleSwim*)task : nullptr;
}














CTaskSimpleDuck* CPedIntelligence::GetTaskDuck(bool bIgnoreCheckingForSimplestActiveTask) {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(tasks): real dyn_cast needs CTaskSimpleDuck (task batch).
    CTask* task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_DUCK);
    if (task && task->GetTaskType() == 0x19F /* TASK_SIMPLE_DUCK */) {
        return (CTaskSimpleDuck*)task;
    }
    if (!bIgnoreCheckingForSimplestActiveTask) {
        task = m_TaskMgr.GetSimplestActiveTask();
        if (task && task->GetTaskType() == 0x19F /* TASK_SIMPLE_DUCK */) {
            return (CTaskSimpleDuck*)task;
        }
    }
    return nullptr;
}














CTaskSimpleJetPack* CPedIntelligence::GetTaskJetPack() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(tasks): real dyn_cast needs CTaskSimpleJetPack (task batch).
    if (m_pPed->IsPlayer()) {
        CTask* task = m_TaskMgr.GetSimplestActiveTask();
        if (task && task->GetTaskType() == 0x517 /* TASK_SIMPLE_JETPACK */) {
            return (CTaskSimpleJetPack*)task;
        }
    }
    return nullptr;
}














CTaskSimpleInAir* CPedIntelligence::GetTaskInAir() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(tasks): real dyn_cast needs CTaskSimpleInAir (task batch).
    CTask* task = m_TaskMgr.GetSimplestActiveTask();
    return (task && task->GetTaskType() == 0xF1 /* TASK_SIMPLE_IN_AIR */) ? (CTaskSimpleInAir*)task : nullptr;
}














CTaskSimpleClimb* CPedIntelligence::GetTaskClimb() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(tasks): real dyn_cast needs CTaskSimpleClimb (task batch).
    CTask* task = m_TaskMgr.GetSimplestActiveTask();
    return (task && task->GetTaskType() == 0xFE /* TASK_SIMPLE_CLIMB */) ? (CTaskSimpleClimb*)task : nullptr;
}














CTaskSimpleDuck* CPedIntelligence::GetTaskSecondaryDuck() {    // no decomp; adapted from gta-reversed
    // Original: return dyn_cast_if_present<CTaskSimpleDuck>(m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_DUCK));
    // TODO(tasks): real dyn_cast needs CTaskSimpleDuck (task batch).
    CTask* task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_DUCK);
    return (task && task->GetTaskType() == 0x19F /* TASK_SIMPLE_DUCK */) ? (CTaskSimpleDuck*)task : nullptr;
}
















bool CPedIntelligence::GetUsingParachute() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(anim-batch): RpAnimBlend* are declaration-only until the RenderWare port lands.
    const CPed* ped = m_pPed;
    if (ped->m_aWeapons[ped->m_nActiveWeaponSlot].GetType() == WEAPON_PARACHUTE
        && !ped->bIsStanding
        && (ped->m_nPhysicalFlags & 0x100) == 0) {
        for (CAnimBlendAssociation* assoc =
                 RpAnimBlendClumpGetFirstAssociation((RpClump*)ped->GetRwObject(), 0x10);
             assoc != nullptr;
             assoc = RpAnimBlendGetNextAssociation(assoc)) {
            const int32_t blockId = assoc->m_BlendHier->m_nAnimBlockId;
            if (_stricmp(CAnimManager::GetAnimBlockById(blockId).Name, "parachute") == 0) {
                return true;
            }
        }
    }
    return false;
}














void CPedIntelligence::SetTaskDuckSecondary(uint16_t nLengthOfDuck) {    // converted from decomp src/CPedIntelligence/*.c
    (void)nLengthOfDuck;
    // TODO(port): task batch. Original logic (00601230):
    //   if the TASK_SECONDARY_DUCK task isn't already a controlled duck task,
    //   allocate a CTaskSimpleDuck via CTask::operator_new + Constructor
    //   (DUCK_TASK_CONTROLLED, nLengthOfDuck, -1) and SetTaskSecondary it;
    //   if the TASK_SECONDARY_ATTACK task is TASK_SIMPLE_USE_GUN, clear its anim;
    //   then invoke the duck task's vtable+0x1c entry (MakePedDuck) on m_pPed.
}














void CPedIntelligence::ClearTaskDuckSecondary() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task batch. Original logic (00601380):
    //   if a TASK_SECONDARY_DUCK task exists: invoke its vtable+0x18 entry
    //   (abort, args m_pPed/0/0); reset move state (or m_fMoveBlendRatio for
    //   player data); if the TASK_SECONDARY_ATTACK task is TASK_SIMPLE_USE_GUN,
    //   clear its anim via CTaskSimpleUseGun::ClearAnim.
}














void CPedIntelligence::ClearTasks(bool bClearPrimaryTasks, bool bClearSecondaryTasks) {    // converted from decomp src/CPedIntelligence/*.c
    (void)bClearPrimaryTasks;
    (void)bClearSecondaryTasks;
    // TODO(port): task/event batch. Original logic (006014A0):
    //   primary: if the ped isn't standing/in a vehicle, queue a script-command
    //     event with a new CTaskSimpleStandStill (or CTaskSimpleCarDrive /
    //     CTaskSimpleCarDriveTimed when in a car); HandleEvents; ManageTasks;
    //     CPedScriptedTaskRecord::Process.
    //   secondary: for each secondary slot except FACIAL_COMPLEX, abort or
    //     remove the task (vtable+0x18 MakePedAbort).
}














void CPedIntelligence::FlushImmediately(bool bSetPrimaryDefaultTask) {    // converted from decomp src/CPedIntelligence/*.c
    (void)bSetPrimaryDefaultTask;
    // TODO(port): task/event batch. Original logic (00601640):
    //   stash the hold-entity / facial sub-tasks, flush the event group, event
    //   handler and task manager, restore the stashed sub-tasks, then set the
    //   default primary task (CTaskSimplePlayerOnFoot for the player,
    //   CTaskComplexWander::GetWanderTaskByPedType or CTaskSimpleStandStill
    //   otherwise).
}














C2dEffect* CPedIntelligence::GetEffectInUse() const {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): event-scanner batch (needs CEventScanner::m_attractorScanner.m_pEffectInUse).
    return nullptr;
}














void CPedIntelligence::SetEffectInUse(C2dEffect* effect) {    // converted from decomp src/CPedIntelligence/*.c
    (void)effect;
    // TODO(port): event-scanner batch (needs CEventScanner::m_attractorScanner.m_pEffectInUse).
}














void CPedIntelligence::ProcessAfterProcCol() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task/RW batch. Original logic (00601910):
    //   if the simplest active task IsSimple (vtable+0xc) and its vtable+0x20
    //   entry returns true for m_pPed, refresh the ped's RW matrix/frame;
    //   then clear bDonePositionOutOfCollision and mask bKilledByStealth /
    //   bRightArmBlocked / bWaitingForScriptBrainToLoad with 0xfffffbff.
}














void CPedIntelligence::ProcessAfterPreRender() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task/RW/FX batch. Original logic (006019E0):
    //   run the secondary partial-anim / attack tasks' vtable+0xc/0x20 entries;
    //   update the gun-flash FX matrix from the weapon bone (RW anim hierarchy);
    //   CBike::FixHandsToBars when the ped is on a bike.
}














void CPedIntelligence::ProcessEventHandler() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): event batch. Original: m_eventHandler.HandleEvents();
}














bool CPedIntelligence::IsFriendlyWith(const CPed& ped) const {    // converted from decomp src/CPedIntelligence/*.c
    const uint32_t respect = m_pPed->m_acquaintance.GetAcquaintances(0 /* ACQUAINTANCE_RESPECT */);
    const uint32_t like = m_pPed->m_acquaintance.GetAcquaintances(1 /* ACQUAINTANCE_LIKE */);
    const uint32_t pedFlag = CPedType::GetPedFlag(ped.m_nPedType);
    if (m_pPed->m_nPedType != ped.m_nPedType && (respect & pedFlag) == 0 && (like & pedFlag) == 0) {
        return false;
    }
    return true;
}














bool CPedIntelligence::IsThreatenedBy(const CPed& ped) const {    // converted from decomp src/CPedIntelligence/*.c
    const uint32_t dislike = m_pPed->m_acquaintance.GetAcquaintances(3 /* ACQUAINTANCE_DISLIKE */);
    const uint32_t hate = m_pPed->m_acquaintance.GetAcquaintances(4 /* ACQUAINTANCE_HATE */);
    const uint32_t pedFlag = CPedType::GetPedFlag(ped.m_nPedType);
    return ((dislike & pedFlag) != 0) || ((hate & pedFlag) != 0);
}














bool CPedIntelligence::Respects(CPed* ped) const {    // converted from decomp src/CPedIntelligence/*.c
    const uint32_t respect = m_pPed->m_acquaintance.GetAcquaintances(0 /* ACQUAINTANCE_RESPECT */);
    const uint32_t pedFlag = CPedType::GetPedFlag(ped->m_nPedType);
    return (respect & pedFlag) != 0;
}














bool CPedIntelligence::IsInACarOrEnteringOne() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task batch. Original logic (0x601CC0, gta-reversed):
    //   Find<CTaskComplexEnterCarAsDriver> -> return !!task->GetTargetCar();
    //   Find<CTaskComplexEnterCarAsPassenger> -> return !!task->GetTargetCar();
    //   Find<CTaskSimpleCarDrive> -> return !!task->GetVehicle();
    //   else false.
    return false;
}














bool CPedIntelligence::AreFriends(const CPed& ped1, const CPed& ped2) {    // converted from decomp src/CPedIntelligence/*.c
    // Cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp (0x601D10).
    return ped1.GetIntelligence()->IsFriendlyWith(ped2) || ped2.GetIntelligence()->IsFriendlyWith(ped1);
}














bool CPedIntelligence::IsPedGoingSomewhereOnFoot() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task batch. Original logic (0x601D50, gta-reversed):
    //   return simplest active task && CTask::IsGoToTask(task).
    return false;
}














eMoveState CPedIntelligence::GetMoveStateFromGoToTask() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task batch. Original logic (0x601D70, gta-reversed):
    //   return the simplest active task's m_moveState when CTask::IsGoToTask(task),
    //   else PEDMOVE_STILL (needs CTaskSimpleGoTo).
    return PEDMOVE_STILL;
}














void CPedIntelligence::FlushIntelligence() {    // converted from decomp src/CPedIntelligence/*.c
    m_TaskMgr.Flush();
    // TODO(port): event/scanner batch. Original also did:
    //   m_eventHandler: m_PhysicalResponseTask/m_EventResponseTask/m_AttackTask/
    //     m_SayTask/m_PartialAnimTask = nullptr;
    //   CEventHandlerHistory::ClearAllEvents(&m_eventHandler.m_History);
    //   CEventGroup::Flush(&m_eventGroup, false);
    //   CEntityScanner::Clear(&m_vehicleScanner); CEntityScanner::Clear(&m_pedScanner);
    //   CAttractorScanner::Clear();
}














bool CPedIntelligence::TestForStealthKill(CPed* pTarget, bool bFullTest) {    // converted from decomp src/CPedIntelligence/*.c
    // Cross-checked vs gta-reversed/source/game_sa/PedIntelligence.cpp (0x601E00).
    // TODO(task-batch): skip when the target's active task is TASK_COMPLEX_KILL_PED_ON_FOOT
    //   (1000) with m_target == m_pPed (needs CTaskComplexKillPedOnFoot).
    // TODO(event-batch): skip when the target's current event source is m_pPed and the
    //   hate/dislike acquaintance flags are set (needs CEventHandlerHistory::GetCurrentEvent
    //   and CEvent::GetSourceEntity; group-intelligence check likewise).
    if (pTarget->bInVehicle) {
        return false;
    }
    if (pTarget->bIsDucking || pTarget->m_fHealth < 1.0f) {
        return false;
    }
    CVector headPos;
    pTarget->GetBonePosition(&headPos, BONE_HEAD, false);
    if (headPos.z < pTarget->GetPosition().z) {
        return false;
    }
    if (bFullTest) {
        return true;
    }
    if (pTarget->m_nMoveState >= PEDMOVE_RUN) {
        return false;
    }
    const CVector distance = pTarget->GetPosition() - m_pPed->GetPosition();
    if (STEALTH_KILL_RANGE * STEALTH_KILL_RANGE < distance.SquaredMagnitude()) {
        return false;
    }
    const CVector& fwd = pTarget->GetForward();
    if (distance.x * fwd.x + distance.y * fwd.y + distance.z * fwd.z <= 0.0f) {
        return false;
    }
    return true;
}














void CPedIntelligence::RecordEventForScript(int32_t eventId,  int32_t eventPriority) {    // converted from decomp src/CPedIntelligence/*.c
        if ((eventId != 0x20) && ((eventId == 0 || ((int)(uint32_t)this->m_nEventPriority < eventPriority))))
        {
            this->m_nEventId = (uint8_t)eventId;
            this->m_nEventPriority = (uint8_t)eventPriority;
        }
        return;
}











bool CPedIntelligence::HasInterestingEntites() {    // converted from decomp src/CPedIntelligence/*.c
    for (CEntity* entity : m_apInterestingEntities) {
        if (entity) {
            return true;
        }
    }
    return false;
}














bool CPedIntelligence::IsInterestingEntity(CEntity* entity) {    // converted from decomp src/CPedIntelligence/*.c
        int iVar1;
        CEntity **ppCVar2;

        iVar1 = 0;
        ppCVar2 = this->m_apInterestingEntities;
        do {
            if (*ppCVar2 == entity) {
                return true;
            }
            iVar1 = iVar1 + 1;
            ppCVar2 = ppCVar2 + 1;
        } while (iVar1 < 3);
        return false;
}











void CPedIntelligence::LookAtInterestingEntities() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): IK/world batch. Original logic (0x602100):
    //   if not bDonePositionOutOfCollision and not already looking and on screen,
    //   50% chance: FindObjectsInRange (15.0), keep the interesting ones,
    //   IKChainManager_c::LookAt a random one for 3000-5000ms.
}














void CPedIntelligence::RemoveAllInterestingEntities() {    // converted from decomp src/CPedIntelligence/*.c
    // Decomp walked this+0x288 (m_apInterestingEntities).
    for (CEntity*& entity : m_apInterestingEntities) {
        if (entity) {
            entity->CleanUpOldReference(&entity);
        }
        entity = nullptr;
    }
}














bool CPedIntelligence::IsPedGoingForCarDoor() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task batch. Original logic (0x602350): true when the simplest
    // active task (or its parent chain) is TASK_COMPLEX_ENTER_CAR (800).
    return false;
}














float CPedIntelligence::CanSeeEntityWithLights(CEntity* entity, int32_t unUsed) {    // converted from decomp src/CPedIntelligence/*.c
    (void)unUsed;
    float lightLevel = LIGHT_AI_LEVEL_MAX;
    const float lighting = static_cast<CPhysical*>(entity)->GetLightingTotal();
    if (entity->GetIsTypePed() && static_cast<CPed*>(entity)->IsPlayer() && lighting <= LIGHT_AI_LEVEL_MAX) {
        const CVector diff = entity->GetPosition() - m_pPed->GetPosition();
        const float dist = sqrtf(diff.SquaredMagnitude()) - 0.7f;
        lightLevel = lighting * lighting - (dist / flt_8D2384) * LIGHT_AI_LEVEL_MAX * LIGHT_AI_LEVEL_MAX;
        if (lightLevel <= 0.0f) {
            lightLevel = lightLevel * lightLevel - (dist / flt_8D2388) * LIGHT_AI_LEVEL_MAX * LIGHT_AI_LEVEL_MAX;
            if (lightLevel <= 0.0f) {
                return 0.0f;
            }
            return -lightLevel;
        }
    }
    return lightLevel;
}














void CPedIntelligence::ProcessStaticCounter() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): task batch (needs CTask::IsGoToTask + CTaskSimpleGoTo::m_moveState).
    // Original logic (00605760): if the simplest active task is a go-to task with
    // move state 4/6/7, maintain m_StaticCounter/m_AnotherStaticCounter from the
    // ped's damage-entity state and movement since m_vecLastPedPosDuringDamageEntity.
}














void CPedIntelligence::ProcessFirst() {    // converted from decomp src/CPedIntelligence/*.c
    // TODO(port): scanner/stuck-checker batch. Original logic (00605A90):
    //   ProcessStaticCounter; CPedStuckChecker::TestPedStuck or
    //   CCollisionEventScanner::ScanForCollisionEvents; set bHasBulletProofVest
    //   flag bit 0x4000 on hard frontal damage impact; zero bike engine-bank
    //   bytes when the ped is on a bike; clear the vest flag bit afterwards.
}














void CPedIntelligence::Process() {    // converted from decomp src/CPedIntelligence/Process_00608260.c
    // TODO(port): task/event/scanner batch unported. Original call sequence (00608260):
    //   CLoadMonitor::StartTimer(&g_LoadMonitor, PED_AI);
    //   CVehicleScanner::ScanForVehiclesInRange(m_pPed);
    //   if (m_pPed->IsAlive())
    //       m_pedScanner.ScanForEntitiesInRange(REPEATSECTOR_PEDS, m_pPed);
    //   CEventScanner::ScanForEvents(m_pPed);
    //   m_eventHandler.HandleEvents();
    //   m_TaskMgr.ManageTasks();
    //   CPlayerRelationshipRecorder::RecordRelationshipWithPlayer(m_pPed);
    //   LookAtInterestingEntities();
    //   CLoadMonitor::EndTimer(&g_LoadMonitor, PED_AI);
}











CTask* CPedIntelligence::GetActivePrimaryTask() const {    // converted from decomp src/CPedIntelligence/*.c
    // Decomp read m_aPrimaryTasks[0..2] via byte offsets (+0/+4/+8).
    CTask* task = m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_PHYSICAL_RESPONSE];
    if (!task) {
        task = m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_EVENT_RESPONSE_TEMP];
    }
    if (!task) {
        task = m_TaskMgr.m_aPrimaryTasks[TASK_PRIMARY_EVENT_RESPONSE_NONTEMP];
    }
    return task;
}














float CPedIntelligence::GetPedFOVRange() const {    // converted from decomp src/CPedIntelligence/*.c
    // Decomp compared *(this+0xC0) < *(this+0xBC): m_fSeeingRange < m_fHearingRange.
    return m_fSeeingRange < m_fHearingRange ? m_fHearingRange : m_fSeeingRange;
}














void CPedIntelligence::IncrementAngerAtPlayer(uint8_t anger) {    // no decomp; adapted from gta-reversed
    // TODO(port): CMentalState not yet ported.
    // Original: if (!m_mentalState.m_AngerTimer.IsOutOfTime()) return;
    //   m_mentalState.m_AngerTimer.Start(3000); m_mentalState.m_AngerAtPlayer += anger;
}













bool CPedIntelligence::IsUsingGun() {    // no decomp; adapted from gta-reversed
    // Minimal version: checks the simplest active task's type.
    // TODO(tasks): TASK_SIMPLE_GANG_DRIVEBY check needs the eTaskType enum (task batch).
    const CTask* task = m_TaskMgr.GetSimplestActiveTask();
    if (!task) {
        return false;
    }
    const int32_t type = task->GetTaskType();
    return type == 0x3F9 /* TASK_SIMPLE_USE_GUN */ || type == 0x3FE /* TASK_SIMPLE_GANG_DRIVEBY */;
}















class CVehicle* CPedIntelligence::GetEnteringVehicle() {    // no decomp; adapted from gta-reversed
    // TODO(port): task classes not yet ported.
    // Original scans TASK_COMPLEX_ENTER_CAR_AS_DRIVER/PASSENGER via FindTaskByType.
    return nullptr;
}












CEntity** CPedIntelligence::GetPedEntities() {    // converted from decomp src/CPedIntelligence/*.c
    // Decomp returned this+0x130 (the scanner's entity array); now direct.
    return m_pedScanner.m_apEntities;
}














CEntity* CPedIntelligence::GetPedEntity(uint32_t index) {    // no decomp; adapted from gta-reversed
    return m_pedScanner.m_apEntities[index];
}















