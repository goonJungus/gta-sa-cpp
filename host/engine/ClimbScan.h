#pragma once
// CTaskSimpleClimb::ScanToGrab: probes the world with the retail climb col
// models (spheres in ped space) to find a handhold.

class GameWorld;

// mode 0: climb (airborne), 1: climb (launch, from a jump on the ground),
//      2: stand-up volume, 3: vault volume.
// ped = ped origin (pelvis, feet + 1.0); rot = retail rotation (forward is
// (-sin rot, cos rot)). Modes 0/1 return true with the handhold position and
// the heading that faces the wall; modes 2/3 return true when the volume hits
// something.
bool ClimbScan(const GameWorld& w, int mode, const float ped[3], float rot, float outPos[3], float* outRot);
