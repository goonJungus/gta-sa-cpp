// CAnimBlendAssociation.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CAnimBlendAssociation. Adapted from gta-reversed for clean-room C++ build.
//
// All methods below are TODO stubs: verify each against the decomp
// (C:\Users\fufid\Documents\Decomps\gta-sa decomp\src\CAnimBlendAssociation\*.c)
// and gta-reversed/source/game_sa/Animation/AnimBlendAssociation.cpp before implementing.
// Priority: ctors/dtor, Init x3, AllocateAnimBlendNodeArray, Start,
// UpdateBlend/UpdateTime, SetBlend.

#include "CAnimBlendAssociation.h"
#include "CAnimBlendNode.h"

// 0x4CEB60 - kept out-of-line so the header never instantiates
// notsa::span<CAnimBlendNode>::operator[] over the incomplete type.
CAnimBlendNode* CAnimBlendAssociation::GetNode(int32_t nodeIndex) {
    return &GetNodes()[nodeIndex];
}

// TODO: implement CAnimBlendAssociation methods from decomp src/CAnimBlendAssociation/*.c:
//   CAnimBlendAssociation() / (RpClump*, CAnimBlendHierarchy*) /
//     (CAnimBlendAssociation&) / (CAnimBlendStaticAssociation&),
//   ~CAnimBlendAssociation,
//   GetTimeProgress,
//   AllocateAnimBlendNodeArray, FreeAnimBlendNodeArray,
//   Init(RpClump*, CAnimBlendHierarchy*), Init(CAnimBlendAssociation&),
//     Init(CAnimBlendStaticAssociation&),
//   ReferenceAnimBlock,
//   SetBlend, SetBlendTo, SetCurrentTime,
//   SetDeleteCallback, SetFinishCallback,
//   Start, SyncAnimation,
//   UpdateBlend, UpdateTime, UpdateTimeStep, HasFinished, GetHashKey,
//   GetNodes (span over m_BlendNodes).
// NOTE: the inline accessors (IsPlaying/IsLooped/..., GetBlendAmount, flags)
// are already defined in the header.
