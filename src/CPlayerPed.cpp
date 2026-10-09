// CPlayerPed.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CPlayerPed/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CPlayerPed.h"
#include "CPlayerPedData.h"
#include "CTimer.h"
#include "CWeaponInfo.h"
#include "CPad.h"
#include "CWanted.h"
#include "CStats.h"
#include "CAnimBlendAssociation.h"
#include "CMBlur.h"
#include "CTaskSimpleUseGun.h"
#include "CVehicle.h"
#include "CWeaponEffects.h"
#include "CCamera.h"
#include "CAudioEngine.h"
#include "CWorld.h"
#include "CGeneral.h"
#include "CCarCtrl.h"
#include "CWaterLevel.h"
#include "CAnimManager.h"
#include "CAnimBlendHierarchy.h"
#include "CAnimBlendStaticAssociation.h"
#include <cmath>
#include <algorithm>
#include "eStatModAbilities.h"
#include <array>
#include <cstdint>

bool CPlayerPed::bHasDisplayedPlayerQuitEnterCarHelpText{}; // game address: 0xC0BC15
bool CPlayerPed::bDebugPlayerInvincible{}; // game address: none (plain static in gta-reversed)
bool CPlayerPed::bDebugTargeting{}; // game address: none (plain static in gta-reversed)
bool CPlayerPed::bDebugTapToTarget{}; // game address: none (plain static in gta-reversed)

std::array<bool, 7> abTempNeverLeavesGroup{}; // game address: 0xC0BC08
int32_t gPlayIdlesAnimBlockIndex{}; // game address: 0xC0BC10

CPlayerPed::CPlayerPed(int32_t playerId,  bool bGroupCreated) : CPed(PED_TYPE_PLAYER1) {    // converted from decomp src/CPlayerPed/Constructor_005de2f0.c
    // TODO(port): CWorld::Players[playerId] (CWorld unported). Decomp: m_pPlayerData = &CWorld::Players[playerId].m_PlayerData;
    // TODO(port): CPlayerPedData::AllocateData(&CWorld::Players[playerId].m_PlayerData) (CPlayerPedData opaque).
    SetModelIndex(0);
    SetInitialState(PED_TYPE_PLAYER1);
    if (this->m_pTargetedObject != nullptr) {
        CleanUpOldReference(&m_pTargetedObject);
    }
    this->m_pTargetedObject = nullptr;
    SetPedState(PEDSTATE_IDLE);
    // TODO(port): ::gPlayIdlesAnimBlockIndex = CAnimManager::GetAnimationBlockIndex("playidles") (CAnimManager unported).
    if (PED_TYPE_PLAYER1 == 0) {
        // TODO(port): group creation (CPedGroups/CPedGroupIntelligence/CPedGroupMembership unported).
        // Decomp: pCVar2->m_nPlayerGroup = CPedGroups::AddGroup(); ... SetLeader(...); CPedGroup::Process(...);
        // TODO(port): CPlayerPedData::__anon0 player-flags manipulation (CPlayerPedData opaque).
    }
    // TODO(port): m_fMaxHealth/m_fHealth = CStats::GetFatAndMuscleModifier(9) (CStats unported).
    this->m_nFightingStyle = STYLE_GRAB_KICK;
    this->m_nAllowedAttackMoves = 0x0f;
    // TODO(port): CAEPedSpeechAudioEntity::Initialise(&m_pedSpeech, this) (audio entity opaque).
    // TODO(port): m_pIntelligence->m_fDmRadius = 30.0f; m_pIntelligence->m_nDmNumPedsToScan = 2; (CPedIntelligence fields differ locally).
    // TODO(port): player-flag bit 0x200000 (bHasBulletProofVest region) set from PED_TYPE_PLAYER1 (bitfield layout differs).
}











