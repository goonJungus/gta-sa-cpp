// CTaskSimpleUseGun - PARTIAL port for the clean-room C++ build
// Source: gta-reversed/source/game_sa/Tasks/TaskTypes/TaskSimpleUseGun.h
//
// Only the state accessed by converted TUs (m_Anim) is ported so far.
// Full port needs: CTaskSimple base, CVector2D, eGunCommand, notsa::EntityRef,
//   CWeaponInfo (complete), CEntity (complete).

#pragma once

class CAnimBlendAssociation;
class CWeaponInfo;
class CEntity;

class CTaskSimpleUseGun {
public:
    CAnimBlendAssociation* m_Anim{};       //!< Animation for the current command (Reloading, Firing, etc)
    CWeaponInfo*           m_WeaponInfo{}; //!< Ped active weapon's info
};
