#pragma once
// CTaskSimpleHoldEntity - minimal stub.
// TODO(port): full task port (added 2026-10-09 for CPed).
#include "CTask.h"
#include "CVector.h" // CVector (ctor param)

class CEntity;
class CPed;

class CTaskSimpleHoldEntity : public CTask {
public:
    CEntity* m_pEntityToHold{}; // TODO(port): verify member
    int32_t m_nBoneFrameId{}; // added 2026-10-09 for CPed::GiveDelayedWeapon
    // gta-reversed TaskSimpleHoldEntity.h. Added 2026-10-09 for CPed::GiveObjectToPedToHold.
    CTaskSimpleHoldEntity(CEntity* entityToHold, const CVector* posn, uint8_t boneFrameId,
        uint8_t boneFlags, int32_t animId, int32_t groupId, bool bDisAllowDroppingOnAnimEnd) {
        (void)entityToHold; (void)posn; (void)boneFrameId; (void)boneFlags;
        (void)animId; (void)groupId; (void)bDisAllowDroppingOnAnimEnd;
    }
    // TODO(port): stubs; signatures from decomp call sites
    void DropEntity(CPed* ped, bool arg) { (void)ped; (void)arg; }
    bool CanThrowEntity() { return false; }
};