void CPlayerPed::ProcessControl() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(convert): goto
    // TODO(convert): anon-struct ref
    // TODO(convert): uint8_t-slice
    // TODO(convert): DAT_ global
        uint8_t *puVar2;
        void *pvVar3;
        CAnimBlendAssociation *pCVar4;
        CEntity *pCVar5;
        eWeaponType weaponType;
        uint32_t uVar6;
        uint32_t uVar7;
        CMatrixLink *pCVar8;
        eMoveState eVar9;
        uint32_t uVar10;
        uint32_t uVar11;
        uint32_t uVar12;
        uint32_t uVar13;
        uint32_t uVar14;
        float fVar15;
        CVector posn;
        eWeaponSkill skill;
        bool bVar16;
        uint8_t uVar17;
        uint8_t green;
        uint8_t blue;
        char cVar18;
        CPad *this_00;
        CWanted *this_01;
        CWeaponInfo *pCVar19;
        CTaskSimpleUseGun *pCVar20;
        int iVar21;
        CSimpleTransform *pCVar22;
        uint32_t uVar23;
        CVector *pCVar24;
        float *pfVar25;
        CTaskSimpleSwim *pCVar26;
        uint32_t uVar27;
        CPlayerPedData *pCVar28;
        CPad *this_02;
        CSimpleTransform *pCVar29;
        CColModel *pCVar30;
        CVehicle *pCVar31;
        CPed *pCVar32;
        uint8_t bVar33;
        long double /* Ghidra float10 */ fVar34;
        float fVar35;
        float fVar36;
        eGlobalSpeechContext gCtx;
        eAudioEvents eventId;
        bool isForceAudible;
        bool isFrontEnd;
        float fStack_34;
        RwV3d RStack_24;
        float fStack_18;
        float fStack_14;
        float fStack_10;
        float afStack_c [3];

        uVar17 = this->m_pPlayerData->m_nCarDangerCounter;
        if (uVar17 != '\0') {
            this->m_pPlayerData->m_nCarDangerCounter = uVar17 + 0xff;
        }
        if (this->m_pPlayerData->m_nCarDangerCounter == '\0') {
            this->m_pPlayerData->m_pDangerCar = nullptr;
        }
        pCVar28 = this->m_pPlayerData;
        if (pCVar28->m_nFadeDrunkenness != '\0') {
            if (pCVar28->m_nDrunkenness == 1 || (int)(pCVar28->m_nDrunkenness - 1) < 0) {
                pCVar28->m_nDrunkenness = '\0';
                CMBlur::ClearDrunkBlur();
                this->m_pPlayerData->m_nFadeDrunkenness = '\0';
            }
            else {
                pCVar28->m_nDrunkenness = pCVar28->m_nDrunkenness + 0xff;
            }
        }
        bVar33 = this->m_pPlayerData->m_nDrunkenness;
        if (bVar33 != 0) {
            CMBlur::SetDrunkBlur((float)bVar33 * 0.003921569);
        }
        pCVar28 = this->m_pPlayerData;
        if ((char)pCVar28->m_bPlayerFlagsByte1 < '\0') {
            fVar34 = (long double /* Ghidra float10 */)CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG);
            if ((long double /* Ghidra float10 */)pCVar28->m_fBreath < fVar34) {
                this->m_pPlayerData->m_fBreath =
                CTimer::ms_fTimeStep + CTimer::ms_fTimeStep + this->m_pPlayerData->m_fBreath;
            }
        // TODO(port): CPlayerPedData::__anon0 player-flags bit 7 clear (CPlayerPedData opaque).
        }
        // TODO(port): CPlayerPedData::__anon0 player-flags bit 7 set (CPlayerPedData opaque).
        ProcessControl();
        // TODO(port): set bit 0x100 in CPed 0x46c flags word (decomp: puVar2=&flags; *puVar2|=0x100).
        if ((m_nFlags >> 6 & 1) != 0) {
            return;
        }
        if (this->m_nPedType == PED_TYPE_PLAYER1) {
            this_00 = (CPad *)CPad::GetPad(0);
        }
        else if (this->m_nPedType == PED_TYPE_PLAYER2) {
            this_00 = (CPad *)CPad::GetPad(1);
        }
        else {
            this_00 = nullptr;
        }
        if (this->m_pPlayerData == nullptr) {
            this_01 = nullptr;
        }
        else {
            this_01 = this->m_pPlayerData->m_pWanted;
        }
        this_01->Update();
        PruneReferences();
        if (m_aWeapons[m_nActiveWeaponSlot].m_Type == (eWeaponType)0x26) {
            // TODO(port): pCVar19 = CWeaponInfo::GetWeaponInfo(WEAPON_MINIGUN, STD); (CWeaponInfo unported)
            pCVar20 = m_pIntelligence->GetTaskUseGun();
            if (pCVar20 != nullptr) {
                pCVar20 = m_pIntelligence->GetTaskUseGun();
                pCVar4 = pCVar20->m_Anim;
                if ((pCVar4 != nullptr) &&
                (pCVar4->m_CurrentTime - pCVar4->m_TimeStep < pCVar19->m_fAnimLoopEnd)) {
                    pCVar28 = this->m_pPlayerData;
                    if (pCVar28->m_fGunSpinSpeed < 0.45) {
                        pCVar28->m_fGunSpinSpeed = CTimer::ms_fTimeStep * 0.025 + pCVar28->m_fGunSpinSpeed;
                        if (0.45 < this->m_pPlayerData->m_fGunSpinSpeed) {
                            this->m_pPlayerData->m_fGunSpinSpeed = 0.45;
                        }
                    }
                    iVar21 = this_00->GetWeapon((CPed *)this);
                    if (((iVar21 == 0) ||
                    (m_aWeapons[m_nActiveWeaponSlot].m_TotalAmmo < 1)) ||
                    (pCVar4->m_CurrentTime < pCVar19->m_fAnimLoopStart)) {
                        m_weaponAudio.AddAudioEvent(0x97);
                    }
                    else {
                        m_weaponAudio.AddAudioEvent(0x96);
                    }
                    goto LAB_0060ed19;
                }
            }
            pCVar28 = this->m_pPlayerData;
            if (0.0 < pCVar28->m_fGunSpinSpeed) {
                pCVar28->m_fGunSpinSpeed = pCVar28->m_fGunSpinSpeed - CTimer::ms_fTimeStep * 0.003;
                if (this->m_pPlayerData->m_fGunSpinSpeed < 0.0) {
                    this->m_pPlayerData->m_fGunSpinSpeed = 0.0;
                }
            }
        }
    LAB_0060ed19:
        if ((m_aWeapons[m_nActiveWeaponSlot].m_Type == WEAPON_CHAINSAW) &&
        (this->m_nPedState != PEDSTATE_ATTACK) &&
        ((bInVehicle) == 0)) {
            this->m_pIntelligence->GetTaskSwim();
        }
        if (this->m_pTargetedObject != nullptr) {
            if (this->m_p3rdPersonMouseTarget != nullptr) {
                CEntity::ClearReference(this->m_p3rdPersonMouseTarget);
                this->m_p3rdPersonMouseTarget = nullptr;
            }
            pCVar5 = this->m_pTargetedObject;
            fStack_34 = 1.0f; // markColor default (gta-reversed)
            RStack_24.x = 0.0;
            RStack_24.y = 0.0;
            RStack_24.z = 0.0;
            if (pCVar5->GetIsTypePed()) {
                weaponType = m_aWeapons[m_nActiveWeaponSlot].m_Type;
                skill = GetWeaponSkill();
                pCVar19 = CWeaponInfo::GetWeaponInfo(weaponType,skill);
                pCVar32 = (CPed *)this->m_pTargetedObject;
                fStack_34 = pCVar32->m_fHealth / pCVar32->m_fMaxHealth;
                fVar35 = pCVar19->GetTargetHeadRange();
                bVar16 = pCVar32->IsAlive();
                if ((bVar16) &&
                (pCVar19 = CWeaponInfo::GetWeaponInfo
                (m_aWeapons[m_nActiveWeaponSlot].m_Type,eWeaponSkill::STD),
                pCVar19->m_nWeaponFire == eWeaponFire::WEAPON_FIRE_INSTANT_HIT)) {
                    fVar34 = (pCVar32->GetPosition() - GetPosition()).SquaredMagnitude();
                    if (fVar35 * fVar35 <= fVar34) goto LAB_0060ee83;
                    this->m_pPlayerData->m_nTargetBone = 5;
                    (this->m_pPlayerData->m_vecTargetBoneOffset).x = 0.05;
                }
                else {
                LAB_0060ee83:
                    this->m_pPlayerData->m_nTargetBone = 3;
                    (this->m_pPlayerData->m_vecTargetBoneOffset).x = 0.2;
                }
                pCVar28 = this->m_pPlayerData;
                RStack_24.x = (pCVar28->m_vecTargetBoneOffset).x;
                RStack_24.y = (pCVar28->m_vecTargetBoneOffset).y;
                RStack_24.z = (pCVar28->m_vecTargetBoneOffset).z;
                pCVar32->GetTransformedBonePosition(RStack_24,(eBoneTagU32)(eBoneTag)pCVar28->m_nTargetBone,false);
                fVar35 = CTimer::ms_fTimeStep;
                if (fStack_34 > 0.0f) {
                    if (!pCVar32->bInVehicle && pCVar32->m_nMoveState != PEDMOVE_STILL) {
                        RStack_24.x += pCVar32->m_vecMoveSpeed.x * fVar35;
                        RStack_24.y += pCVar32->m_vecMoveSpeed.y * fVar35;
                        RStack_24.z += pCVar32->m_vecMoveSpeed.z * fVar35;
                    }
                }
                if (pCVar32->bInVehicle) {
                    CVehicle* pTargVeh = pCVar32->m_pVehicle;
                    if (pTargVeh != nullptr) {
                        RStack_24.x += (pTargVeh->m_vecMoveSpeed.x + pTargVeh->m_vecTurnSpeed.x) * fVar35;
                        RStack_24.y += (pTargVeh->m_vecMoveSpeed.y + pTargVeh->m_vecTurnSpeed.y) * fVar35;
                        RStack_24.z += (pTargVeh->m_vecMoveSpeed.z + pTargVeh->m_vecTurnSpeed.z) * fVar35;
                    }
                }
            }
            else if (pCVar5->GetIsTypeVehicle()) {
                CVehicle* pTargVeh2 = (CVehicle*)pCVar5;
                CVector vOff = (pTargVeh2->m_vecMoveSpeed + pTargVeh2->m_vecTurnSpeed) * CTimer::ms_fTimeStep;
                const CVector& vPos = pTargVeh2->GetPosition();
                RStack_24.x = vOff.x + vPos.x;
                RStack_24.y = vOff.y + vPos.y;
                RStack_24.z = vOff.z + vPos.z;
            }
            else if (pCVar5->GetIsTypeObject()) {
                // TODO(port): full CObject targeting (m_vecMoveSpeed, m_fHealth) - using position only
                const CVector& oPos = pCVar5->GetPosition();
                RStack_24.x = oPos.x;
                RStack_24.y = oPos.y;
                RStack_24.z = oPos.z;
            }
            else {
                const CVector& ePos = pCVar5->GetPosition();
                RStack_24.x = ePos.x;
                RStack_24.y = ePos.y;
                RStack_24.z = ePos.z;
                fStack_34 = 0.0f; // limitMarkColor = false -> force RGB zero
            }
            if (fStack_34 > 0.0f) {
                float c = fStack_34 < 1.0f ? fStack_34 : 1.0f;
                uVar17 = (uint8_t)((1.0f - c) * 255.0f);
                green = (uint8_t)(c * 255.0f);
                blue = 0;
            }
            else {
                blue = 0;
                green = 0;
                uVar17 = 0;
            }
            if (this->m_matrix == nullptr) {
                pCVar29 = &this->m_placement;
            }
            else {
                pCVar29 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            fVar35 = RStack_24.x - (pCVar29->m_vPosn).x;
            fVar36 = RStack_24.y - (pCVar29->m_vPosn).y;
            fVar15 = RStack_24.z - (pCVar29->m_vPosn).z;
            posn.y = RStack_24.y;
            posn.x = RStack_24.x;
            posn.z = RStack_24.z;
            CWeaponEffects::MarkTarget
            ((CrossHairId)this->m_nPedType,posn,uVar17,green,blue,0xff,
            (float)(1.0 - sqrt(fVar36 * fVar36 + fVar15 * fVar15 + fVar35 * fVar35) * 0.02),false);
        }
        eVar9 = this->m_nMoveState;
        if (eVar9 == PEDMOVE_NONE) {
            uVar10 = (bIsStanding ? 1u : 0u) |
                     (bInVehicle ? 0x100u : 0u) |
                     (bFiringWeapon ? 0x10000u : 0u) |
                     (bNotAllowedToDuck ? 0x1000000u : 0u);
            if ((((uVar10 & 0x100) != 0) && (this->m_pVehicle != nullptr)) &&
            (this->m_pVehicle->m_nVehicleSubType != VEHICLE_TYPE_BMX)) {
            LAB_0060f1e7:
                HandleSprintEnergy(false,1.0f);
            }
        }
        else if (eVar9 == PEDMOVE_RUN) {
            pCVar28 = this->m_pPlayerData;
            fVar34 = (long double /* Ghidra float10 */)CStats::GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN);
            if ((long double /* Ghidra float10 */)pCVar28->m_fTimeCanRun < fVar34) {
                this->m_pPlayerData->m_fTimeCanRun =
                CTimer::ms_fTimeStep * 0.15 + this->m_pPlayerData->m_fTimeCanRun;
            }
        }
        else if (eVar9 != PEDMOVE_SPRINT) goto LAB_0060f1e7;
        m_aWeapons[m_nActiveWeaponSlot].Update((CPed *)this);
        if ((this->m_nPedState == PEDSTATE_DEAD) || (this->m_nPedState == PEDSTATE_DIE)) {
            ClearWeaponTarget();
            return;
        }
        if (this_00 != nullptr) {
            bVar16 = this_00->WeaponJustDown((CPed *)this);
            if (bVar16) {
                iVar21 = (int)m_aWeapons[m_nActiveWeaponSlot].m_Type;
                bVar16 = TheCamera.Using1stPersonWeaponMode();
                if (((!bVar16) ||
                m_aWeapons[m_nActiveWeaponSlot].m_State == WEAPONSTATE_OUT_OF_AMMO) &&
                (pCVar26 = this->m_pIntelligence->GetTaskSwim(),
                pCVar26 == nullptr)) {
                    if (iVar21 == 0x22) {
                        eventId = (eAudioEvents)41; // AE_FRONTEND_FIRE_FAIL_SNIPERRIFFLE
                    }
                    else {
                        if ((iVar21 != 0x23) && (iVar21 != 0x24)) goto LAB_0060f2b0;
                        eventId = (eAudioEvents)42; // AE_FRONTEND_FIRE_FAIL_ROCKET
                    }
                    AudioEngine.ReportFrontendAudioEvent(eventId,0.0f,1.0f);
                }
            }
        LAB_0060f2b0:
            bVar16 = IsPedShootable();
            if ((bVar16) && (this->m_nPedState != PEDSTATE_ANSWER_MOBILE)) {
                // TODO(port): CWorld::Players[slot].m_pRemoteVehicle check (CPlayerInfo not ported)
                ProcessWeaponSwitch(this_00);
            }
        }
        ProcessAnimGroups();
        if ((this_00 != nullptr) && (TheCamera.GetActiveCamera().m_nMode == MODE_FOLLOWPED) &&
        !TheCamera.GetActiveCamera().m_nDirectionWasLooking) {
            auto& activeCam = TheCamera.GetActiveCamera();
            this->m_nLookTime = 0;
            float lookDir = CGeneral::LimitRadianAngle(atan2f(-activeCam.m_vecFront.x, activeCam.m_vecFront.y));
            float angle = fabsf(lookDir - this->m_fCurrentRotation);
            if ((this->m_nPedState == PEDSTATE_ATTACK) || (angle <= 0.5235988f) || (5.759587f <= angle)) {
                ClearLookFlag();
            }
            else {
                if ((2.6179938f < angle) && (angle < 3.6651917f)) {
                    float dir1 = CGeneral::LimitRadianAngle(this->m_fCurrentRotation - 2.6179938f);
                    float dir2 = CGeneral::LimitRadianAngle(this->m_fCurrentRotation + 2.6179938f);
                    lookDir = dir1;
                    if ((this->m_fLookDirection != 999999.0f) && !bIsDucking) {
                        if (fabsf(dir2 - this->m_fLookDirection) <= fabsf(dir1 - this->m_fLookDirection))
                            lookDir = dir2;
                    }
                }
                SetLookFlag(lookDir, true, false);
                SetLookTimer((uint32_t)((CTimer::ms_fTimeStep * 0.02f * 1000.0f) * 5.0f));
            }
        }
        else {
            ClearLookFlag();
        }
        if ((this->m_nMoveState == PEDMOVE_SPRINT) && (bIsLooking != 0))
        {
            ClearLookFlag();
            SetLookTimer(0xfa);
        }
        pCVar28 = this->m_pPlayerData;
        if (this->m_vecMoveSpeed.Magnitude() >= 0.1f) {
            pCVar28->m_nStandStillTimer = 0;
            pCVar28->m_bStoppedMoving = false;
        }
        else if (pCVar28->m_nStandStillTimer == 0) {
            pCVar28->m_nStandStillTimer = CTimer::m_snTimeInMilliseconds + 500;
        }
        else if (CTimer::m_snTimeInMilliseconds > pCVar28->m_nStandStillTimer) {
            pCVar28->m_bStoppedMoving = true;
        }
    LAB_0060f52b:
        if ((this->m_pPlayerData->m_bDontAllowWeaponChange != false) &&
        (bVar16 = IsPlayer(), bVar16)) {
            this_02 = (CPad *)CPad::GetPad(0);
            bVar16 = this_02->GetTarget();
            if (!bVar16) {
                this->m_pPlayerData->m_bDontAllowWeaponChange = false;
            }
        }
        if ((this->m_nPedState != PEDSTATE_SNIPER_MODE) &&
        m_aWeapons[m_nActiveWeaponSlot].m_State == WEAPONSTATE_FIRING) {
            this->m_pPlayerData->m_nLastTimeFiring = CTimer::m_snTimeInMilliseconds;
        }
        ProcessGroupBehaviour(this_00);
        uVar12 = (bIsStanding ? 1u : 0u) |
                 (bInVehicle ? 0x100u : 0u) |
                 (bFiringWeapon ? 0x10000u : 0u) |
                 (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar12 & 0x100) != 0) {
            CCarCtrl::RegisterVehicleOfInterest(this->m_pVehicle);
        }
        if (!GetIsVisible()) {
            UpdateRpHAnim();
        }
        uVar13 = (bIsStanding ? 1u : 0u) |
                 (bInVehicle ? 0x100u : 0u) |
                 (bFiringWeapon ? 0x10000u : 0u) |
                 (bNotAllowedToDuck ? 0x1000000u : 0u);
        if ((uVar13 & 0x100) != 0) {
            CPad* pad0 = CPad::GetPad(0);
            if (!pad0->IsDPadDownPressed()) {
                if (pad0->IsDPadUpPressed()) {
                    this->m_pPlayerData->m_bPlayersGangActive = true;
                }
            }
            else {
                this->m_pPlayerData->m_bPlayersGangActive = false;
            }
        }
        if ((m_nPhysicalFlags & 0x100) == 0) {
            this->m_pPlayerData->m_nWaterCoverPerc = '\0';
        }
        else {
            if (this->m_matrix == nullptr) {
                pCVar29 = &this->m_placement;
            }
            else {
                pCVar29 = (CSimpleTransform *)&this->m_matrix->m_pos;
            }
            fStack_10 = (pCVar29->m_vPosn).z;
            bVar16 = CWaterLevel::GetWaterLevel
            ((pCVar29->m_vPosn).x,(pCVar29->m_vPosn).y,fStack_10 + 1.5,
            &this->m_pPlayerData->m_fWaterHeight,'\x01',nullptr);
            if (bVar16) {
                CColModel* pCol = GetColModel();
                float playerMinZ = fStack_10 + pCol->m_boundBox.m_vecMin.z;
                float playerMaxZ = fStack_10 + pCol->m_boundBox.m_vecMax.z;
                pCVar28 = this->m_pPlayerData;
                if (pCVar28->m_fWaterHeight < playerMaxZ) {
                    if (pCVar28->m_fWaterHeight > playerMinZ) {
                        pCVar28->m_nWaterCoverPerc =
                            (uint8_t)((pCVar28->m_fWaterHeight - playerMinZ) / (playerMaxZ - playerMinZ) * 100.0f);
                    }
                    else {
                        pCVar28->m_nWaterCoverPerc = '\0';
                    }
                }
                else {
                    pCVar28->m_nWaterCoverPerc = 100;
                }
            }
            else {
                m_nPhysicalFlags = m_nPhysicalFlags & 0xfffffeff;
            }
        }
        // TODO(port): group behaviour (CPedGroups/CPedGroupMembership/CGame/CTX_* not ported)
        // TODO(port): breathing audio (CEntryExitManager not ported)
        (void)pCVar31; (void)pCVar32; (void)iVar21; (void)uVar27; (void)uVar14;
        (void)fVar34; (void)fVar35; (void)bVar16; (void)cVar18; (void)gCtx;
        (void)isFrontEnd; (void)isForceAudible;
        return;
}









