// CBaseModelInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CBaseModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method.)

#include "CBaseModelInfo.h"
#include "CModelInfo.h"   // ms_modelInfoPtrs, Get2dEffectStore
#include "CColModel.h"    // delete m_pColModel needs the complete type
#include "CTimeInfo.h"    // CTimeInfo* return type
#include "RenderWare.h"   // RpAtomicGetGeometry, Get2DEffectAtomic, 2dFX plugin helpers

#include <cstdint>
#include <cstring>

// ---- PORT(txdstores): CTxdStore not ported yet. Minimal interface used here;
//      replace with #include "CTxdStore.h" when the texture-dictionary
//      subsystem lands. Signatures from the decomp (0x4C4B40/0x4C4B80/0x4C4B90).
class CTxdStore {
public:
    static int32_t FindTxdSlot(const char* name);
    static int32_t AddTxdSlot(const char* name);
    static void AddRef(int32_t index);
    static void RemoveRef(int32_t index);
};

// ---- PORT(keygen): CKeyGen not ported yet. Minimal interface used here;
//      replace with #include "CKeyGen.h" when it lands.
class CKeyGen {
public:
    static uint32_t GetUppercaseKey(const char* str);
};

// 0x4C4A60
CBaseModelInfo::CBaseModelInfo() {
    // Decomp writes m_nRefCount = 0 (offset 8) and m_nTxdIndex = -1 (offset 10)
    // directly; expressed here via the named members.
    m_nRefCount = 0;
    ClearTexDictionary();
}

// 0x4C4A80
CAtomicModelInfo* CBaseModelInfo::AsAtomicModelInfoPtr() {
    return nullptr;
}

// 0x4C4A90
CDamageAtomicModelInfo* CBaseModelInfo::AsDamageAtomicModelInfoPtr() {
    return nullptr;
}

// 0x4C4AA0
CLodAtomicModelInfo* CBaseModelInfo::AsLodAtomicModelInfoPtr() {
    return nullptr;
}

// 0x4C4AB0
CTimeInfo* CBaseModelInfo::GetTimeInfo() {
    return nullptr;
}

// 0x4C4B10
void CBaseModelInfo::Init() {
    m_nRefCount = 0;
    m_pColModel = nullptr;
    ClearTexDictionary();
    Init2dEffects();
    m_nObjectInfoIndex = -1;
    m_fDrawDistance = 2000.0F;
    m_pRwObject = nullptr;

    // Decomp stores 0xC0 to the flags word: bIsBackfaceCulled | bIsLod.
    m_nFlags = 0;
    bIsBackfaceCulled = true;
    bIsLod = true;
}

// 0x4C4D50
void CBaseModelInfo::Shutdown() {
    DeleteRwObject();
    DeleteCollisionModel();

    bIsLod = true;
    m_nObjectInfoIndex = -1;
    ClearTexDictionary();
    Init2dEffects();
}

// 0x4C4AC0
void CBaseModelInfo::SetAnimFile(const char* filename) {
    // NOP in the original (base class has no anim file).
    (void)filename;
}

// 0x4C4AD0
void CBaseModelInfo::ConvertAnimFileIndex() {
    // NOP in the original.
}

// 0x4C4AE0
int32_t CBaseModelInfo::GetAnimFileIndex() {
    return -1;
}

// 0x4C4B40
// NOTE: the decomp uses FindTxdSlot + AddTxdSlot (NOT FindOrAddTxdSlot).
void CBaseModelInfo::SetTexDictionary(const char* txdName) {
    int32_t index = CTxdStore::FindTxdSlot(txdName);
    if (index == -1)
        index = CTxdStore::AddTxdSlot(txdName);
    m_nTxdIndex = static_cast<int16_t>(index);
}

// 0x4C4B70
void CBaseModelInfo::ClearTexDictionary() {
    m_nTxdIndex = -1;
}

// 0x4C4B80
void CBaseModelInfo::AddTexDictionaryRef() {
    CTxdStore::AddRef(m_nTxdIndex);
}

// 0x4C4B90
void CBaseModelInfo::RemoveTexDictionaryRef() {
    CTxdStore::RemoveRef(m_nTxdIndex);
}

// 0x4C4BA0
void CBaseModelInfo::AddRef() {
    ++m_nRefCount;
    AddTexDictionaryRef();
}

// 0x4C4BB0
void CBaseModelInfo::RemoveRef() {
    --m_nRefCount;
    RemoveTexDictionaryRef();
}

