// CStreamingInfo.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CStreamingInfo/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CStreamingInfo.h"

// Static data members (game addresses recorded from gta-reversed StaticRef)
CStreamingInfo* CStreamingInfo::ms_pArrayBase{}; // game address: 0x9654B4 - Just a pointer to `CStreaming::ms_aInfoForModel`

void CStreamingInfo::Init() {
    // TODO: decomp src/CStreamingInfo/Init_*.c
}

CdStreamPos CStreamingInfo::GetCdPosn() const {
    // TODO: decomp src/CStreamingInfo/GetCdPosn_*.c
    return {};
}

void CStreamingInfo::SetCdPosnAndSize(uint32 offset, size_t CdSize) {
    // TODO: decomp src/CStreamingInfo/SetCdPosnAndSize_*.c
    (void)offset;
    (void)CdSize;
}

bool CStreamingInfo::GetCdPosnAndSize(CdStreamPos& CdPosn, size_t& CdSize) {
    // TODO: decomp src/CStreamingInfo/GetCdPosnAndSize_*.c
    (void)CdPosn;
    (void)CdSize;
    return false;
}

void CStreamingInfo::AddToList(CStreamingInfo* after) {
    // TODO: decomp src/CStreamingInfo/AddToList_*.c
    (void)after;
}

void CStreamingInfo::RemoveFromList() {
    // TODO: decomp src/CStreamingInfo/RemoveFromList_*.c
}

bool CStreamingInfo::InList() const {
    // TODO: decomp src/CStreamingInfo/InList_*.c
    return false;
}