void CPlayerPed::SetMoveAnim() {    // converted from decomp src/CPlayerPed/*.c
        return;
}









bool CPlayerPed::Load() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full Load (CPedClothesDesc/CGenericGameStorage not ported)
    return CPed::Load();
}


bool CPlayerPed::Save() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full Save (CPedClothesDesc/CGenericGameStorage not ported)
    return CPed::Save();
}









CPad* CPlayerPed::GetPadFromPlayer() const {    // converted from decomp src/CPlayerPed/*.c
        CPad *pCVar1;

        if (this->m_nPedType == PED_TYPE_PLAYER1) {
            pCVar1 = (CPad *)CPad::GetPad(0);
            return pCVar1;
        }
        if (this->m_nPedType == PED_TYPE_PLAYER2) {
            pCVar1 = (CPad *)CPad::GetPad(1);
            return pCVar1;
        }
        return nullptr;
}









bool CPlayerPed::CanPlayerStartMission() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full check (CGameLogic/CTaskManager/CEventGroup not ported)
    return false;
}









bool CPlayerPed::IsHidden() {    // converted from decomp src/CPlayerPed/*.c
        uint32_t uVar1;
        float fVar2;

        // Decomp wrote the bools into uVar1's bytes (uVar1._0_1_ .. _3_1_).
        // 2026-10-09: packed explicitly; the CPed bool bitfields are 0/1.
        uVar1 = static_cast<uint32_t>(bIsStanding)
              | (static_cast<uint32_t>(bInVehicle) << 8)
              | (static_cast<uint32_t>(bFiringWeapon) << 16)
              | (static_cast<uint32_t>(bNotAllowedToDuck) << 24);
        if ((uVar1 & 0x100) == 0) {
            fVar2 = this->GetLightingTotal();
            if (fVar2 < 0.05 != (fVar2 == 0.05)) {
                return true;
            }
        }
        return false;
}