// 0x4C4BC0
void CBaseModelInfo::SetColModel(CColModel* colModel, bool bIsLodModel) {
    m_pColModel = colModel;
    if (!bIsLodModel) {
        bIsLod = false;
        return;
    }

    bIsLod = true;

    // When this is a LOD model, propagate the collision model to the paired
    // time model (if any). The decomp loops via GetTimeInfo(); the binary only
    // ever chains one level (GetTimeInfo()->GetOtherTimeModel()), matching
    // gta-reversed's flattened form.
    CTimeInfo* timeInfo = GetTimeInfo();
    if (!timeInfo)
        return;
    if (timeInfo->GetOtherTimeModel() == -1)
        return;

    CBaseModelInfo* lodInfo = CModelInfo::GetModelInfo(timeInfo->GetOtherTimeModel());
    lodInfo->m_pColModel = colModel;
    lodInfo->bIsLod = false;
}

// 0x4C4C20
void CBaseModelInfo::Init2dEffects() {
    m_n2dEffectIndex = -1;
    m_n2dfxCount = 0;
}

// 0x4C4C40
void CBaseModelInfo::DeleteCollisionModel() {
    // Decomp checks the raw bIsLod bit (0x80 of the flags low byte).
    if (m_pColModel && bIsLod)
        delete m_pColModel;

    m_pColModel = nullptr;
}

// 0x4C4C70
C2dEffect* CBaseModelInfo::Get2dEffect(int32_t index) const {
    uint32_t storedEffectsCount = m_n2dfxCount;
    RpGeometry* geometry = nullptr;

    // vtable+0x24 is GetRwModelType(); decomp branches on rpATOMIC (1) /
    // rpCLUMP (2) and reads the 2dFX plugin data off the geometry.
    if (GetRwObject()) {
        if (GetRwModelType() == rpATOMIC) {
            geometry = RpAtomicGetGeometry(GetRpAtomic());
        } else if (GetRwModelType() == rpCLUMP) {
#if 0 // TODO(2026-10-09): RW layer not ported - Get2DEffectAtomic(RpClump*) overload missing
            RpAtomic* atomic = Get2DEffectAtomic(GetRpClump());
            if (atomic)
                geometry = RpAtomicGetGeometry(atomic);
#endif
        }

        if (geometry)
            storedEffectsCount -= RpGeometryGet2dFxCount(geometry);
    }

    if (static_cast<int32_t>(storedEffectsCount) <= index) {
        // Effect lives in the geometry's 2dFX plugin data.
        return RpGeometryGet2dFxAtIndex(geometry, index - storedEffectsCount);
    }

    // Effect lives in the global 2dFX store. The decomp does raw byte math
    // ((m_n2dEffectIndex + index) * 0x40 + storeBase); C2dEffect is 0x40 bytes.
    return &CModelInfo::Get2dEffectStore()->GetItemAtIndex(m_n2dEffectIndex + index);
}

// 0x4C4D20
void CBaseModelInfo::Add2dEffect(C2dEffect* effect) {
    if (m_n2dEffectIndex >= 0) {
        ++m_n2dfxCount;
        return;
    }
    // Decomp: m_n2dEffectIndex = (effect - (storeBase + 4)) >> 6, i.e. the
    // effect's index in the 0x40-byte-stride global 2dFX store.
    const auto* base = reinterpret_cast<const char*>(CModelInfo::Get2dEffectStore());
    const auto* ptr  = reinterpret_cast<const char*>(effect);
    m_n2dEffectIndex = static_cast<int16_t>((ptr - base) / 0x40);
    m_n2dfxCount = 1;
}

// Was inline in the original; moved here because it needs CKeyGen.
void CBaseModelInfo::SetModelName(const char* modelName) {
    m_nKey = CKeyGen::GetUppercaseKey(modelName);
    g_HashToStringMap[m_nKey] = modelName;
}

// 0x5B3AD0
// Bit mapping verified against the decomp (raw flag-byte manipulation):
//   bDrawLast          <- dwFlags & 0x0000000C
//   bAdditiveRender    <- dwFlags & 0x00000008
//   bDontWriteZBuffer  <- dwFlags & 0x00000040
//   bDontCastShadowsOn <- dwFlags & 0x00000080
//   bIsBackfaceCulled  <- !(dwFlags & 0x00200000)
void SetBaseModelInfoFlags(CBaseModelInfo* modelInfo, uint32_t dwFlags) {
    modelInfo->bDrawLast           = (dwFlags & 0x0000000Cu) != 0;
    modelInfo->bAdditiveRender     = (dwFlags & 0x00000008u) != 0;
    modelInfo->bDontWriteZBuffer   = (dwFlags & 0x00000040u) != 0;
    modelInfo->bDontCastShadowsOn  = (dwFlags & 0x00000080u) != 0;
    modelInfo->bIsBackfaceCulled   = (dwFlags & 0x00200000u) == 0;
}

void CBaseModelInfo::SetBaseModelInfoFlags(uint32_t flags) {
    ::SetBaseModelInfoFlags(this, flags);
}