// TODO(anim): RpAnimBlendClumpGetAssociation not ported (RenderWare anim)
// 2026-10-09: was (void*, AnimationId); taking (RpClump*, AnimationId) makes
// this static stub an exact-match overload vs RenderWare.h's (RpClump*, uint32_t),
// fixing the C2666 ambiguity at the call below. Still returns nullptr (RW unported).
static CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, AnimationId animId) {
    (void)clump; (void)animId;
    return nullptr;
}

void CPlayerPed::ReApplyMoveAnims() {    // converted from decomp src/CPlayerPed/*.c
        AnimationId animId;
        CAnimBlendAssociation *pCVar1;
        CAnimBlendStaticAssociation *pCVar2;
        CAnimBlendAssociation *pCVar3;
        int iVar4;
        AnimationId local_14 [5];

        iVar4 = 0;
        local_14[0] = ANIM_ID_WALK;
        local_14[1] = (AnimationId)1;
        local_14[2] = (AnimationId)2;
        local_14[3] = (AnimationId)3;
        local_14[4] = (AnimationId)5;
        do {
            animId = local_14[iVar4];
            pCVar1 = RpAnimBlendClumpGetAssociation((RpClump *)GetRwObject(),animId);
            if (pCVar1 != nullptr) {
                pCVar2 = CAnimManager::GetAnimAssociation(this->m_nAnimGroup,animId);
                if (pCVar1->m_BlendHier->m_hashKey != pCVar2->m_BlendHier->m_hashKey) {
                    pCVar3 = CAnimManager::AddAnimation((RpClump *)GetRwObject(),this->m_nAnimGroup,animId);
                    pCVar3->m_BlendDelta = pCVar1->m_BlendDelta;
                    pCVar3->m_BlendAmount = pCVar1->m_BlendAmount;
                    *(uint8_t *)&pCVar1->m_Flags = (uint8_t)pCVar1->m_Flags | 4;
                    pCVar1->m_BlendDelta = -1000.0;
                }
            }
            iVar4 = iVar4 + 1;
        } while (iVar4 < 5);
        return;
}









bool CPlayerPed::DoesPlayerWantNewWeapon(eWeaponType weaponType,  bool arg1) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(convert): anon-struct ref
        ePedType eVar1;
        int iVar2;
        eWeaponType weaponType_00;
        eWeaponSkill eVar3;
        CWeaponInfo *pCVar4;
        CTaskSimpleJetPack *pCVar5;

        eVar1 = this->m_nPedType;
        if ((eVar1 == PED_TYPE_PLAYER1) || (eVar1 == PED_TYPE_PLAYER2)) {
            CPad::GetPad(eVar1);
        }
        pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType,eWeaponSkill::STD);
        iVar2 = (int)pCVar4->m_nSlot;
        weaponType_00 = m_aWeapons[iVar2].m_Type;
        if ((weaponType_00 == weaponType) || (weaponType_00 == WEAPON_UNARMED)) {
            return true;
        }
        if (!arg1) {
            pCVar5 = this->m_pIntelligence->GetTaskJetPack();
            if ((pCVar5 != nullptr) && ((char)this->m_nActiveWeaponSlot == iVar2)) {
                eVar3 = CPed::GetWeaponSkill(weaponType_00);
                pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType_00,eVar3);
                if ((pCVar4->m_nFlags >> 1 & 1) != 0) {
                    eVar3 = CPed::GetWeaponSkill(weaponType_00);
                    pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType_00,eVar3);
                    if ((pCVar4->m_nFlags >> 1 & 1) == 0) {
                        return false;
                    }
                }
            }
            if ((this->m_nPedState != PEDSTATE_ATTACK) && (this->m_nPedState != PEDSTATE_AIMGUN)) {
                return true;
            }
            if (iVar2 != (char)this->m_nActiveWeaponSlot) {
                return true;
            }
        }
        return false;
}









void CPlayerPed::ProcessPlayerWeapon(CPad* pad) {    // converted from decomp src/CPlayerPed/*.c
        return;
}









void CPlayerPed::PickWeaponAllowedFor2Player() {    // converted from decomp src/CPlayerPed/*.c
        bool bVar1;

        bVar1 = this->m_aWeapons[this->m_pPlayerData->m_nChosenWeapon].CanBeUsedFor2Player();
        if (!bVar1) {
            this->m_pPlayerData->m_nChosenWeapon = '\0';
        }
        return;
}









void CPlayerPed::UpdateCameraWeaponModes(CPad* pad) {    // converted from decomp src/CPlayerPed/*.c
        switch(*(uint32_t *)&this->m_aWeapons[this->m_nActiveWeaponSlot]) {
            case 0x1f:
            TheCamera.SetNewPlayerWeaponMode(MODE_M16_1STPERSON,0,0);
            return;
        default:
            TheCamera.ClearPlayerWeaponMode();
            return;
            case 0x22:
            /* TODO(port): SetNewPlayerWeaponMode mode unknown for this weapon */
            return;
            case 0x23:
            TheCamera.SetNewPlayerWeaponMode(MODE_ROCKETLAUNCHER,0,0);
            return;
            case 0x24:
            /* TODO(port): SetNewPlayerWeaponMode mode unknown for this weapon */
            return;
            case 0x2b:
            TheCamera.SetNewPlayerWeaponMode(MODE_CAMERA,0,0);
            return;
        }
}









void CPlayerPed::ProcessAnimGroups() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full anim group logic (needs CVisibilityPlugins)
}









void CPlayerPed::ClearWeaponTarget() {    // converted from decomp src/CPlayerPed/*.c
        if ((this->m_nPedType == PED_TYPE_PLAYER1) || (this->m_nPedType == PED_TYPE_PLAYER2)) {
            if (this->m_pTargetedObject != nullptr) {
                CleanUpOldReference(&m_pTargetedObject);
            }
            this->m_pTargetedObject = nullptr;
            TheCamera.ClearPlayerWeaponMode();
            CWeaponEffects::ClearCrossHair(this->m_nPedType);
        }
        return;
}









float CPlayerPed::GetWeaponRadiusOnScreen() {    // converted from decomp src/CPlayerPed/*.c
    // Adapted from gta-reversed (clean implementation)
    CWeapon& wep = GetActiveWeapon();
    CWeaponInfo& wepInfo = wep.GetWeaponInfo(this);

    if (wep.IsTypeMelee())
        return 0.0f;

    const float accuracyProg = 0.5f / wepInfo.m_fAccuracy;
    switch (wep.m_Type) {
    case WEAPON_SHOTGUN:
    case WEAPON_SPAS12_SHOTGUN:
    case WEAPON_SAWNOFF_SHOTGUN:
        return std::max(0.2f, accuracyProg);

    default: {
        const float rangeProg = std::min(1.0f, 15.0f / wepInfo.m_fWeaponRange);
        const float radius = (this->m_pPlayerData->m_fAttackButtonCounter * 0.5f + 1.0f) * rangeProg * accuracyProg;
        if (bIsDucking)
            return std::max(0.2f, radius / 2.0f);
        return std::max(0.2f, radius);
    }
    }
}









float CPlayerPed::FindTargetPriority(CEntity* entity) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full priority logic (needs CTaskManager, CPedGroups, m_nObjectType)
    (void)entity;
    return 0.1f;
}









void CPlayerPed::Clear3rdPersonMouseTarget() {    // converted from decomp src/CPlayerPed/*.c
        CPed **entity;

        entity = &this->m_p3rdPersonMouseTarget;
        if (*entity != nullptr) {
            CEntity::CleanUpOldReference((CEntity**)entity);
            *entity = nullptr;
        }
        return;
}









void CPlayerPed::Busted() {    // converted from decomp src/CPlayerPed/*.c
        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_pWanted->m_ChaosLevel = 0;
            return;
        }
        /* TODO: uRam00000000 decompiler artifact removed */
        return;
}









eWantedLevel CPlayerPed::GetWantedLevel() const {    // converted from decomp src/CPlayerPed/*.c
    // Adapted from gta-reversed
    if (const auto* wanted = GetWanted()) {
        return wanted->GetWantedLevel();
    }
    return (eWantedLevel)0;
}









void CPlayerPed::SetWantedLevel(eWantedLevel level) {    // converted from decomp src/CPlayerPed/*.c
        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_pWanted->SetWantedLevel(level);
            return;
        }
        return; // no player data: nothing to set
}









void CPlayerPed::SetWantedLevelNoDrop(eWantedLevel level) {    // converted from decomp src/CPlayerPed/*.c
        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_pWanted->SetWantedLevelNoDrop(level);
            return;
        }
        return; // no player data: nothing to set
}









void CPlayerPed::CheatWantedLevel(eWantedLevel level) {    // converted from decomp src/CPlayerPed/*.c
        if (this->m_pPlayerData != nullptr) {
            this->m_pPlayerData->m_pWanted->CheatWantedLevel(level);
            return;
        }
        return; // no player data: nothing to set
}









bool CPlayerPed::CanIKReachThisTarget(CVector posn,  CWeapon* weapon,  bool arg2) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(convert): anon-struct ref
        eWeaponType weaponType;
        CMatrixLink *pCVar1;
        float fVar2;
        float fVar3;
        eWeaponSkill skill;
        CWeaponInfo *pCVar4;

        weaponType = weapon->m_Type;
        skill = CPed::GetWeaponSkill(weaponType);
        pCVar4 = CWeaponInfo::GetWeaponInfo(weaponType,skill);
        if (((pCVar4->m_nFlags >> 1 & 1) == 0) &&
        (pCVar1 = this->m_matrix, fVar2 = posn.y - (pCVar1->m_pos).y,
        fVar3 = posn.x - (pCVar1->m_pos).x,
        sqrtf(fVar2 * fVar2 + fVar3 * fVar3) < (pCVar1->m_pos).z - posn.z)) {
            return false;
        }
        return true;
}









CPlayerInfo* CPlayerPed::GetPlayerInfoForThisPlayerPed() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CWorld::Players (not ported)
    return nullptr;
}









void CPlayerPed::DoStuffToGoOnFire() {    // converted from decomp src/CPlayerPed/*.c
        if (this->m_nPedState == PEDSTATE_SNIPER_MODE) {
            TheCamera.ClearPlayerWeaponMode();
            return;
        }
        return;
}









void CPlayerPed::AnnoyPlayerPed(bool arg0) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedStat (not ported)
    (void)arg0;
}









void CPlayerPed::ClearAdrenaline() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(convert): anon-struct ref
        CPlayerPedData *pCVar1;

        pCVar1 = this->m_pPlayerData;
        if (((pCVar1->m_bPlayerFlagsByte1 & 2) != 0) &&
        (pCVar1->m_nAdrenalineEndTime != 0)) {
            pCVar1->m_nAdrenalineEndTime = 0;
            CTimer::ms_fTimeScale = 1.0;
        }
        return;
}









void CPlayerPed::DisbandPlayerGroup() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedGroups/CPedGroupMembership (not ported)
}









void CPlayerPed::MakeGroupRespondToPlayerTakingDamage(CEventDamage& damageEvent) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CEvent/CPedGroups (not ported)
    (void)damageEvent;
}









void CPlayerPed::TellGroupToStartFollowingPlayer(bool arg0, bool arg1, bool arg2) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CEvent/CPedGroups (not ported)
    (void)arg0; (void)arg1; (void)arg2;
}









void CPlayerPed::MakePlayerGroupDisappear() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedGroups (not ported)
}









void CPlayerPed::MakePlayerGroupReappear() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedGroups (not ported)
}









void CPlayerPed::ResetSprintEnergy() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CWorld::Players (not ported)
}









bool CPlayerPed::HandleSprintEnergy(bool sprint, float adrenalineConsumedPerTimeStep) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full sprint energy logic (needs CPad/DAT globals)
    (void)sprint; (void)adrenalineConsumedPerTimeStep;
    return true;
}









float CPlayerPed::ControlButtonSprint(eSprintType sprintType) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full sprint control (needs CPad/DAT globals)
    (void)sprintType;
    return 0.0f;
}









float CPlayerPed::GetButtonSprintResults(eSprintType sprintType) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full sprint results (needs CPad/DAT globals)
    (void)sprintType;
    return 0.0f;
}









void CPlayerPed::ResetPlayerBreath() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CCheat (not ported)
}









void CPlayerPed::HandlePlayerBreath(bool bDecreaseAir, float fMultiplier) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full breath logic (needs CStats/CGeneral)
    (void)bDecreaseAir; (void)fMultiplier;
}









void CPlayerPed::SetRealMoveAnim() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full move anim logic (needs animation system)
}









void CPlayerPed::MakeChangesForNewWeapon(eWeaponType weaponType) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full weapon change logic (needs animation/weapon systems)
    (void)weaponType;
}











void CPlayerPed::Compute3rdPersonMouseTarget(bool meleeWeapon) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full 3rd person targeting (needs CCamera/CWorld/CEvent systems)
    (void)meleeWeapon;
}









void CPlayerPed::DrawTriangleForMouseRecruitPed() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): RenderWare immediate mode (RwIm3D*) not ported
}









bool CPlayerPed::DoesTargetHaveToBeBroken(CEntity* entity,  CWeapon* weapon) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CTagManager/CEntity internals
    (void)entity; (void)weapon;
    return false;
}









void CPlayerPed::KeepAreaAroundPlayerClear() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CTask/CEvent/CPedGroups systems
}









void CPlayerPed::SetPlayerMoveBlendRatio(CVector* arg0) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full blend ratio logic
    (void)arg0;
}









CPed* CPlayerPed::FindPedToAttack() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedPool/CWorld systems
    return nullptr;
}









void CPlayerPed::ForceGroupToAlwaysFollow(bool enable) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedGroups
    (void)enable;
}









void CPlayerPed::ForceGroupToNeverFollow(bool enable) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPedGroups
    (void)enable;
}









void CPlayerPed::MakeThisPedJoinOurGroup(CPed* ped) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CTask/CEvent/CPedGroups systems
    (void)ped;
}









bool CPlayerPed::PlayerWantsToAttack() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CTagManager/CPedGroups/CTask systems
    return false;
}









void CPlayerPed::SetInitialState(bool bGroupCreated) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CTask/CEvent systems
    (void)bGroupCreated;
}









void CPlayerPed::MakeChangesForNewWeapon(uint32_t weaponSlot) {    // converted from decomp src/CPlayerPed/MakeChangesForNewWeapon_0060d000.c
        if (weaponSlot != 0xffffffff) {
            MakeChangesForNewWeapon(m_aWeapons[weaponSlot].m_Type);
            return;
        }
        return;
}











void CPlayerPed::EvaluateTarget(CEntity* target,  CEntity*& outTarget,  float& outTargetPriority,  float maxDistance,  float arg4,  bool arg5) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full target evaluation (needs targeting systems)
    (void)target; (void)outTarget; (void)outTargetPriority; (void)maxDistance; (void)arg4; (void)arg5;
}









void CPlayerPed::EvaluateNeighbouringTarget(CEntity* target,  CEntity** outTarget,  float* outTargetPriority,  float maxDistance,  float arg4,  bool arg5) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full targeting evaluation (needs DAT globals)
    (void)target; (void)outTarget; (void)outTargetPriority; (void)maxDistance; (void)arg4; (void)arg5;
}









void CPlayerPed::ProcessGroupBehaviour(CPad* pad) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CPad/CPedGroups/DAT globals
    (void)pad;
}









bool CPlayerPed::PlayerHasJustAttackedSomeone() {    // no decomp; adapted from gta-reversed
    // Decomp has no standalone; body from gta-reversed PlayerPed.cpp.
    return PlayerWantsToAttack();
}











void CPlayerPed::ProcessWeaponSwitch(CPad* pad) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full weapon switch logic (needs CPad/DAT globals/CGameLogic)
    (void)pad;
}









bool CPlayerPed::FindWeaponLockOnTarget() {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full weapon lock-on targeting (needs CWorld/CPad systems)
    return false;
}









bool CPlayerPed::FindNextWeaponLockOnTarget(CEntity* arg0,  bool arg1) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): full weapon lock-on targeting
    return false;
}









CWanted* CPlayerPed::GetWanted() {    // no decomp; adapted from gta-reversed
    // TODO(port): CPlayerPedData incomplete (see CPlayerPed.h).
    // Original (gta-reversed PlayerPed.h): return GetPlayerData() ? GetPlayerData()->m_pWanted : nullptr;
    return nullptr;
}











const CWanted* CPlayerPed::GetWanted() const {    // no decomp; adapted from gta-reversed
    // TODO(port): CPlayerPedData incomplete (see CPlayerPed.h).
    return nullptr;
}











void CPlayerPed::RemovePlayerPed(int32_t playerId) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CWorld::Players
    (void)playerId;
}









void CPlayerPed::DeactivatePlayerPed(int32_t playerId) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CWorld::Players
    (void)playerId;
}









void CPlayerPed::ReactivatePlayerPed(int32_t playerId) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CWorld::Players
    (void)playerId;
}









bool CPlayerPed::PedCanBeTargettedVehicleWise(CPed* ped) {    // converted from decomp src/CPlayerPed/*.c
    // Adapted from gta-reversed
    if (ped->bInVehicle) {
        CVehicle* veh = ped->m_pVehicle;
        return veh && (veh->IsBike() || veh->vehicleFlags.bVehicleCanBeTargetted);
    }
    return true;
}









void CPlayerPed::SetupPlayerPed(int playerId) {    // converted from decomp src/CPlayerPed/*.c
    // TODO(port): needs CWorld::Players/CPedPool (not ported)
    (void)playerId;
}









bool LOSBlockedBetweenPeds(CEntity* entity1,  CEntity* entity2) {    // TODO: decomp src/CPlayerPed/*.c
    (void)entity1;
    (void)entity2;
    return false;
}

// Declared-only in the header (no stub): incomplete-type reference
// returns - define when the real classes land:
//   CPedGroup& CPlayerPed::GetPlayerGroup();
