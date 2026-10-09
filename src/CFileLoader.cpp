// CFileLoader - file-loading subsystem (IDE/IPL/COL/DFF/TXD parsers).
// Method bodies converted from the decompiled exports in src/CFileLoader/*.c.
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
//
// Conversion notes:
// - Ghidra types mapped: undefined4->uint32, undefined2->uint16,
//   undefined1/undefined->uint8, uchar/byte->uint8, ushort->uint16,
//   uint->uint32, ulonglong->uint64.
// - __cdecl/__thiscall/__fastcall stripped; MSVC SEH frame boilerplate
//   (the ExceptionList dance, CRT frame-handler cookie, uStack_4 security
//   cookie) removed - the compiler generates its own.
// - operator_new(N) -> explicit ::operator new(N) call; constructor invoked
//   as a function (X::X()) -> placement new on the tracked allocation.
// - Read-only .rdata string globals inlined: &DAT_00859c6c -> "r",
//   &DAT_0085a53c -> "rb". _sscanf/_strncpy -> sscanf/strncpy.
// - Methods that need not-yet-ported subsystems are converted but wrapped in
//   #if 0 with a TODO(port) naming the missing dependencies, so the tree
//   keeps building. Enable each block as its subsystem lands.
// - The nullsub_/unk_/set_/uses_ exports in src/CFileLoader/ are fragments
//   of other functions (CRT stubs, jump-table pieces) misattributed to this
//   namespace - deliberately not converted.

#include "CFileLoader.h"
#include "CColModel.h" // CColModel, CCollisionData, CBoundingBox (via ColTypes.h)

#include <cstdio>  // sscanf
#include <cstring> // strncpy
#include <new>     // placement new

// RenderWare opaque types used only as pointers (the RW layer is not ported yet)
struct RpClump;
struct RpAtomic;

// Line scratch buffer (was StaticRef<char[512]>(0xB71848) in the original)
char CFileLoader::ms_line[512]{};

// Callback scratch model id (declared `extern uint32& gAtomicModelId` in the
// header; the original bound it to a fixed game address)
static uint32 gAtomicModelId_storage{};
uint32& gAtomicModelId = gAtomicModelId_storage;

// Scratch line buffer for LoadLine(char*&, int32&) - the binary's static
// s_MemoryHeapBuffer (original size unverified; 512 matches ms_line usage)
static char s_MemoryHeapBuffer[512]{};

// CFileLoader::LoadBoundingBox - converted from src/CFileLoader/LoadBoundingBox_005374b0.c (decompiled @ 005374b0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
// Copies 10 dwords from the .col record into the CBoundingBox with the
// original's field reorder (bound sphere first in the file layout).
void CFileLoader::LoadBoundingBox(uint8* data, CBoundingBox& outBoundBox)
{
  uint32* in  = (uint32*)data;
  uint32* out = (uint32*)&outBoundBox;
  out[9] = in[0];
  out[6] = in[1];
  out[7] = in[2];
  out[8] = in[3];
  out[0] = in[4];
  out[1] = in[5];
  out[2] = in[6];
  out[3] = in[7];
  out[4] = in[8];
  out[5] = in[9];
}

// @@METHOD@@ CFileLoader::LoadLine
// CFileLoader::LoadLine - converted from src/CFileLoader/LoadLine_00536f80.c (decompiled @ 00536f80)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
char* CFileLoader::LoadLine(FILESTREAM file)
{
  uint8 *pbVar1;
  uint8 bVar2;
  char *pcVar4;
  char *pcVar5;
  
  pcVar5 = ms_line;
  if (!CFileMgr::ReadLine(file, ms_line, 0x200)) {
    return nullptr;
  }
  bVar2 = ms_line[0];
  if (ms_line[0] != '\0') {
    pcVar4 = ms_line;
    do {
      if ((uint8)*pcVar4 < 0x20 || *pcVar4 == 0x2c) {
        *pcVar4 = 0x20;
      }
      pbVar1 = (uint8 *)(pcVar4 + 1);
      pcVar4 = pcVar4 + 1;
      bVar2 = ms_line[0];
    } while (*pbVar1 != 0);
  }
  while ((bVar2 < 0x21 && (bVar2 != 0))) {
    pbVar1 = (uint8 *)(pcVar5 + 1);
    pcVar5 = pcVar5 + 1;
    bVar2 = *pbVar1;
  }
  return pcVar5;
}

// @@METHOD@@ CFileLoader::LoadLine
// CFileLoader::LoadLine - converted from src/CFileLoader/LoadLine_00536fe0.c (decompiled @ 00536fe0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
char* CFileLoader::LoadLine(char*& outLine, int32& outSize)
{
  int iVar1;
  uint8 bVar2;
  char *pcVar3;
  uint8 *pbVar4;
  
  pbVar4 = (uint8*)outLine;
  pcVar3 = s_MemoryHeapBuffer;
  if (outSize < 1 || *pbVar4 == 0) {
    pcVar3 = nullptr;
  }
  else {
    do {
      bVar2 = *pbVar4;
      if ((bVar2 == 10) || (bVar2 == 0)) break;
      if ((bVar2 < 0x20) || (bVar2 == 0x2c)) {
        *pcVar3 = 0x20;
      }
      else {
        *pcVar3 = bVar2;
      }
      iVar1 = outSize;
      pcVar3 = pcVar3 + 1;
      pbVar4 = pbVar4 + 1;
      outSize = iVar1 + -1;
    } while (0 < iVar1 + -1);
    outLine = (char*)(pbVar4 + 1);
    *pcVar3 = 0;
    pcVar3 = s_MemoryHeapBuffer;
    bVar2 = s_MemoryHeapBuffer[0];
    if ((uint8)s_MemoryHeapBuffer[0] < 0x21) {
      while (bVar2 != 0) {
        pbVar4 = (uint8 *)(pcVar3 + 1);
        pcVar3 = pcVar3 + 1;
        bVar2 = *pbVar4;
        if (0x20 < *pbVar4) {
          return pcVar3;
        }
      }
    }
  }
  return pcVar3;
}

// --- Declared in CFileLoader.h but with no decompiled export in src/CFileLoader/ ---
// TODO: verify against a future decomp export or re-derive from the binary.

char* CFileLoader::FindFirstNonNullOrWS(char* it) {
    // TODO: decomp src/CFileLoader/ (no export found)
    (void)it;
    return nullptr;
}

char* CFileLoader::FindFirstNullOrWS(char* it) {
    // TODO: decomp src/CFileLoader/ (no export found)
    (void)it;
    return nullptr;
}

const char* GetFilename(const char* filepath) {
    // TODO: decomp src/CFileLoader/ (no export found)
    (void)filepath;
    return nullptr;
}

void LoadingScreenLoadingFile(const char* str) {
    // TODO: decomp src/CFileLoader/ (no export found)
    (void)str;
}

RwTexture* AddTextureCB(RwTexture* texture, void* dict) {
    // TODO: decomp src/CFileLoader/ (no export found)
    (void)texture; (void)dict;
    return nullptr;
}

RpAtomic* CloneAtomicToClumpCB(RpAtomic* atomic, void* data) {
    // TODO: decomp src/CFileLoader/ (no export found)
    (void)atomic; (void)data;
    return nullptr;
}

// Null-sub in the original binary (single RET @ 005b3ac0); the header declares
// it with a filename argument. Kept for API compatibility.
void CFileLoader::ReloadObjectTypes(const char* arg1) {
    (void)arg1;
}

// @@METHOD@@ CFileLoader::AddTexDictionaries
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::AddTexDictionaries - converted from src/CFileLoader/AddTexDictionaries_005b3910.c (decompiled @ 005b3910)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::AddTexDictionaries(RwTexDictionary* dictionary, RwTexDictionary* baseDictionary)
{
  RwTexDictionaryForAllTextures(baseDictionary,AddTextureCB,dictionary);
  return;
}
#endif

// @@METHOD@@ CFileLoader::FindRelatedModelInfoCB
#if 0 // TODO(port): enable when ported (CDamageAtomicModelInfo, CModelInfo, CVisibilityPlugins)
// CFileLoader::FindRelatedModelInfoCB - converted from src/CFileLoader/FindRelatedModelInfoCB_005b3930.c (decompiled @ 005b3930)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
RpAtomic* CFileLoader::FindRelatedModelInfoCB(RpAtomic* atomic, void* data)
{
  RpAtomic *atomic_00;
  char *name;
  CBaseModelInfo *pCVar1;
  int *piVar2;
  CDamageAtomicModelInfo *this;
  uint32 uVar3;
  char *objName;
  RpAtomic **bIsDamageModel;
  int iStack_1c;
  char local_18 [24];
  
  atomic_00 = atomic;
  bIsDamageModel = &atomic;
  objName = local_18;
  name = GetFrameNodeName((atomic->object).object.parent);
  GetNameAndDamage(name,objName,(bool *)bIsDamageModel);
  pCVar1 = CModelInfo::GetModelInfo(local_18,&iStack_1c);
  if (pCVar1 != (CBaseModelInfo *)0x0) {
    piVar2 = (int *)(**(code **)((int)pCVar1->vtable + 4))();
    CVisibilityPlugins::SetAtomicRenderCallback(atomic_00,(void *)0x0);
    if ((char)atomic == '\0') {
      (**(code **)(*piVar2 + 0x3c))(atomic_00);
    }
    else {
      this = (CDamageAtomicModelInfo *)(**(code **)(*piVar2 + 8))();
      CDamageAtomicModelInfo::SetDamagedAtomic(this,atomic_00);
    }
    RpClumpRemoveAtomic(data,atomic_00);
    uVar3 = RwFrameCreate();
    RpAtomicSetFrame(atomic_00,uVar3);
    CVisibilityPlugins::SetModelInfoIndex(atomic_00,iStack_1c);
  }
  return atomic_00;
}
#endif

// @@METHOD@@ CFileLoader::FinishLoadClumpFile
#if 0 // TODO(port): enable when ported (CModelInfo, CVehicleModelInfo)
// CFileLoader::FinishLoadClumpFile - converted from src/CFileLoader/FinishLoadClumpFile_00537450.c (decompiled @ 00537450)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
bool CFileLoader::FinishLoadClumpFile(RwStream* stream, uint32 modelIndex)
{
  char cVar1;
  int iVar2;
  
  cVar1 = (**(code **)((int)CModelInfo::ms_modelInfoPtrs[modelIndex]->vtable + 0x10))();
  if (cVar1 == '\x06') {
    CVehicleModelInfo::UseCommonVehicleTexDicationary();
  }
  iVar2 = RpClumpGtaStreamRead2(stream);
  if (cVar1 == '\x06') {
    CVehicleModelInfo::StopUsingCommonVehicleTexDicationary();
  }
  if (iVar2 != 0) {
    (**(code **)((int)CModelInfo::ms_modelInfoPtrs[modelIndex]->vtable + 0x40))(iVar2);
    return true;
  }
  return false;
}
#endif

// @@METHOD@@ CFileLoader::Load2dEffect
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CGeneral, CMemoryMgr, CModelInfo, CTxdStore)
// CFileLoader::Load2dEffect - converted from src/CFileLoader/Load2dEffect_005b7670.c (decompiled @ 005b7670)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::Load2dEffect(const char* line)
{
  char *pcVar1;
  C2dEffect *effect;
  CBaseModelInfo *this;
  uint8 uVar2;
  char cVar3;
  int iVar4;
  int *piVar5;
  char *pcVar6;
  RwTexture *pRVar7;
  uint32 *puVar8;
  uint32 local_1cc;
  float local_1c8;
  float local_1c4;
  float local_1c0;
  int local_1bc;
  RwRGBA RStack_1b8;
  uint32 uStack_1b4;
  uint32 uStack_1b0;
  uint32 uStack_1ac;
  uint32 uStack_1a8;
  uint32 uStack_1a4;
  uint32 uStack_1a0;
  uint32 uStack_19c;
  uint32 uStack_198;
  uint32 uStack_194;
  uint32 uStack_190;
  uint32 uStack_18c;
  uint32 uStack_188;
  uint8 auStack_184 [4];
  uint16 auStack_180 [2];
  uint8 auStack_17c [4];
  uint32 uStack_178;
  uint32 uStack_174;
  uint32 uStack_170;
  uint32 uStack_16c;
  uint32 uStack_168;
  uint32 uStack_164;
  uint32 uStack_160;
  uint32 uStack_15c;
  uint32 uStack_158;
  uint32 uStack_154;
  uint8 auStack_150 [4];
  uint32 uStack_14c;
  uint32 uStack_148;
  uint32 uStack_144;
  float fStack_12c;
  RwTexture *pRStack_128;
  char acStack_124 [4];
  int iStack_120;
  float fStack_11c;
  uint8 auStack_118 [4];
  float fStack_114;
  RwRGBA RStack_110;
  float fStack_10c;
  RwRGBA RStack_108;
  float fStack_104;
  float fStack_100;
  float fStack_fc;
  RwRGBA RStack_f8;
  float fStack_f4;
  uint8 auStack_f0 [4];
  uint8 auStack_ec [4];
  uint32 uStack_e8;
  uint32 uStack_e4;
  uint32 uStack_e0;
  uint8 auStack_c8 [4];
  uint32 uStack_c4;
  uint32 uStack_c0;
  uint32 uStack_bc;
  uint8 auStack_a4 [4];
  uint32 uStack_a0;
  uint32 uStack_9c;
  uint32 uStack_98;
  char acStack_80 [128];
  
  sscanf(param_1,"%d %f %f %f %d",&local_1bc,&local_1c4,&local_1c8,&local_1c0,&local_1cc);
  CTxdStore::PushCurrentTxd();
  iVar4 = CTxdStore::FindTxdSlot("particle");
  CTxdStore::SetCurrentTxd(iVar4);
  this = CModelInfo::ms_modelInfoPtrs[local_1bc];
  piVar5 = (int *)CModelInfo::Get2dEffectStore();
  effect = (C2dEffect *)(piVar5 + *piVar5 * 0x10 + 1);
  *piVar5 = *piVar5 + 1;
  CBaseModelInfo::Add2dEffect(this,effect);
  (effect->m_Pos).x = local_1c4;
  (effect->m_Pos).y = local_1c8;
  (effect->m_Pos).z = local_1c0;
  effect->m_Type = (e2dEffectType)local_1cc;
  switch(local_1cc) {
  case 0:
    sscanf(param_1,"%d %f %f %f %d %d %d %d %d",&local_1bc,&local_1c4,&local_1c8,&local_1c0,
            &local_1cc,&RStack_1b8,&uStack_1b0,&uStack_1b4,&uStack_1a8);
    do {
      cVar3 = *param_1;
      param_1 = param_1 + 1;
    } while (cVar3 != '\"');
    pcVar6 = auStack_150;
    cVar3 = *param_1;
    while (cVar3 != '\"') {
      *pcVar6 = cVar3;
      pcVar1 = param_1 + 1;
      pcVar6 = pcVar6 + 1;
      param_1 = param_1 + 1;
      cVar3 = *pcVar1;
    }
    *pcVar6 = '\0';
    param_1 = param_1 + 1;
    do {
      cVar3 = *param_1;
      param_1 = param_1 + 1;
    } while (cVar3 != '\"');
    pcVar6 = auStack_ec;
    cVar3 = *param_1;
    while (cVar3 != '\"') {
      *pcVar6 = cVar3;
      pcVar1 = param_1 + 1;
      pcVar6 = pcVar6 + 1;
      param_1 = param_1 + 1;
      cVar3 = *pcVar1;
    }
    *pcVar6 = '\0';
    sscanf(param_1 + 1,"%f %f %f %f %d %d %d %d %d %d %d %d %d",
            (uint8 *)((int)&effect->__anon0 + 4),(uint8 *)((int)&effect->__anon0 + 8),
            (uint8 *)((int)&effect->__anon0 + 0xc),(uint8 *)((int)&effect->__anon0 + 0x10)
            ,&uStack_1ac,&uStack_1a4,&uStack_190,&uStack_198,&uStack_1a0,&uStack_18c,&uStack_19c,
            &uStack_188,&uStack_194);
    (effect->__anon0).light.m_color.green = (uint8)uStack_1b0;
    (effect->__anon0).light.m_color.red = RStack_1b8.red;
    (effect->__anon0).light.m_nShadowColorMultiplier = (uint8)uStack_1ac;
    (effect->__anon0).light.m_color.blue = (uint8)uStack_1b4;
    (effect->__anon0).light.m_color.alpha = (uint8)uStack_1a8;
    (effect->__anon0).light.m_nCoronaFlareType = (uint8)uStack_198;
    (effect->__anon0).light.m_nCoronaFlashType = (e2dCoronaFlashType)uStack_1a4;
    (effect->__anon0).light.m_bCoronaEnableReflection = (bool)(char)uStack_190;
    (effect->__anon0).light.offsetX = (char)uStack_19c;
    (effect->__anon0).light.__anon0.m_nFlags = (uint16)uStack_1a0;
    (effect->__anon0).light.m_nShadowZDistance = (char)uStack_18c;
    (effect->__anon0).light.offsetY = (char)uStack_188;
    (effect->__anon0).light.offsetZ = (char)uStack_194;
    pRVar7 = (RwTexture *)RenderWare::str_null_007f3ac0(auStack_150,0);
    (effect->__anon0).light.m_pCoronaTex = pRVar7;
    pRVar7 = (RwTexture *)RenderWare::str_null_007f3ac0(auStack_ec,0);
    (effect->__anon0).light.m_pShadowTex = pRVar7;
    if (((uint8)uStack_1a0 & 4) != 0) {
      pcVar6 = (effect->__anon0).particle.m_szName + 0x14;
      *pcVar6 = *pcVar6 & 0xfd;
      CTxdStore::PopCurrentTxd();
      return;
    }
    break;
  case 1:
    sscanf(param_1,"%d %f %f %f %d %s",&local_1bc,&local_1c4,&local_1c8,&local_1c0,&local_1cc,
            &effect->__anon0);
    CTxdStore::PopCurrentTxd();
    return;
  case 3:
    sscanf(param_1,"%d %f %f %f %d %d %f %f %f %f %f %f %f %f %f %d %d %s",&local_1bc,&local_1c4,
            &local_1c8,&local_1c0,&local_1cc,&RStack_1b8,&effect->__anon0,
            (uint8 *)((int)&effect->__anon0 + 4),(uint8 *)((int)&effect->__anon0 + 8),
            (uint8 *)((int)&effect->__anon0 + 0xc),(uint8 *)((int)&effect->__anon0 + 0x10)
            ,(uint8 *)((int)&effect->__anon0 + 0x14),
            (uint8 *)((int)&effect->__anon0 + 0x18),
            (uint8 *)((int)&effect->__anon0 + 0x1c),
            (uint8 *)((int)&effect->__anon0 + 0x20),&uStack_1b0,&uStack_1b4,acStack_80);
    (effect->__anon0).pedAttractor.m_nAttractorType = RStack_1b8.red;
    pcVar6 = acStack_80;
    do {
      cVar3 = *pcVar6;
      pcVar6[(int)effect + (0x38 - (int)acStack_80)] = cVar3;
      pcVar6 = pcVar6 + 1;
    } while (cVar3 != '\0');
    (effect->__anon0).pedAttractor.m_nPedExistingProbability = (uint8)uStack_1b0;
    (effect->__anon0).pedAttractor.field_36 = (uint8)uStack_1b4;
    CTxdStore::PopCurrentTxd();
    return;
  case 5:
    uStack_1a0 = 0xffffffff;
    uStack_194 = 0xffffffff;
    uStack_188 = 0xffffffff;
    uStack_19c = 0xffffffff;
    uStack_18c = 0xffffffff;
    uStack_198 = 0xffffffff;
    uStack_190 = 0xffffffff;
    uStack_1a4 = 0xffffffff;
    uStack_1ac = 0xffffffff;
    uStack_1a8 = 0xffffffff;
    uStack_1b4 = 0xffffffff;
    uStack_1b0 = 0xffffffff;
    RStack_1b8.red = 0xff;
    RStack_1b8.green = 0xff;
    RStack_1b8.blue = 0xff;
    RStack_1b8.alpha = 0xff;
    uStack_16c = 0xffffffff;
    uStack_168 = 0xffffffff;
    uStack_164 = 0xffffffff;
    uStack_15c = 0xffffffff;
    uStack_158 = 0xffffffff;
    uStack_154 = 0xffffffff;
    uStack_178 = 0xffffffff;
    uStack_174 = 0xffffffff;
    uStack_170 = 0xffffffff;
    auStack_150 = (uint8  [4])0xffffffff;
    uStack_14c = 0xffffffff;
    uStack_148 = 0xffffffff;
    uStack_160 = 0x32;
    sscanf(param_1,
            "%d %f %f %f %d %d %f %f %f %f %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d  %d %d %d %d  %d %d %d %d  %d %d %d %d"
            ,&local_1bc,&local_1c4,&local_1c8,&local_1c0,&local_1cc,&effect->__anon0,auStack_f0,
            auStack_118,auStack_17c,(uint8 *)((int)&effect->__anon0 + 0x20),auStack_184,
            auStack_180,&uStack_1a0,&uStack_160,&uStack_194,&uStack_188,&uStack_19c,&uStack_18c,
            &uStack_198,&uStack_190,&uStack_1a4,&uStack_1ac,&uStack_1a8,&uStack_1b4,&uStack_1b0,
            &RStack_1b8,&uStack_16c,&uStack_15c,&uStack_178,auStack_150,&uStack_168,&uStack_158,
            &uStack_174,&uStack_14c,&uStack_164,&uStack_154,&uStack_170,&uStack_148);
    uVar2 = CGeneral::unk_00821b40();
    (effect->__anon0).light.m_color.blue = uVar2;
    uVar2 = CGeneral::unk_00821b40();
    (effect->__anon0).light.m_color.alpha = uVar2;
    cVar3 = CGeneral::unk_00821b40();
    (effect->__anon0).particle.m_szName[4] = cVar3;
    (effect->__anon0).particle.m_szName[5] = auStack_184[0];
    (effect->__anon0).enEx.m_nFlags1 = (uint8)auStack_180[0];
    (effect->__anon0).light.m_color.green = (uint8)uStack_1a0;
    (effect->__anon0).enEx.m_nSkyColor = (uint8)uStack_160;
    (effect->__anon0).particle.m_szName[6] = (char)uStack_194;
    (effect->__anon0).particle.m_szName[7] = (char)uStack_188;
    (effect->__anon0).particle.m_szName[10] = (char)uStack_19c;
    (effect->__anon0).particle.m_szName[0xb] = (char)uStack_18c;
    (effect->__anon0).particle.m_szName[8] = (uint8)uStack_198;
    (effect->__anon0).particle.m_szName[9] = (char)uStack_190;
    (effect->__anon0).particle.m_szName[0xc] = (e2dCoronaFlashType)uStack_1a4;
    (effect->__anon0).particle.m_szName[0xd] = (uint8)uStack_1ac;
    (effect->__anon0).particle.m_szName[0x10] = (uint8)uStack_1a8;
    (effect->__anon0).particle.m_szName[0x11] = (uint8)uStack_1b4;
    (effect->__anon0).particle.m_szName[0xe] = (uint8)uStack_1b0;
    (effect->__anon0).particle.m_szName[0xf] = RStack_1b8.red;
    (effect->__anon0).particle.m_szName[0x12] = (char)uStack_16c;
    (effect->__anon0).particle.m_szName[0x15] = (char)uStack_15c;
    (effect->__anon0).light.m_nCoronaFlareType = (uint8)uStack_178;
    (effect->__anon0).light.offsetX = auStack_150[0];
    (effect->__anon0).particle.m_szName[0x13] = (char)uStack_168;
    (effect->__anon0).light.m_nCoronaFlashType = (e2dCoronaFlashType)uStack_158;
    (effect->__anon0).light.m_nShadowColorMultiplier = (uint8)uStack_174;
    (effect->__anon0).light.offsetY = (char)uStack_14c;
    (effect->__anon0).particle.m_szName[0x14] = (char)uStack_164;
    (effect->__anon0).light.m_bCoronaEnableReflection = uStack_154._0_1_;
    (effect->__anon0).light.m_nShadowZDistance = (char)uStack_170;
    (effect->__anon0).light.offsetZ = (char)uStack_148;
    CTxdStore::PopCurrentTxd();
    return;
  case 6:
    sscanf(param_1,"%d %f %f %f %d %f %f %f %f %f %f %f %d %d %s %d",&local_1bc,&local_1c4,
            &local_1c8,&local_1c0,&local_1cc,&effect->__anon0,
            (uint8 *)((int)&effect->__anon0 + 4),(uint8 *)((int)&effect->__anon0 + 8),
            (uint8 *)((int)&effect->__anon0 + 0xc),(uint8 *)((int)&effect->__anon0 + 0x10)
            ,(uint8 *)((int)&effect->__anon0 + 0x14),
            (uint8 *)((int)&effect->__anon0 + 0x18),auStack_180,auStack_184,auStack_150,
            auStack_17c);
    *(uint16 *)((int)&effect->__anon0 + 0x1c) = auStack_180[0];
    (effect->__anon0).enEx.m_nFlags1 = auStack_184[0];
    (effect->__anon0).enEx.m_nSkyColor = auStack_17c[0];
    strncpy((effect->__anon0).enEx.m_szInteriorName,auStack_150,8);
    CTxdStore::PopCurrentTxd();
    return;
  case 7:
    auStack_ec[0] = 0x20;
    auStack_150[0] = 0x20;
    puVar8 = (uint32 *)((int)auStack_ec + 1);
    for (iVar4 = 8; iVar4 != 0; iVar4 = iVar4 + -1) {
      *puVar8 = 0;
      puVar8 = puVar8 + 1;
    }
    auStack_a4[0] = 0x20;
    puVar8 = (uint32 *)((int)auStack_150 + 1);
    for (iVar4 = 8; iVar4 != 0; iVar4 = iVar4 + -1) {
      *puVar8 = 0;
      puVar8 = puVar8 + 1;
    }
    auStack_c8[0] = 0x20;
    puVar8 = (uint32 *)((int)auStack_a4 + 1);
    for (iVar4 = 8; iVar4 != 0; iVar4 = iVar4 + -1) {
      *puVar8 = 0;
      puVar8 = puVar8 + 1;
    }
    puVar8 = (uint32 *)((int)auStack_c8 + 1);
    for (iVar4 = 8; iVar4 != 0; iVar4 = iVar4 + -1) {
      *puVar8 = 0;
      puVar8 = puVar8 + 1;
    }
    sscanf(param_1,"%d %f %f %f %d %f %f %f %f %f %d %s %s %s %s",&local_1bc,&local_1c4,&local_1c8,
            &local_1c0,&local_1cc,&RStack_1b8,&uStack_1b0,&uStack_1b4,&uStack_1a8,&uStack_1ac,
            &uStack_1a4,auStack_ec,auStack_150,auStack_a4,auStack_c8);
    (effect->__anon0).light.m_color = RStack_1b8;
    *(uint32 *)((int)&effect->__anon0 + 4) = uStack_1b0;
    *(uint32 *)((int)&effect->__anon0 + 8) = uStack_1b4;
    *(uint32 *)((int)&effect->__anon0 + 0xc) = uStack_1a8;
    *(uint32 *)((int)&effect->__anon0 + 0x10) = uStack_1ac;
    (effect->__anon0).light.__anon0.m_nFlags = (uint16)uStack_1a4;
    pcVar6 = (char *)CMemoryMgr::Malloc(0x40,0);
    (effect->__anon0).roadsign.m_pText = pcVar6;
    *(uint8 (*) [4])pcVar6 = auStack_ec;
    *(uint32 *)(pcVar6 + 4) = uStack_e8;
    *(uint32 *)(pcVar6 + 8) = uStack_e4;
    *(uint32 *)(pcVar6 + 0xc) = uStack_e0;
    *(uint8 (*) [4])(pcVar6 + 0x10) = auStack_150;
    *(uint32 *)(pcVar6 + 0x14) = uStack_14c;
    *(uint32 *)(pcVar6 + 0x18) = uStack_148;
    *(uint32 *)(pcVar6 + 0x1c) = uStack_144;
    *(uint8 (*) [4])(pcVar6 + 0x20) = auStack_a4;
    *(uint32 *)(pcVar6 + 0x24) = uStack_a0;
    *(uint32 *)(pcVar6 + 0x28) = uStack_9c;
    *(uint32 *)(pcVar6 + 0x2c) = uStack_98;
    *(uint8 (*) [4])(pcVar6 + 0x30) = auStack_c8;
    *(uint32 *)(pcVar6 + 0x34) = uStack_c4;
    *(uint32 *)(pcVar6 + 0x38) = uStack_c0;
    *(uint32 *)(pcVar6 + 0x3c) = uStack_bc;
    (effect->__anon0).pedAttractor.m_vecForwardDir.y = 0.0;
    CTxdStore::PopCurrentTxd();
    return;
  case 8:
    sscanf(param_1,"%d %f %f %f %d %d",&local_1bc,&local_1c4,&local_1c8,&local_1c0,&local_1cc,
            &RStack_110);
    (effect->__anon0).light.m_color = RStack_110;
    CTxdStore::PopCurrentTxd();
    return;
  case 9:
    sscanf(param_1,"%d %f %f %f %d %f %f %d",&local_1bc,&local_1c4,&local_1c8,&local_1c0,&local_1cc
            ,&RStack_108,&fStack_100,acStack_124);
    (effect->__anon0).light.m_color = RStack_108;
    (effect->__anon0).light.m_fCoronaFarClip = fStack_100;
    (effect->__anon0).particle.m_szName[8] = acStack_124[0];
    CTxdStore::PopCurrentTxd();
    return;
  case 10:
    sscanf(param_1,"%d %f %f %f %d %f %f %f %f %f %f %f %f %f %d",&local_1bc,&local_1c4,&local_1c8,
            &local_1c0,&local_1cc,&RStack_f8,&fStack_11c,&fStack_fc,&fStack_114,&fStack_12c,
            &fStack_10c,&fStack_f4,&fStack_104,&pRStack_128,&iStack_120);
    (effect->__anon0).light.m_color = RStack_f8;
    (effect->__anon0).light.m_fCoronaSize = fStack_114;
    (effect->__anon0).pedAttractor.m_vecForwardDir.x = fStack_f4;
    (effect->__anon0).light.m_fCoronaFarClip = fStack_11c;
    (effect->__anon0).light.m_fPointlightRange = fStack_fc;
    (effect->__anon0).light.m_fShadowSize = fStack_12c;
    (effect->__anon0).pedAttractor.m_vecUseDir.z = fStack_10c;
    (effect->__anon0).pedAttractor.m_vecForwardDir.y = fStack_104;
    (effect->__anon0).light.m_pCoronaTex = pRStack_128;
    (effect->__anon0).pedAttractor.m_nAttractorType = iStack_120 != 0;
  }
  CTxdStore::PopCurrentTxd();
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadAnimatedClumpObject
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CKeyGen, CModelInfo)
// CFileLoader::LoadAnimatedClumpObject - converted from src/CFileLoader/LoadAnimatedClumpObject_005b40c0.c (decompiled @ 005b40c0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadAnimatedClumpObject(const char* line)
{
  uint8 *puVar1;
  int iVar2;
  CClumpModelInfo *this;
  uint32 uVar3;
  CBaseModelInfo *this_00;
  uint32 extraout_EDX;
  int unaff_EBX;
  char *pcVar4;
  char *pcVar5;
  bool bVar6;
  int local_4c;
  float local_48;
  char local_44 [4];
  uint32 local_40;
  uint8 local_3c;
  uint32 local_3b;
  uint32 local_37;
  uint16 local_33;
  uint8 local_31;
  char local_30 [24];
  char local_18 [24];
  
  local_3b = 0;
  local_40 = 0x6c6c756e;
  local_37 = 0;
  local_33 = 0;
  local_3c = 0;
  local_31 = 0;
  local_4c = -1;
  local_48 = 2000.0;
  iVar2 = sscanf(line,"%d %s %s %s %f %d",&local_4c,local_30,local_18,&local_40,&local_48,local_44)
  ;
  if (iVar2 != 6) {
    return -1;
  }
  this = CModelInfo::AddClumpModel(local_4c);
  uVar3 = CKeyGen::GetUppercaseKey(local_30);
  this->m_nKey = uVar3;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_18);
  this->m_fDrawDistance = local_48;
  (**(code **)((int)this->vtable + 0x30))(&local_40);
  CBaseModelInfo::SetBaseModelInfoFlags(this_00,(uint32)this);
  if ((extraout_EDX & 0x20) == 0) {
    puVar1 = &(this->__anon0).__anon0.m_nFlagsLowerByte;
    *puVar1 = *puVar1 & 0xfb;
  }
  else {
    puVar1 = &(this->__anon0).__anon0.m_nFlagsLowerByte;
    *puVar1 = *puVar1 | 4;
  }
  iVar2 = 5;
  bVar6 = true;
  pcVar4 = local_44;
  pcVar5 = "null";
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    bVar6 = *pcVar4 == *pcVar5;
    pcVar4 = pcVar4 + 1;
    pcVar5 = pcVar5 + 1;
  } while (bVar6);
  if (!bVar6) {
    puVar1 = &(this->__anon0).__anon0.m_nFlagsLowerByte;
    *puVar1 = *puVar1 | 1;
  }
  return unaff_EBX;
}
#endif

// @@METHOD@@ CFileLoader::LoadAtomicFile2Return
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadAtomicFile2Return - converted from src/CFileLoader/LoadAtomicFile2Return_00537060.c (decompiled @ 00537060)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
RpClump* CFileLoader::LoadAtomicFile2Return(const char* filename)
{
  uint32 uVar1;
  int iVar2;
  uint32 uVar3;
  
  uVar3 = 0;
  uVar1 = RwStreamOpen(2,1,filename);
  iVar2 = RwStreamFindChunk(uVar1,0x10,0,0);
  if (iVar2 != 0) {
    uVar3 = RpClumpStreamRead(uVar1);
  }
  RwStreamClose(uVar1,0);
  return uVar3;
}
#endif

// @@METHOD@@ CFileLoader::LoadAtomicFile
#if 0 // TODO(port): enable when ported (CModelInfo, CVehicleModelInfo, gAtomicModelId)
// CFileLoader::LoadAtomicFile - converted from src/CFileLoader/LoadAtomicFile_005371f0.c (decompiled @ 005371f0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
bool CFileLoader::LoadAtomicFile(RwStream* stream, uint32 modelId)
{
  bool bVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = (**(code **)((int)CModelInfo::ms_modelInfoPtrs[modelId]->vtable + 4))();
  bVar1 = false;
  if ((iVar2 != 0) && ((*(uint8 *)(iVar2 + 0x13) & 0x80) != 0)) {
    bVar1 = true;
    CVehicleModelInfo::UseCommonVehicleTexDicationary();
  }
  iVar3 = RwStreamFindChunk(stream,0x10,0,0);
  if (iVar3 != 0) {
    iVar3 = RpClumpStreamRead(stream);
    if (iVar3 == 0) {
      if (!bVar1) {
        return false;
      }
      CVehicleModelInfo::StopUsingCommonVehicleTexDicationary();
      return false;
    }
    gAtomicModelId = modelId;
    RpClumpForAllAtomics(iVar3,SetRelatedModelInfoCB,iVar3);
    RpClumpDestroy(iVar3);
  }
  if (*(int *)(iVar2 + 0x1c) == 0) {
    return false;
  }
  if (bVar1) {
    CVehicleModelInfo::StopUsingCommonVehicleTexDicationary();
  }
  return true;
}
#endif

// @@METHOD@@ CFileLoader::LoadAtomicFile
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadAtomicFile - converted from src/CFileLoader/LoadAtomicFile_005b39d0.c (decompiled @ 005b39d0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadAtomicFile(const char* filename)
{
  uint32 uVar1;
  int iVar2;
  
  uVar1 = RwStreamOpen(2,1,filename);
  iVar2 = RwStreamFindChunk(uVar1,0x10,0,0);
  if ((iVar2 != 0) && (iVar2 = RpClumpStreamRead(uVar1), iVar2 != 0)) {
    RpClumpForAllAtomics(iVar2,FindRelatedModelInfoCB,iVar2);
    RpClumpDestroy(iVar2);
  }
  RwStreamClose(uVar1,0);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadAudioZone
#if 0 // TODO(port): enable when ported (CAudioZones)
// CFileLoader::LoadAudioZone - converted from src/CFileLoader/LoadAudioZone_005b4d70.c (decompiled @ 005b4d70)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadAudioZone(const char* line)
{
  char name [8];
  CVector position;
  char name_00 [8];
  CVector min;
  CVector max;
  int iVar1;
  float unaff_ESI;
  uint32 local_34;
  uint32 local_30;
  bool local_2c [4];
  int local_28;
  uint32 local_24;
  uint32 local_20;
  uint32 local_1c;
  float local_18;
  float local_14;
  uint8 local_10 [16];
  
  iVar1 = sscanf(line,"%s %d %d %f %f %f %f %f %f",local_10,&local_24,&local_28,local_2c,&local_30,
                  &local_34,&local_18,&local_1c,&local_20);
  if (iVar1 == 9) {
    name_00[4] = (uint8)local_24;
    name_00[5] = local_24._1_1_;
    name_00[6] = local_24._2_1_;
    name_00[7] = local_24._3_1_;
    name_00._0_4_ = local_10;
    min.y = (float)local_34;
    min.x = (float)local_30;
    min.z = local_18;
    max.y = (float)local_20;
    max.x = (float)local_1c;
    max.z = unaff_ESI;
    CAudioZones::RegisterAudioBox
              (name_00,CONCAT31((int3)((uint32)local_28 >> 8),local_28 != 0),local_2c[0],min,max);
    return;
  }
  sscanf(line,"%s %d %d %f %f %f %f",local_10,&local_24,&local_28,local_2c,&local_30,&local_34,
          &local_14);
  name[4] = (uint8)local_24;
  name[5] = local_24._1_1_;
  name[6] = local_24._2_1_;
  name[7] = local_24._3_1_;
  name._0_4_ = local_10;
  position.y = (float)local_34;
  position.x = (float)local_30;
  position.z = local_14;
  CAudioZones::RegisterAudioSphere
            (name,CONCAT31((int3)((uint32)local_34 >> 8),local_28 != 0),local_2c[0],position,
             (float)line);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCarGenerator
#if 0 // TODO(port): enable when ported (CCarGenerator, CTheCarGenerators)
// CFileLoader::LoadCarGenerator - converted from src/CFileLoader/LoadCarGenerator_00537990.c (decompiled @ 00537990)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCarGenerator(CFileCarGenerator* carGen, int32 iplId)
{
  uint32 uVar1;
  
  uVar1 = CTheCarGenerators::CreateCarGenerator
                    (carGen->m_vecPosn,carGen->m_fAngle * 57.295776,carGen->m_nModelId,
                     (short)carGen->m_nPrimaryColor,(short)carGen->m_nSecondaryColor,
                     *(uint8 *)&carGen->__anon0 & 1,(uint8)carGen->m_nAlarmChance,
                     (uint8)carGen->m_nDoorLockChance,(uint16)carGen->m_nMinDelay,
                     (uint16)carGen->m_nMaxDelay,(uint8)iplId,(bool)(*(uint8 *)&carGen->__anon0 & 2))
  ;
  if (-1 < (int)uVar1) {
    CCarGenerator::SwitchOn(CTheCarGenerators::CarGeneratorArray + (uVar1 & 0xffff));
    return;
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCarGenerator
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadCarGenerator - converted from src/CFileLoader/LoadCarGenerator_005b4740.c (decompiled @ 005b4740)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCarGenerator(const char* line, int32 iplId)
{
  int iVar1;
  CFileCarGenerator local_30;
  
  iVar1 = sscanf(line,"%f %f %f %f %d %d %d %d %d %d %d %d",&local_30,&local_30.m_vecPosn.y,
                  &local_30.m_vecPosn.z,&local_30.m_fAngle,&local_30.m_nModelId,
                  &local_30.m_nPrimaryColor,&local_30.m_nSecondaryColor,&local_30.__anon0,
                  &local_30.m_nAlarmChance,&local_30.m_nDoorLockChance,&local_30.m_nMinDelay,
                  &local_30.m_nMaxDelay);
  if (iVar1 == 0xc) {
    LoadCarGenerator(&local_30,iplId);
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCarPathNode
#if 0 // TODO(port): enable when ported (CGeneral)
// CFileLoader::LoadCarPathNode - converted from src/CFileLoader/LoadCarPathNode_005b4380.c (decompiled @ 005b4380)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCarPathNode(const char* line, int32 objModelIndex, int32 pathEntryIndex, bool a4)
{
  short sVar1;
  uint32 uVar2;
  uint32 uVar3;
  uint32 uVar4;
  uint32 uVar5;
  uint32 uVar6;
  uint32 uVar7;
  undefined3 in_stack_00000011;
  char *pcVar8;
  uint32 *puVar9;
  float fVar10;
  float fVar11;
  uint32 uVar12;
  uint32 uVar13;
  uint32 local_34;
  uint32 local_30;
  uint32 local_2c;
  uint32 local_28;
  uint32 local_24;
  uint32 local_20;
  uint32 local_1c;
  uint8 local_18 [4];
  uint8 local_14 [4];
  uint8 local_10 [4];
  uint32 local_c;
  uint32 local_8;
  uint8 local_4 [4];
  
  puVar9 = &local_8;
  pcVar8 = "%d %d %d %f %f %f %f %d %d %d %d %f %d";
  local_30 = 0x3f800000;
  local_34 = 0;
  sscanf(line,"%d %d %d %f %f %f %f %d %d %d %d %f %d",puVar9,&local_c,local_4,local_10,local_14,
          local_18,&local_1c,&local_20,&local_24,&local_28,&local_2c,&local_30,&local_34);
  if (objModelIndex == -1) {
    uVar12 = 0;
    uVar13 = local_34;
    uVar2 = CGeneral::unk_00821b40(0);
    uVar6 = local_2c >> 1 & 0xffffff01;
    uVar7 = local_2c >> 2 & 0xffffff01;
    uVar3 = local_2c & 0xffffff01;
    sVar1 = CGeneral::unk_00821b40(local_1c,local_20,local_24,uVar3,uVar7,local_28,uVar6,_a4,uVar2);
    fVar11 = (float)(int)sVar1;
    sVar1 = CGeneral::unk_00821b40(pcVar8,puVar9,fVar11);
    fVar10 = (float)(int)sVar1;
    sVar1 = CGeneral::unk_00821b40(pcVar8,fVar10);
    nullsub_0044d2f0(pathEntryIndex,local_8,local_c,(float)(int)sVar1,fVar10,fVar11,local_1c,
                     local_20,local_24,uVar3,uVar7,local_28,uVar6,_a4,uVar2,uVar12,uVar13);
    return;
  }
  uVar13 = local_34;
  uVar2 = CGeneral::unk_00821b40(local_34);
  uVar6 = local_2c >> 1 & 0xffffff01;
  uVar7 = local_2c >> 2 & 0xffffff01;
  uVar3 = local_2c & 0xffffff01;
  uVar12 = CGeneral::unk_00821b40(local_1c,local_20,local_24,uVar3,uVar7,local_28,uVar6,_a4,uVar2);
  uVar4 = CGeneral::unk_00821b40(uVar12);
  uVar5 = CGeneral::unk_00821b40(uVar4);
  nullsub_0044d2c0(objModelIndex,pathEntryIndex,local_8,local_c,uVar5,uVar4,uVar12,local_1c,local_20
                   ,local_24,uVar3,uVar7,local_28,uVar6,_a4,uVar2,uVar13);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadClumpFile
#if 0 // TODO(port): enable when ported (CCollisionPlugin, CModelInfo, CVehicleModelInfo)
// CFileLoader::LoadClumpFile - converted from src/CFileLoader/LoadClumpFile_005372d0.c (decompiled @ 005372d0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
bool CFileLoader::LoadClumpFile(RwStream* stream, uint32 modelIndex)
{
  CClumpModelInfo *modelInfo;
  char cVar1;
  int iVar2;
  uint32 uVar3;
  int iVar4;
  uint32 uVar5;
  
  modelInfo = (CClumpModelInfo *)CModelInfo::ms_modelInfoPtrs[modelIndex];
  cVar1 = (**(code **)((int)modelInfo->vtable + 0x10))();
  if (((modelInfo->__anon0).__anon0.m_nFlagsLowerByte & 2) == 0) {
    iVar2 = RwStreamFindChunk(stream,0x10,0,0);
    if (iVar2 != 0) {
      if (cVar1 == '\x06') {
        CCollisionPlugin::SetModelInfo(modelInfo);
        CVehicleModelInfo::UseCommonVehicleTexDicationary();
      }
      iVar2 = RpClumpStreamRead(stream);
      if (cVar1 == '\x06') {
        CCollisionPlugin::SetModelInfo((CClumpModelInfo *)0x0);
        CVehicleModelInfo::StopUsingCommonVehicleTexDicationary();
      }
      if (iVar2 != 0) {
        (**(code **)((int)modelInfo->vtable + 0x40))(iVar2);
        if (modelIndex == 0x1fc) {
          *(uint8 *)&modelInfo[2].m_nKey = 2;
        }
        return true;
      }
    }
  }
  else {
    iVar2 = RpClumpCreate();
    uVar3 = RwFrameCreate();
    *(uint32 *)(iVar2 + 4) = uVar3;
    iVar4 = RwStreamFindChunk(stream,0x10,0,0);
    while( true ) {
      if (iVar4 == 0) {
        (**(code **)((int)modelInfo->vtable + 0x40))(iVar2);
        return true;
      }
      iVar4 = RpClumpStreamRead(stream);
      if (iVar4 == 0) break;
      uVar5 = _rwFrameCloneAndLinkClones(*(uint32 *)(iVar4 + 4));
      RwFrameAddChild(uVar3,uVar5);
      RpClumpForAllAtomics(iVar4,CloneAtomicToClumpCB,iVar2);
      RpClumpDestroy(iVar4);
      iVar4 = RwStreamFindChunk(stream,0x10,0,0);
    }
  }
  return false;
}
#endif

// @@METHOD@@ CFileLoader::LoadClumpFile
#if 0 // TODO(port): enable when ported (CModelInfo)
// CFileLoader::LoadClumpFile - converted from src/CFileLoader/LoadClumpFile_005b3a30.c (decompiled @ 005b3a30)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadClumpFile(const char* filename)
{
  uint32 uVar1;
  int iVar2;
  char *name;
  CBaseModelInfo *pCVar3;
  int *index;
  
  uVar1 = RwStreamOpen(2,1,filename);
  iVar2 = RwStreamFindChunk(uVar1,0x10,0,0);
  while (iVar2 != 0) {
    iVar2 = RpClumpStreamRead(uVar1);
    if (iVar2 != 0) {
      index = (int *)0x0;
      name = GetFrameNodeName(*(RwFrame **)(iVar2 + 4));
      pCVar3 = CModelInfo::GetModelInfo(name,index);
      if (pCVar3 == (CBaseModelInfo *)0x0) {
        RpClumpDestroy(iVar2);
      }
      else {
        (**(code **)((int)pCVar3->vtable + 0x40))();
      }
    }
    iVar2 = RwStreamFindChunk(uVar1,0x10,0,0);
  }
  RwStreamClose(uVar1,0);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadClumpObject
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CKeyGen, CModelInfo, CTempColModels)
// CFileLoader::LoadClumpObject - converted from src/CFileLoader/LoadClumpObject_005b4040.c (decompiled @ 005b4040)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadClumpObject(const char* line)
{
  int iVar1;
  CClumpModelInfo *this;
  uint32 uVar2;
  int local_34;
  char local_30 [24];
  char local_18 [24];
  
  local_34 = -1;
  iVar1 = sscanf(line,"%d %s %s",&local_34,local_30,local_18);
  if (iVar1 != 3) {
    return -1;
  }
  this = CModelInfo::AddClumpModel(local_34);
  uVar2 = CKeyGen::GetUppercaseKey(local_30);
  this->m_nKey = uVar2;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_18);
  CBaseModelInfo::SetColModel((CBaseModelInfo *)this,&CTempColModels::ms_colModelBBox,false);
  return local_34;
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionFileFirstTime
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CColAccel, CColModel, CColStore, CKeyGen, CModelInfo, CRT)
// CFileLoader::LoadCollisionFileFirstTime - converted from src/CFileLoader/LoadCollisionFileFirstTime_005b5000.c (decompiled @ 005b5000)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
bool CFileLoader::LoadCollisionFileFirstTime(uint8* data, uint32 dataSize, uint8 colId)
{
  uint32 uVar1;
  uint32 uVar3;
  int iVar4;
  uint32 uVar5;
  int *piVar6;
  CColModel *outColModel;
  int *piVar7;
  CBaseModelInfo *local_38;
  PackedModelStartEnd local_34;
  void *pvStack_30;
  int local_28;
  int local_24 [6];
  void *local_c;
  
  local_38 = (CBaseModelInfo *)0x0;
  uVar5 = dataSize;
  do {
    if (uVar5 < 9) {
      return true;
    }
    iVar4 = *(int *)data;
    local_28 = *(int *)((int)data + 4);
    if (iVar4 == 0x324c4f43) {
      dataSize = 1;
    }
    else if (iVar4 == 0x334c4f43) {
      dataSize = 2;
    }
    else {
      if (iVar4 != 0x4c4c4f43) {
        return true;
      }
      dataSize = 0;
    }
    piVar6 = (int *)((int)data + 8);
    piVar7 = local_24;
    for (iVar4 = 5; iVar4 != 0; iVar4 = iVar4 + -1) {
      *piVar7 = *piVar6;
      piVar6 = piVar6 + 1;
      piVar7 = piVar7 + 1;
    }
    *(short *)piVar7 = (short)*piVar6;
    local_34.__anon0.wModelEnd = 0;
    local_34.__anon0.wModelStart = *(uint16 *)((int)data + 0x1e);
    piVar6 = (int *)((int)data + 0x20);
    if ((uint32)local_34 < 20000) {
      local_38 = CModelInfo::ms_modelInfoPtrs[local_34.modelId];
    }
    if ((local_38 == (CBaseModelInfo *)0x0) ||
       (uVar1 = local_38->m_nKey, uVar3 = CKeyGen::GetUppercaseKey((char *)local_24), uVar1 != uVar3
       )) {
      local_38 = CModelInfo::GetModelInfo((char *)local_24,&local_34.modelId);
    }
    outColModel = (CColModel *)0x0;
    if ((local_38 != (CBaseModelInfo *)0x0) &&
       (CColStore::IncludeModelIndex(colId,local_34.modelId),
       (char)(local_38->__anon0).__anon0.m_nFlagsUpperByte < '\0')) {
      pvStack_30 = CColModel::operator new(0x30);
      if (pvStack_30 != (void *)0x0) {
        outColModel = (CColModel*)new (pvStack_30) CColModel();
      }
      if (dataSize == 0) {
        LoadCollisionModel((uint8 *)piVar6,outColModel);
      }
      else if (dataSize == 1) {
        LoadCollisionModelVer2((uint8 *)piVar6,local_28 - 0x18,outColModel,(char *)local_24);
      }
      else if (dataSize == 2) {
        LoadCollisionModelVer3((uint8 *)piVar6,local_28 - 0x18,outColModel,(char *)local_24);
      }
      outColModel->m_nColSlot = colId;
      CBaseModelInfo::SetColModel(local_38,outColModel,true);
      CColAccel::addCacheCol(local_34,outColModel);
    }
    uVar5 = uVar5 + (-8 - local_28);
    data = (uint8 *)(local_28 + -0x18 + (int)piVar6);
  } while( true );
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionFile
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CColModel, CKeyGen, CModelInfo, CPlantMgr, CRT)
// CFileLoader::LoadCollisionFile - converted from src/CFileLoader/LoadCollisionFile_00538440.c (decompiled @ 00538440)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
bool CFileLoader::LoadCollisionFile(uint8* data, uint32 dataSize, uint8 colId)
{
  uint32 uVar1;
  char cVar3;
  uint32 uVar4;
  uint32 uVar5;
  int iVar6;
  void *pvVar7;
  CColModel *colModel;
  int iVar8;
  CBaseModelInfo *this;
  uint32 *puVar9;
  uint32 *puVar10;
  uint32 local_34;
  uint32 local_24 [6];
  void *local_c;
  CColModel *local_4;
  
  local_4 = (CColModel *)0xffffffff;
  this = (CBaseModelInfo *)0x0;
  do {
    if (dataSize < 9) {
      return true;
    }
    uVar5 = *(uint32 *)data;
    uVar1 = *(uint32 *)((int)data + 4);
    if (uVar5 < 0x344c4f44) {
      if (uVar5 == 0x344c4f43) {
        local_34 = 3;
      }
      else if (uVar5 == 0x324c4f43) {
        local_34 = 1;
      }
      else {
        if (uVar5 != 0x334c4f43) {
          return true;
        }
        local_34 = 2;
      }
    }
    else {
      if (uVar5 != 0x4c4c4f43) {
        return true;
      }
      local_34 = 0;
    }
    puVar9 = (uint32 *)((int)data + 8);
    puVar10 = local_24;
    for (iVar8 = 5; iVar8 != 0; iVar8 = iVar8 + -1) {
      *puVar10 = *puVar9;
      puVar9 = puVar9 + 1;
      puVar10 = puVar10 + 1;
    }
    *(short *)puVar10 = (short)*puVar9;
    puVar9 = (uint32 *)((int)data + 0x20);
    if (*(uint16 *)((int)data + 0x1e) < 20000) {
      this = CModelInfo::ms_modelInfoPtrs[*(uint16 *)((int)data + 0x1e)];
    }
    if ((this == (CBaseModelInfo *)0x0) ||
       (uVar5 = this->m_nKey, uVar4 = CKeyGen::GetUppercaseKey((char *)local_24), uVar5 != uVar4)) {
      uVar5 = (uint32)colId;
      if (*(char *)(ms_pColPool[1] + uVar5) < '\0') {
        iVar8 = 0;
      }
      else {
        iVar8 = uVar5 * 0x2c + *ms_pColPool;
      }
      if (*(char *)(ms_pColPool[1] + uVar5) < '\0') {
        iVar6 = 0;
      }
      else {
        iVar6 = uVar5 * 0x2c + *ms_pColPool;
      }
      this = CModelInfo::GetModelInfo
                       ((char *)local_24,(int)*(short *)(iVar6 + 0x22),(int)*(short *)(iVar8 + 0x24)
                       );
    }
    if ((this != (CBaseModelInfo *)0x0) && ((char)(this->__anon0).__anon0.m_nFlagsUpperByte < '\0'))
    {
      colModel = this->m_pColModel;
      if (colModel == (CColModel *)0x0) {
        pvVar7 = CColModel::operator new(0x30);
        if (pvVar7 == (void *)0x0) {
          colModel = (CColModel *)0x0;
        }
        else {
          local_4 = colModel;
          colModel = (CColModel*)new (pvVar7) CColModel();
        }
        local_4 = (CColModel *)0xffffffff;
        CBaseModelInfo::SetColModel(this,colModel,true);
      }
      switch(local_34) {
      case 0:
        LoadCollisionModel((uint8 *)puVar9,colModel);
        break;
      case 1:
        LoadCollisionModelVer2((uint8 *)puVar9,uVar1 - 0x18,colModel,(char *)local_24);
        break;
      case 2:
        LoadCollisionModelVer3((uint8 *)puVar9,uVar1 - 0x18,colModel,(char *)local_24);
        break;
      case 3:
        LoadCollisionModelVer4((uint8 *)puVar9,uVar1 - 0x18,colModel,(char *)local_24);
      }
      colModel->m_nColSlot = colId;
      cVar3 = (**(code **)((int)this->vtable + 0x10))();
      if (cVar3 == '\x01') {
        CPlantMgr::SetPlantFriendlyFlagInAtomicMI((CAtomicModelInfo *)this);
      }
    }
    data = (uint8 *)((uVar1 - 0x18) + (int)puVar9);
    dataSize = dataSize + (-8 - uVar1);
  } while( true );
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionFile
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CColModel, CModelInfo, CRT)
// CFileLoader::LoadCollisionFile - converted from src/CFileLoader/LoadCollisionFile_005b4e60.c (decompiled @ 005b4e60)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCollisionFile(const char* filename, uint8 colId)
{
  CColModel *pCVar1;
  FILE *_File;
  uint32 uVar2;
  CBaseModelInfo *this;
  void *pvVar3;
  CColModel *pCVar4;
  CColModel *local_30;
  int local_2c;
  int local_28;
  char local_24 [24];
  
  _File = (FILE *)CFileMgr::OpenFile(filename,"r");
  uVar2 = CFileMgr::Read(_File,&local_2c,8);
  pCVar1 = local_30;
  while (uVar2 != 0) {
    if (local_2c == 0x324c4f43) {
      local_30 = (CColModel *)0x1;
    }
    else if (local_2c == 0x334c4f43) {
      local_30 = (CColModel *)0x2;
    }
    else if (local_2c == 0x4c4c4f43) {
      local_30 = (CColModel *)0x0;
    }
    CFileMgr::Read(_File,local_24,0x18);
    CFileMgr::Read(_File,&::buffer,local_28 - 0x18);
    this = CModelInfo::GetModelInfo(local_24,(int *)0x0);
    pCVar4 = pCVar1;
    if ((this != (CBaseModelInfo *)0x0) && ((char)(this->__anon0).__anon0.m_nFlagsUpperByte < '\0'))
    {
      pCVar4 = this->m_pColModel;
      if (pCVar4 == (CColModel *)0x0) {
        pvVar3 = CColModel::operator new(0x30);
        if (pvVar3 == (void *)0x0) {
          pCVar4 = (CColModel *)0x0;
        }
        else {
          pCVar4 = (CColModel*)new (pvVar3) CColModel();
        }
        CBaseModelInfo::SetColModel(this,pCVar4,true);
        pCVar4 = pCVar1;
      }
      if (local_30 == (CColModel *)0x0) {
        LoadCollisionModel(&::buffer,pCVar4);
      }
      else if (local_30 == (CColModel *)0x1) {
        LoadCollisionModelVer2(&::buffer,local_28 - 0x18,pCVar4,local_24);
      }
      else if (local_30 == (CColModel *)0x2) {
        LoadCollisionModelVer3(&::buffer,local_28 - 0x18,pCVar4,local_24);
      }
      pCVar4->m_nColSlot = colId;
    }
    uVar2 = CFileMgr::Read(_File,&local_2c,8);
    pCVar1 = pCVar4;
  }
  CFileMgr::CloseFile(_File);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionModelVer2
#if 0 // TODO(port): enable when ported (CBoundingBox, CMemoryMgr)
// CFileLoader::LoadCollisionModelVer2 - converted from src/CFileLoader/LoadCollisionModelVer2_00537ee0.c (decompiled @ 00537ee0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCollisionModelVer2(uint8* buffer, uint32 fileSize, CColModel& cm, const char* modelName)
{
  CCollisionData____anon0 *pCVar1;
  uint32 uVar2;
  uint8 bVar3;
  uint32 uVar4;
  CCollisionData *pCVar5;
  int iVar6;
  uint32 uVar7;
  float *pfVar8;
  uint8 *puVar9;
  float *pfVar10;
  CColModel *pCVar11;
  CCollisionData *pCVar12;
  float local_4c [6];
  float fStack_34;
  float fStack_30;
  float fStack_2c;
  float fStack_28;
  uint16 uStack_24;
  uint16 uStack_22;
  uint16 uStack_20;
  uint8 uStack_1e;
  uint8 bStack_1c;
  int iStack_18;
  int iStack_14;
  int iStack_10;
  int iStack_c;
  int iStack_8;
  
  CBoundingBox::CBoundingBox();
  pfVar8 = (float *)buffer;
  pfVar10 = local_4c;
  for (iVar6 = 0x13; iVar6 != 0; iVar6 = iVar6 + -1) {
    *pfVar10 = *pfVar8;
    pfVar8 = pfVar8 + 1;
    pfVar10 = pfVar10 + 1;
  }
  (cm->m_boundSphere).m_vecCenter.x = fStack_34;
  (cm->m_boundSphere).m_vecCenter.y = fStack_30;
  (cm->m_boundSphere).m_vecCenter.z = fStack_2c;
  (cm->m_boundSphere).m_fRadius = fStack_28;
  bVar3 = (cm->__anon0).m_nFlags;
  pfVar8 = local_4c;
  pCVar11 = cm;
  for (iVar6 = 6; iVar6 != 0; iVar6 = iVar6 + -1) {
    (pCVar11->m_boundBox).m_vecMin.x = *pfVar8;
    pfVar8 = pfVar8 + 1;
    pCVar11 = (CColModel *)&(pCVar11->m_boundBox).m_vecMin.y;
  }
  uVar4 = fileSize - 0x4c;
  (cm->__anon0).m_nFlags = bVar3 ^ (bStack_1c >> 1 ^ bVar3) & 1;
  if (uVar4 != 0) {
    pCVar5 = (CCollisionData *)CMemoryMgr::Malloc(fileSize - 0x1c,0);
    cm->m_pColData = pCVar5;
    puVar9 = buffer + 0x4c;
    pCVar12 = pCVar5 + 1;
    for (uVar7 = uVar4 >> 2; uVar7 != 0; uVar7 = uVar7 - 1) {
      uVar2 = *(uint32 *)puVar9;
      pCVar12->m_nNumSpheres = (short)uVar2;
      pCVar12->m_nNumBoxes = (short)((uint32)uVar2 >> 0x10);
      puVar9 = puVar9 + 4;
      pCVar12 = (CCollisionData *)&pCVar12->m_nNumTriangles;
    }
    for (uVar4 = uVar4 & 3; uVar4 != 0; uVar4 = uVar4 - 1) {
      *(uint8 *)&pCVar12->m_nNumSpheres = *puVar9;
      puVar9 = puVar9 + 1;
      pCVar12 = (CCollisionData *)((int)&pCVar12->m_nNumSpheres + 1);
    }
    cm->m_pColData->m_nNumSpheres = uStack_24;
    cm->m_pColData->m_nNumBoxes = uStack_22;
    cm->m_pColData->m_nNumLines = uStack_1e;
    cm->m_pColData->m_nNumTriangles = uStack_20;
    pCVar1 = &cm->m_pColData->__anon0;
    pCVar1->bits_bUsesDisks = pCVar1->bits_bUsesDisks & 0xfe;
    (cm->m_pColData->__anon0).bits_bUsesDisks = (cm->m_pColData->__anon0).bits_bUsesDisks & 0xfb;
    pCVar1 = &cm->m_pColData->__anon0;
    pCVar1->bits_bUsesDisks =
         pCVar1->bits_bUsesDisks ^ (bStack_1c >> 2 ^ (cm->m_pColData->__anon0).bits_bUsesDisks) & 2;
    if (iStack_18 == 0) {
      cm->m_pColData->m_pSpheres = (CColSphere *)0x0;
    }
    else {
      cm->m_pColData->m_pSpheres = (CColSphere *)(iStack_18 + -0x38 + (int)pCVar5);
    }
    if (iStack_14 == 0) {
      cm->m_pColData->m_pBoxes = (CColBox *)0x0;
    }
    else {
      cm->m_pColData->m_pBoxes = (CColBox *)(iStack_14 + -0x38 + (int)pCVar5);
    }
    if (iStack_10 == 0) {
      (cm->m_pColData->__anon1).m_pLines = (CColLine *)0x0;
    }
    else {
      (cm->m_pColData->__anon1).m_pLines = (CColLine *)(iStack_10 + -0x38 + (int)pCVar5);
    }
    if (iStack_c == 0) {
      cm->m_pColData->m_pVertices = (void *)0x0;
    }
    else {
      cm->m_pColData->m_pVertices = (void *)(iStack_c + -0x38 + (int)pCVar5);
    }
    if (iStack_8 == 0) {
      cm->m_pColData->m_pTriangles = (CColTriangle *)0x0;
    }
    else {
      cm->m_pColData->m_pTriangles = (CColTriangle *)(iStack_8 + -0x38 + (int)pCVar5);
    }
    cm->m_pColData->m_pTrianglePlanes = (CColTrianglePlane *)0x0;
    pCVar1 = &cm->m_pColData->__anon0;
    pCVar1->bits_bUsesDisks = pCVar1->bits_bUsesDisks & 0xfb;
    cm->m_pColData->m_pShadowVertices = (void *)0x0;
    cm->m_pColData->m_pShadowTriangles = (CColTriangle *)0x0;
    cm->m_pColData->m_nNumShadowVertices = 0;
    (cm->__anon0).m_nFlags = (cm->__anon0).m_nFlags | 2;
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionModelVer3
#if 0 // TODO(port): enable when ported (CBoundingBox, CMemoryMgr)
// CFileLoader::LoadCollisionModelVer3 - converted from src/CFileLoader/LoadCollisionModelVer3_00537ce0.c (decompiled @ 00537ce0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCollisionModelVer3(uint8* buffer, uint32 fileSize, CColModel& cm, const char* modelName)
{
  CCollisionData____anon0 *pCVar1;
  uint32 uVar2;
  CCollisionData *pCVar3;
  CCollisionData *pCVar4;
  int iVar5;
  uint32 uVar6;
  uint8 bVar7;
  uint32 uVar8;
  float *pfVar9;
  uint8 *puVar10;
  float *pfVar11;
  CColModel *pCVar12;
  float local_58 [6];
  float local_40;
  float local_3c;
  float local_38;
  float local_34;
  uint16 local_30;
  uint16 local_2e;
  uint16 local_2c;
  uint8 local_2a;
  uint8 local_28;
  int local_24;
  int local_20;
  int local_1c;
  int local_18;
  int local_14;
  uint32 local_c;
  int local_8;
  int local_4;
  
  CBoundingBox::CBoundingBox();
  pfVar9 = (float *)buffer;
  pfVar11 = local_58;
  for (iVar5 = 0x16; iVar5 != 0; iVar5 = iVar5 + -1) {
    *pfVar11 = *pfVar9;
    pfVar9 = pfVar9 + 1;
    pfVar11 = pfVar11 + 1;
  }
  (cm->m_boundSphere).m_vecCenter.x = local_40;
  (cm->m_boundSphere).m_vecCenter.y = local_3c;
  (cm->m_boundSphere).m_vecCenter.z = local_38;
  (cm->m_boundSphere).m_fRadius = local_34;
  pfVar9 = local_58;
  pCVar12 = cm;
  for (iVar5 = 6; iVar5 != 0; iVar5 = iVar5 + -1) {
    (pCVar12->m_boundBox).m_vecMin.x = *pfVar9;
    pfVar9 = pfVar9 + 1;
    pCVar12 = (CColModel *)&(pCVar12->m_boundBox).m_vecMin.y;
  }
  uVar8 = fileSize - 0x58;
  (cm->__anon0).m_nFlags = (cm->__anon0).m_nFlags ^ (local_28 >> 1 ^ (cm->__anon0).m_nFlags) & 1;
  if (uVar8 == 0) {
    return;
  }
  pCVar3 = (CCollisionData *)CMemoryMgr::Malloc(fileSize - 0x28,0);
  cm->m_pColData = pCVar3;
  puVar10 = buffer + 0x58;
  pCVar4 = pCVar3 + 1;
  for (uVar6 = uVar8 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
    uVar2 = *(uint32 *)puVar10;
    pCVar4->m_nNumSpheres = (short)uVar2;
    pCVar4->m_nNumBoxes = (short)((uint32)uVar2 >> 0x10);
    puVar10 = puVar10 + 4;
    pCVar4 = (CCollisionData *)&pCVar4->m_nNumTriangles;
  }
  for (uVar8 = uVar8 & 3; uVar8 != 0; uVar8 = uVar8 - 1) {
    *(uint8 *)&pCVar4->m_nNumSpheres = *puVar10;
    puVar10 = puVar10 + 1;
    pCVar4 = (CCollisionData *)((int)&pCVar4->m_nNumSpheres + 1);
  }
  cm->m_pColData->m_nNumSpheres = local_30;
  cm->m_pColData->m_nNumBoxes = local_2e;
  cm->m_pColData->m_nNumLines = local_2a;
  cm->m_pColData->m_nNumTriangles = local_2c;
  pCVar1 = &cm->m_pColData->__anon0;
  pCVar1->bits_bUsesDisks = pCVar1->bits_bUsesDisks & 0xfe;
  bVar7 = (cm->m_pColData->__anon0).bits_bUsesDisks;
  (cm->m_pColData->__anon0).bits_bUsesDisks = bVar7 ^ (local_28 >> 2 ^ bVar7) & 2;
  cm->m_pColData->m_nNumShadowTriangles = local_c;
  if (local_24 == 0) {
    cm->m_pColData->m_pSpheres = (CColSphere *)0x0;
  }
  else {
    cm->m_pColData->m_pSpheres = (CColSphere *)(local_24 + -0x44 + (int)pCVar3);
  }
  if (local_20 == 0) {
    cm->m_pColData->m_pBoxes = (CColBox *)0x0;
  }
  else {
    cm->m_pColData->m_pBoxes = (CColBox *)(local_20 + -0x44 + (int)pCVar3);
  }
  if (local_1c == 0) {
    (cm->m_pColData->__anon1).m_pLines = (CColLine *)0x0;
  }
  else {
    (cm->m_pColData->__anon1).m_pLines = (CColLine *)(local_1c + -0x44 + (int)pCVar3);
  }
  if (local_18 == 0) {
    cm->m_pColData->m_pVertices = (void *)0x0;
  }
  else {
    cm->m_pColData->m_pVertices = (void *)(local_18 + -0x44 + (int)pCVar3);
  }
  if (local_14 == 0) {
    cm->m_pColData->m_pTriangles = (CColTriangle *)0x0;
  }
  else {
    cm->m_pColData->m_pTriangles = (CColTriangle *)(local_14 + -0x44 + (int)pCVar3);
  }
  if (local_8 == 0) {
    cm->m_pColData->m_pShadowVertices = (void *)0x0;
  }
  else {
    cm->m_pColData->m_pShadowVertices = (void *)(local_8 + -0x44 + (int)pCVar3);
  }
  if (local_4 == 0) {
    cm->m_pColData->m_pShadowTriangles = (CColTriangle *)0x0;
  }
  else {
    cm->m_pColData->m_pShadowTriangles = (CColTriangle *)(local_4 + -0x44 + (int)pCVar3);
    if ((local_8 != 0) && (0 < (int)local_c)) {
      pCVar4 = cm->m_pColData;
      bVar7 = (pCVar4->__anon0).bits_bUsesDisks | 4;
      goto LAB_00537eae;
    }
  }
  pCVar4 = cm->m_pColData;
  bVar7 = (pCVar4->__anon0).bits_bUsesDisks & 0xfb;
LAB_00537eae:
  (pCVar4->__anon0).bits_bUsesDisks = bVar7;
  pCVar4 = cm->m_pColData;
  if (((pCVar4->__anon0).bits_bUsesDisks & 4) == 0) {
    pCVar4->m_nNumShadowVertices = 0;
  }
  else {
    uVar8 = Header::GetNoOfShdwVerts();
    pCVar4->m_nNumShadowVertices = uVar8;
  }
  cm->m_pColData->m_pTrianglePlanes = (CColTrianglePlane *)0x0;
  (cm->__anon0).m_nFlags = (cm->__anon0).m_nFlags | 2;
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionModelVer4
#if 0 // TODO(port): enable when ported (CBoundingBox, CMemoryMgr)
// CFileLoader::LoadCollisionModelVer4 - converted from src/CFileLoader/LoadCollisionModelVer4_00537ae0.c (decompiled @ 00537ae0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCollisionModelVer4(uint8* buffer, uint32 fileSize, CColModel& cm, const char* modelName)
{
  CCollisionData____anon0 *pCVar1;
  uint32 uVar2;
  CCollisionData *pCVar3;
  CCollisionData *pCVar4;
  int iVar5;
  uint32 uVar6;
  uint8 bVar7;
  uint32 uVar8;
  float *pfVar9;
  uint8 *puVar10;
  float *pfVar11;
  CColModel *pCVar12;
  float local_5c [6];
  float local_44;
  float local_40;
  float local_3c;
  float local_38;
  uint16 local_34;
  uint16 local_32;
  uint16 local_30;
  uint8 local_2e;
  uint8 local_2c;
  int local_28;
  int local_24;
  int local_20;
  int local_1c;
  int local_18;
  uint32 local_10;
  int local_c;
  int local_8;
  
  CBoundingBox::CBoundingBox();
  pfVar9 = (float *)buffer;
  pfVar11 = local_5c;
  for (iVar5 = 0x17; iVar5 != 0; iVar5 = iVar5 + -1) {
    *pfVar11 = *pfVar9;
    pfVar9 = pfVar9 + 1;
    pfVar11 = pfVar11 + 1;
  }
  (cm->m_boundSphere).m_vecCenter.x = local_44;
  (cm->m_boundSphere).m_vecCenter.y = local_40;
  (cm->m_boundSphere).m_vecCenter.z = local_3c;
  (cm->m_boundSphere).m_fRadius = local_38;
  pfVar9 = local_5c;
  pCVar12 = cm;
  for (iVar5 = 6; iVar5 != 0; iVar5 = iVar5 + -1) {
    (pCVar12->m_boundBox).m_vecMin.x = *pfVar9;
    pfVar9 = pfVar9 + 1;
    pCVar12 = (CColModel *)&(pCVar12->m_boundBox).m_vecMin.y;
  }
  uVar8 = fileSize - 0x5c;
  (cm->__anon0).m_nFlags = (cm->__anon0).m_nFlags ^ (local_2c >> 1 ^ (cm->__anon0).m_nFlags) & 1;
  if (uVar8 == 0) {
    return;
  }
  pCVar3 = (CCollisionData *)CMemoryMgr::Malloc(fileSize - 0x2c,0);
  cm->m_pColData = pCVar3;
  puVar10 = buffer + 0x5c;
  pCVar4 = pCVar3 + 1;
  for (uVar6 = uVar8 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
    uVar2 = *(uint32 *)puVar10;
    pCVar4->m_nNumSpheres = (short)uVar2;
    pCVar4->m_nNumBoxes = (short)((uint32)uVar2 >> 0x10);
    puVar10 = puVar10 + 4;
    pCVar4 = (CCollisionData *)&pCVar4->m_nNumTriangles;
  }
  for (uVar8 = uVar8 & 3; uVar8 != 0; uVar8 = uVar8 - 1) {
    *(uint8 *)&pCVar4->m_nNumSpheres = *puVar10;
    puVar10 = puVar10 + 1;
    pCVar4 = (CCollisionData *)((int)&pCVar4->m_nNumSpheres + 1);
  }
  cm->m_pColData->m_nNumSpheres = local_34;
  cm->m_pColData->m_nNumBoxes = local_32;
  cm->m_pColData->m_nNumLines = local_2e;
  cm->m_pColData->m_nNumTriangles = local_30;
  pCVar1 = &cm->m_pColData->__anon0;
  pCVar1->bits_bUsesDisks = pCVar1->bits_bUsesDisks & 0xfe;
  bVar7 = (cm->m_pColData->__anon0).bits_bUsesDisks;
  (cm->m_pColData->__anon0).bits_bUsesDisks = bVar7 ^ (local_2c >> 2 ^ bVar7) & 2;
  cm->m_pColData->m_nNumShadowTriangles = local_10;
  if (local_28 == 0) {
    cm->m_pColData->m_pSpheres = (CColSphere *)0x0;
  }
  else {
    cm->m_pColData->m_pSpheres = (CColSphere *)(local_28 + -0x48 + (int)pCVar3);
  }
  if (local_24 == 0) {
    cm->m_pColData->m_pBoxes = (CColBox *)0x0;
  }
  else {
    cm->m_pColData->m_pBoxes = (CColBox *)(local_24 + -0x48 + (int)pCVar3);
  }
  if (local_20 == 0) {
    (cm->m_pColData->__anon1).m_pLines = (CColLine *)0x0;
  }
  else {
    (cm->m_pColData->__anon1).m_pLines = (CColLine *)(local_20 + -0x48 + (int)pCVar3);
  }
  if (local_1c == 0) {
    cm->m_pColData->m_pVertices = (void *)0x0;
  }
  else {
    cm->m_pColData->m_pVertices = (void *)(local_1c + -0x48 + (int)pCVar3);
  }
  if (local_18 == 0) {
    cm->m_pColData->m_pTriangles = (CColTriangle *)0x0;
  }
  else {
    cm->m_pColData->m_pTriangles = (CColTriangle *)(local_18 + -0x48 + (int)pCVar3);
  }
  if (local_c == 0) {
    cm->m_pColData->m_pShadowVertices = (void *)0x0;
  }
  else {
    cm->m_pColData->m_pShadowVertices = (void *)(local_c + -0x48 + (int)pCVar3);
  }
  if (local_8 == 0) {
    cm->m_pColData->m_pShadowTriangles = (CColTriangle *)0x0;
  }
  else {
    cm->m_pColData->m_pShadowTriangles = (CColTriangle *)(local_8 + -0x48 + (int)pCVar3);
    if ((local_c != 0) && (0 < (int)local_10)) {
      pCVar4 = cm->m_pColData;
      bVar7 = (pCVar4->__anon0).bits_bUsesDisks | 4;
      goto LAB_00537cae;
    }
  }
  pCVar4 = cm->m_pColData;
  bVar7 = (pCVar4->__anon0).bits_bUsesDisks & 0xfb;
LAB_00537cae:
  (pCVar4->__anon0).bits_bUsesDisks = bVar7;
  pCVar4 = cm->m_pColData;
  if (((pCVar4->__anon0).bits_bUsesDisks & 4) == 0) {
    pCVar4->m_nNumShadowVertices = 0;
  }
  else {
    uVar8 = Header::GetNoOfShdwVerts();
    pCVar4->m_nNumShadowVertices = uVar8;
  }
  cm->m_pColData->m_pTrianglePlanes = (CColTrianglePlane *)0x0;
  (cm->__anon0).m_nFlags = (cm->__anon0).m_nFlags | 2;
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCollisionModel
#if 0 // TODO(port): enable when ported (CColBox, CColSphere, CCollisionData, CGeneral, CMemoryMgr)
// CFileLoader::LoadCollisionModel - converted from src/CFileLoader/LoadCollisionModel_00537580.c (decompiled @ 00537580)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCollisionModel(uint8* data, CColModel& outColModel)
{
  uint8 uVar1;
  tColLighting____anon0 tVar2;
  eSurfaceType eVar3;
  uint16 uVar4;
  float fVar5;
  float fVar6;
  uint16 uVar7;
  void *pvVar8;
  CCollisionData *pCVar9;
  CColSphere *pCVar10;
  CColBox *pCVar11;
  uint8 *puVar12;
  CColTriangle *pCVar13;
  int iVar14;
  int iVar15;
  uint16 *puVar16;
  float *pfVar17;
  int iStack_14;
  uint16 uStack_10;
  
  (outColModel->m_boundSphere).m_fRadius = *(float *)data;
  (outColModel->m_boundSphere).m_vecCenter.x = *(float *)(data + 4);
  (outColModel->m_boundSphere).m_vecCenter.y = *(float *)(data + 8);
  (outColModel->m_boundSphere).m_vecCenter.z = *(float *)(data + 0xc);
  (outColModel->m_boundBox).m_vecMin.x = *(float *)(data + 0x10);
  (outColModel->m_boundBox).m_vecMin.y = *(float *)(data + 0x14);
  (outColModel->m_boundBox).m_vecMin.z = *(float *)(data + 0x18);
  (outColModel->m_boundBox).m_vecMax.x = *(float *)(data + 0x1c);
  (outColModel->m_boundBox).m_vecMax.y = *(float *)(data + 0x20);
  (outColModel->m_boundBox).m_vecMax.z = *(float *)(data + 0x24);
  pvVar8 = operator new(0x30);
  iVar14 = 0;
  if (pvVar8 == (void *)0x0) {
    pCVar9 = (CCollisionData *)0x0;
  }
  else {
    pCVar9 = (CCollisionData*)new (pvVar8) CCollisionData();
  }
  outColModel->m_pColData = pCVar9;
  uVar4 = *(uint16 *)(data + 0x28);
  pfVar17 = (float *)(data + 0x2c);
  pCVar9->m_nNumSpheres = uVar4;
  if ((short)uVar4 < 1) {
    pCVar9->m_pSpheres = (CColSphere *)0x0;
  }
  else {
    pCVar10 = (CColSphere *)CMemoryMgr::Malloc((short)uVar4 * 0x14,0);
    iVar15 = 0;
    pCVar9->m_pSpheres = pCVar10;
    if (0 < (short)pCVar9->m_nNumSpheres) {
      do {
        CColSphere::Set((CColSphere *)((int)&(pCVar9->m_pSpheres->m_vecCenter).x + iVar14),*pfVar17,
                        (CVector *)(pfVar17 + 1),*(eSurfaceType *)(pfVar17 + 4),
                        *(uint8 *)((int)pfVar17 + 0x11),
                        (tColLighting)
                        (((tColLighting____anon0 *)((int)pfVar17 + 0x13))->__anon0).bits_day);
        pfVar17 = pfVar17 + 5;
        iVar15 = iVar15 + 1;
        iVar14 = iVar14 + 0x14;
      } while (iVar15 < (short)pCVar9->m_nNumSpheres);
    }
  }
  uVar1 = *(uint8 *)pfVar17;
  pfVar17 = pfVar17 + 1;
  pCVar9->m_nNumLines = uVar1;
  if ((char)uVar1 < '\x01') {
    (pCVar9->__anon1).m_pLines = (CColLine *)0x0;
  }
  else {
    pfVar17 = pfVar17 + (char)uVar1 * 6;
  }
  pCVar9->m_nNumLines = '\0';
  (pCVar9->__anon1).m_pLines = (CColLine *)0x0;
  uVar4 = *(uint16 *)pfVar17;
  pfVar17 = pfVar17 + 1;
  pCVar9->m_nNumBoxes = uVar4;
  if ((short)uVar4 < 1) {
    pCVar9->m_pBoxes = (CColBox *)0x0;
  }
  else {
    pCVar11 = (CColBox *)CMemoryMgr::Malloc((short)uVar4 * 0x1c,0);
    iVar14 = 0;
    pCVar9->m_pBoxes = pCVar11;
    if (0 < (short)pCVar9->m_nNumBoxes) {
      do {
        CColBox::Set(pfVar17,pfVar17 + 3,*(char *)(pfVar17 + 6),*(char *)((int)pfVar17 + 0x19),
                     *(char *)((int)pfVar17 + 0x1b));
        pfVar17 = pfVar17 + 7;
        iVar14 = iVar14 + 1;
      } while (iVar14 < (short)pCVar9->m_nNumBoxes);
    }
  }
  iVar14 = 0;
  fVar5 = *pfVar17;
  pfVar17 = pfVar17 + 1;
  if ((int)fVar5 < 1) {
    pCVar9->m_pVertices = (void *)0x0;
  }
  else {
    pvVar8 = (void *)CMemoryMgr::Malloc((int)fVar5 * 6,0);
    pCVar9->m_pVertices = pvVar8;
    puVar12 = (uint8 *)0x0;
    data = (uint8 *)0x0;
    if (3 < (int)fVar5) {
      iStack_14 = 3;
      do {
        puVar16 = (uint16 *)((int)pCVar9->m_pVertices + iVar14);
        uVar7 = CGeneral::unk_00821b40();
        *puVar16 = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[1] = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[2] = uVar7;
        puVar16 = (uint16 *)((int)pCVar9->m_pVertices + iVar14 + 6);
        uVar7 = CGeneral::unk_00821b40();
        *puVar16 = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[1] = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[2] = uVar7;
        puVar16 = (uint16 *)((int)pCVar9->m_pVertices + iVar14 + 0xc);
        uVar7 = CGeneral::unk_00821b40();
        *puVar16 = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[1] = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[2] = uVar7;
        puVar16 = (uint16 *)((int)pCVar9->m_pVertices + iVar14 + 0x12);
        uVar7 = CGeneral::unk_00821b40();
        *puVar16 = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[1] = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar12 = data + 4;
        puVar16[2] = uVar7;
        iStack_14 = iStack_14 + 4;
        iVar14 = iVar14 + 0x18;
        pfVar17 = pfVar17 + 0xc;
        data = puVar12;
      } while (iStack_14 < (int)fVar5);
    }
    if ((int)puVar12 < (int)fVar5) {
      iVar14 = (int)puVar12 * 6;
      data = (uint8 *)((int)fVar5 - (int)puVar12);
      do {
        puVar16 = (uint16 *)((int)pCVar9->m_pVertices + iVar14);
        uVar7 = CGeneral::unk_00821b40();
        *puVar16 = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[1] = uVar7;
        uVar7 = CGeneral::unk_00821b40();
        puVar16[2] = uVar7;
        pfVar17 = pfVar17 + 3;
        iVar14 = iVar14 + 6;
        data = data + -1;
      } while (data != (uint8 *)0x0);
    }
  }
  uVar4 = *(uint16 *)pfVar17;
  pfVar17 = pfVar17 + 1;
  pCVar9->m_nNumTriangles = uVar4;
  if ((short)uVar4 < 1) {
    pCVar9->m_pTriangles = (CColTriangle *)0x0;
  }
  else {
    pCVar13 = (CColTriangle *)CMemoryMgr::Malloc((int)(short)uVar4 << 3,0);
    iVar14 = 0;
    pCVar9->m_pTriangles = pCVar13;
    if (0 < (short)pCVar9->m_nNumTriangles) {
      do {
        fVar5 = pfVar17[2];
        tVar2 = *(tColLighting____anon0 *)((int)pfVar17 + 0xf);
        eVar3 = *(eSurfaceType *)(pfVar17 + 3);
        fVar6 = pfVar17[1];
        pCVar13 = pCVar9->m_pTriangles + iVar14;
        (pCVar13->__anon0).__anon0.vA = *(uint16 *)pfVar17;
        data._0_2_ = SUB42(fVar6,0);
        (pCVar13->__anon0).__anon0.vB = (uint16)data;
        uStack_10 = SUB42(fVar5,0);
        (pCVar13->__anon0).__anon0.vC = uStack_10;
        pCVar13->m_nMaterial = eVar3;
        (pCVar13->m_nLight).__anon0 = tVar2;
        iVar14 = iVar14 + 1;
        pfVar17 = pfVar17 + 4;
      } while (iVar14 < (short)pCVar9->m_nNumTriangles);
    }
    pCVar9->m_nNumTriangles = (uint16)iVar14;
  }
  (pCVar9->__anon0).bits_bUsesDisks = (pCVar9->__anon0).bits_bUsesDisks & 0xfb;
  pCVar9->m_pShadowTriangles = (CColTriangle *)0x0;
  pCVar9->m_pShadowVertices = (void *)0x0;
  outColModel->m_pColData->m_nNumShadowVertices = 0;
  if (((pCVar9->m_nNumSpheres != 0) || (pCVar9->m_nNumBoxes != 0)) || (pCVar9->m_nNumTriangles != 0)
     ) {
    (outColModel->__anon0).m_nFlags = (outColModel->__anon0).m_nFlags | 1;
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadCullZone
#if 0 // TODO(port): enable when ported (CCullZones)
// CFileLoader::LoadCullZone - converted from src/CFileLoader/LoadCullZone_005b4b40.c (decompiled @ 005b4b40)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadCullZone(const char* line)
{
  int iVar1;
  eZoneAttributes local_3c [2];
  float local_38;
  float local_34;
  float local_30;
  float local_2c;
  float local_28;
  float local_24 [3];
  float local_18;
  float local_14;
  float local_10;
  CVector local_c;
  
  local_24[1] = 0.0;
  iVar1 = sscanf(line,"%f %f %f %f %f %f %f %f %f %d %f %f %f %f",&local_c,&local_c.y,&local_c.z,
                  local_24,&local_28,&local_2c,&local_30,&local_34,&local_38,local_3c,&local_14,
                  &local_18,local_24 + 2,&local_10);
  if (iVar1 == 0xe) {
    CCullZones::AddMirrorAttributeZone
              (&local_c,local_24[0],local_28,local_2c,local_30,local_34,local_38,local_3c[0],
               local_10,local_14,local_18,local_24[2]);
    return;
  }
  sscanf(line,"%f %f %f %f %f %f %f %f %f %d %d",&local_c,&local_c.y,&local_c.z,local_24,&local_28,
          &local_2c,&local_30,&local_34,&local_38,local_3c,local_24 + 1);
  CCullZones::AddCullZone
            (&local_c,local_24[0],local_28,local_2c,local_30,local_34,local_38,local_3c[0]);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadEntryExit
#if 0 // TODO(port): enable when ported (CEntryExitManager)
// CFileLoader::LoadEntryExit - converted from src/CFileLoader/LoadEntryExit_005b8030.c (decompiled @ 005b8030)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadEntryExit(const char* line)
{
  char *pcVar1;
  char *name;
  int iVar2;
  CEntryExit__eFlags local_64;
  int local_60;
  int local_5c;
  int local_58;
  int local_54;
  eAreaCodes local_50;
  float local_4c;
  float local_48;
  float local_44;
  float local_40;
  float local_3c;
  float local_38;
  float local_34;
  float local_30;
  float local_2c;
  float local_28;
  float local_24;
  char local_20;
  char local_1f [31];
  
  local_60 = 2;
  local_58 = 0;
  local_5c = 0x18;
  sscanf(line,"%f %f %f %f %f %f %f %f %f %f %f %d %d %s %d %d %d %d",&local_24,&local_28,&local_2c
          ,&local_30,&local_34,&local_38,&local_3c,&local_40,&local_44,&local_48,&local_4c,&local_50
          ,&local_64,&local_20,&local_54,&local_60,&local_58,&local_5c);
  pcVar1 = _strrchr(&local_20,0x22);
  name = (char *)0x0;
  if (pcVar1 != (char *)0x0) {
    *pcVar1 = '\0';
    name = local_1f;
  }
  iVar2 = CEntryExitManager::AddOne
                    (local_24,local_28,local_2c,local_30,local_34,local_38,local_3c,local_40,
                     local_44,local_48,local_4c,local_50,local_64,local_54,local_58,local_5c,
                     local_60,name);
  if ((*(uint8 *)(iVar2 + mp_poolEntryExits[1]) & 0x80) == 0) {
    iVar2 = iVar2 * 0x3c + *mp_poolEntryExits;
  }
  else {
    iVar2 = 0;
  }
  if ((local_64 & UNKNOWN_INTERIOR) != 0) {
    *(uint16 *)(iVar2 + 0x30) = *(uint16 *)(iVar2 + 0x30) | 1;
  }
  if ((local_64 & UNKNOWN_PAIRING) != 0) {
    *(uint8 *)(iVar2 + 0x30) = *(uint8 *)(iVar2 + 0x30) | 2;
  }
  if ((local_64 & CREATE_LINKED_PAIR) != 0) {
    *(uint16 *)(iVar2 + 0x30) = *(uint16 *)(iVar2 + 0x30) | 4;
  }
  if ((local_64 & REWARD_INTERIOR) != 0) {
    *(uint16 *)(iVar2 + 0x30) = *(uint16 *)(iVar2 + 0x30) | 8;
  }
  if ((local_64 & USED_REWARD_ENTRANCE) != 0) {
    *(uint16 *)(iVar2 + 0x30) = *(uint16 *)(iVar2 + 0x30) | 0x10;
  }
  if ((local_64 & CARS_AND_AIRCRAFT) != 0) {
    *(uint16 *)(iVar2 + 0x30) = *(uint16 *)(iVar2 + 0x30) | 0x20;
  }
  if ((local_64 & BIKES_AND_MOTORCYCLES) != 0) {
    *(uint16 *)(iVar2 + 0x30) = *(uint16 *)(iVar2 + 0x30) | 0x40;
  }
  if ((char)(uint8)local_64 < '\0') {
    *(uint8 *)(iVar2 + 0x30) = *(uint8 *)(iVar2 + 0x30) | 0x80;
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadGarage
#if 0 // TODO(port): enable when ported (CGarages)
// CFileLoader::LoadGarage - converted from src/CFileLoader/LoadGarage_005b4530.c (decompiled @ 005b4530)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadGarage(const char* line)
{
  int iVar1;
  uint32 local_30;
  uint32 local_2c;
  uint32 local_28;
  uint32 local_24;
  uint32 local_20;
  uint32 local_1c;
  uint32 local_18;
  uint32 local_14;
  uint32 local_10;
  uint32 local_c;
  uint8 local_8 [8];
  
  iVar1 = sscanf(line,"%f %f %f %f %f %f %f %f %d %d %s",&local_c,&local_10,&local_14,&local_18,
                  &local_1c,&local_20,&local_24,&local_28,&local_30,&local_2c,local_8);
  if (iVar1 == 0xb) {
    CGarages::AddOne(local_c,local_10,local_14,local_18,local_1c,local_20,local_24,local_28,local_2c
                     ,0,local_8,local_30);
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadLevel
#if 0 // TODO(port): enable when ported (CColStore, CGeneral, CIplStore, CLoadingScreen, CModelInfo, CObjectData, CStreaming, CTrain, CVehicleModelInfo, gString)
// CFileLoader::LoadLevel - converted from src/CFileLoader/LoadLevel_005b9030.c (decompiled @ 005b9030)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadLevel(const char* filename)
{
  char cVar1;
  bool bVar2;
  int iVar3;
  FILE *_File;
  char *pcVar4;
  int iVar5;
  RwTexDictionary *pRVar6;
  char *pcVar7;
  CBaseModelInfo **ppCVar8;
  int *unaff_EDI;
  char *pcVar9;
  bool bVar10;
  uint32 uVar11;
  char local_40 [64];
  
  iVar3 = RwTexDictionaryGetCurrent();
  bVar2 = false;
  if (iVar3 == 0) {
    iVar3 = RwTexDictionaryCreate();
    RwTexDictionarySetCurrent(iVar3);
  }
  _File = (FILE *)CFileMgr::OpenFile(filename,"rb");
  pcVar4 = LoadLine((char **)_File,(int *)filename);
  while (pcVar4 != (char *)0x0) {
    if (*pcVar4 != '#') {
      iVar5 = _strncmp("EXIT",pcVar4,4);
      if (iVar5 == 0) break;
      iVar5 = _strncmp("TEXDICTION",pcVar4,10);
      if (iVar5 == 0) {
        pcVar4 = pcVar4 + 0xb;
        iVar5 = -(int)pcVar4;
        do {
          cVar1 = *pcVar4;
          pcVar4[(int)(local_40 + iVar5)] = cVar1;
          pcVar4 = pcVar4 + 1;
        } while (cVar1 != '\0');
        pcVar4 = _strrchr(local_40,0x5c);
        if (pcVar4 == (char *)0x0) {
          pcVar4 = local_40;
        }
        else {
          pcVar4 = pcVar4 + 1;
        }
        CGeneral::uses_flsbuf_00821bb5(gString,"Loading %s",pcVar4);
        LoadingScreen("Loading the Game",gString,(char *)0x0);
        pRVar6 = LoadTexDictionary(local_40);
        RwTexDictionaryForAllTextures(pRVar6,AddTextureCB,iVar3);
        RwTexDictionaryDestroy(pRVar6);
      }
      else {
        iVar5 = _strncmp((char *)&PTR_LAB_00869b60,pcVar4,3);
        if (iVar5 == 0) {
          pcVar9 = "=MODELS\\GTA_INT.IMG";
          iVar5 = 0x13;
          bVar10 = true;
          pcVar7 = pcVar4 + 4;
          do {
            pcVar9 = pcVar9 + 1;
            if (iVar5 == 0) break;
            iVar5 = iVar5 + -1;
            bVar10 = *pcVar7 == *pcVar9;
            pcVar7 = pcVar7 + 1;
          } while (bVar10);
          if (!bVar10) {
            CStreaming::AddImageToList(pcVar4 + 4,1);
          }
        }
        else {
          iVar5 = _strncmp("COLFILE",pcVar4,7);
          if (iVar5 == 0) {
            LoadingScreenLoadingFile(pcVar4 + 10);
            LoadCollisionFile(pcVar4 + 10,'\0');
          }
          else {
            iVar5 = _strncmp("MODELFILE",pcVar4,9);
            if (iVar5 == 0) {
              LoadingScreenLoadingFile(pcVar4 + 10);
              LoadAtomicFile(pcVar4 + 10);
            }
            else {
              iVar5 = _strncmp("HIERFILE",pcVar4,8);
              if (iVar5 == 0) {
                LoadingScreenLoadingFile(pcVar4 + 9);
                LoadClumpFile(pcVar4 + 9);
              }
              else {
                iVar5 = _strncmp((char *)&PTR_unk_00454449_00869b3c,pcVar4,3);
                if (iVar5 == 0) {
                  LoadingScreenLoadingFile(pcVar4 + 4);
                  LoadObjectTypes(pcVar4 + 4);
                }
                else {
                  iVar5 = _strncmp("IPL",pcVar4,3);
                  if (iVar5 == 0) {
                    if (!bVar2) {
                      MatchAllModelStrings();
                    }
                    pcVar4 = pcVar4 + 4;
                    iVar5 = -(int)pcVar4;
                    do {
                      cVar1 = *pcVar4;
                      pcVar4[(int)(local_40 + iVar5)] = cVar1;
                      pcVar4 = pcVar4 + 1;
                    } while (cVar1 != '\0');
                    if (!bVar2) {
                      LoadingScreenLoadingFile("Object Data");
                      uVar11 = 0;
                      pcVar4 = "DATA\\OBJECT.DAT";
                      CObjectData::Initialise("DATA\\OBJECT.DAT");
                      LoadingScreenLoadingFile("Setup vehicle info data",pcVar4,uVar11);
                      CVehicleModelInfo::SetupCommonData();
                      LoadingScreenLoadingFile("Streaming Init");
                      CStreaming::Init2();
                      CLoadingScreen::NewChunkLoaded();
                      LoadingScreenLoadingFile("Collision");
                      CColStore::LoadAllBoundingBoxes();
                      ppCVar8 = CModelInfo::ms_modelInfoPtrs;
                      do {
                        if (*ppCVar8 != (CBaseModelInfo *)0x0) {
                          (**(code **)((int)(*ppCVar8)->vtable + 0x34))();
                        }
                        ppCVar8 = ppCVar8 + 1;
                      } while ((int)ppCVar8 < 0xaae948);
                      bVar2 = true;
                    }
                    LoadingScreenLoadingFile(local_40);
                    LoadScene(local_40);
                  }
                  else {
                    _strncmp("SPLASH",pcVar4,6);
                  }
                }
              }
            }
          }
        }
      }
    }
    pcVar4 = LoadLine((char **)_File,unaff_EDI);
  }
  CFileMgr::CloseFile(_File);
  RwTexDictionarySetCurrent(iVar3);
  if (bVar2) {
    CIplStore::LoadAllRemainingIpls();
    CColStore::BoundingBoxesPostProcess();
    CTrain::InitTrains();
    CColStore::RemoveAllCollision();
    return;
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadObjectInstance
#if 0 // TODO(port): enable when ported (CAnimatedBuilding, CBuilding, CColStore, CDummyObject, CGlass, CMatrix, CModelInfo, CObject, CPlaceable, CRT, CRect, CSimpleTransform)
// CFileLoader::LoadObjectInstance - converted from src/CFileLoader/LoadObjectInstance_00538090.c (decompiled @ 00538090)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
CEntity* CFileLoader::LoadObjectInstance(CFileObjectInstance* objInstance, const char* modelName)
{
  float fVar1;
  float fVar2;
  uint8 uVar3;
  CBaseModelInfo *pCVar4;
  CColModel *pCVar5;
  bool bVar6;
  char cVar7;
  int iVar8;
  CRect *restriction;
  CRect *this;
  CSimpleTransform *pCVar9;
  int unaff_EBP;
  CEntity *entity;
  float10 fVar10;
  float fVar11;
  CQuaternion____anon0 local_30;
  uint8 auStack_20 [16];
  void *pvStack_10;
  void *local_c;
  uint32 local_4;
  
  local_4 = 0xffffffff;
  pCVar4 = CModelInfo::ms_modelInfoPtrs[objInstance->m_nModelId];
  entity = (CEntity *)0x0;
  if (pCVar4 == (CBaseModelInfo *)0x0) {
    return (CEntity *)0x0;
  }
  local_30.__anon0.imag.x = (float)pCVar4;
  if (pCVar4->m_nObjectInfoIndex == -1) {
    cVar7 = (**(code **)((int)pCVar4->vtable + 0x10))();
    if ((cVar7 == '\x05') && (((pCVar4->__anon0).__anon0.m_nFlagsLowerByte & 1) != 0)) {
      iVar8 = new(0x38);
      local_4 = 1;
      if (iVar8 == 0) {
LAB_0053819e:
        entity = (CEntity *)0x0;
      }
      else {
        entity = (CEntity *)CAnimatedBuilding::CAnimatedBuilding();
      }
    }
    else {
      iVar8 = new(0x38);
      local_4 = 2;
      if (iVar8 == 0) goto LAB_0053819e;
      entity = (CEntity *)CBuilding::CBuilding();
    }
    local_4 = 0xffffffff;
    (**(code **)((int)entity->vtable + 0x18))(objInstance->m_nModelId);
    if (((pCVar4->__anon0).__anon0.m_nFlagsUpperByte & 0x10) != 0) {
      (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags | 0x10000;
    }
    if (pCVar4->m_fDrawDistance < _unk_00858ca0) goto LAB_005381ce;
  }
  else {
    iVar8 = new(0x38);
    local_4 = 0;
    if (iVar8 != 0) {
      entity = (CEntity *)CDummyObject::CDummyObject();
    }
    local_4 = 0xffffffff;
    (**(code **)((int)entity->vtable + 0x18))(objInstance->m_nModelId);
    bVar6 = CGlass::IsObjectGlass(entity);
    if ((bVar6) &&
       (((CModelInfo::ms_modelInfoPtrs[(short)entity->m_nModelIndex]->__anon0).m_nFlags & 0x7800) !=
        0x2800)) {
LAB_005381ce:
      (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags & 0xffffff7f;
    }
  }
  if (((0.05 < ABS((objInstance->m_qRotation).__anon0.__anon0.imag.x)) ||
      (0.05 < ABS((objInstance->m_qRotation).__anon0.__anon0.imag.y))) ||
     ((((objInstance->__anon0).m_nInstanceType & 0x200) != 0 &&
      (((objInstance->m_qRotation).__anon0.__anon0.imag.x != 0.0 &&
       ((objInstance->m_qRotation).__anon0.__anon0.imag.y != 0.0)))))) {
    (objInstance->m_qRotation).__anon0.__anon0.imag.x =
         -(objInstance->m_qRotation).__anon0.__anon0.imag.x;
    (objInstance->m_qRotation).__anon0.__anon0.imag.y =
         -(objInstance->m_qRotation).__anon0.__anon0.imag.y;
    (objInstance->m_qRotation).__anon0.__anon0.imag.z =
         -(objInstance->m_qRotation).__anon0.__anon0.imag.z;
    CPlaceable::AllocateStaticMatrix((CPlaceable *)entity);
    local_30.__anon0.imag.z = (objInstance->m_qRotation).__anon0.__anon0.imag.z;
    local_30.__anon0.real = (objInstance->m_qRotation).__anon0.__anon0.real;
    local_30.__anon0.imag.x = (objInstance->m_qRotation).__anon0.__anon0.imag.x;
    local_30.__anon0.imag.y = (objInstance->m_qRotation).__anon0.__anon0.imag.y;
    if (entity->m_matrix == (CMatrixLink *)0x0) {
      CPlaceable::AllocateMatrix((CPlaceable *)entity);
      CSimpleTransform::UpdateMatrix(entity->m_matrix);
    }
    CMatrix::SetRotate((CMatrix *)entity->m_matrix,(CQuaternion *)&local_30.__anon0);
  }
  else {
    if (0.0 <= (objInstance->m_qRotation).__anon0.__anon0.imag.z) {
      fVar10 = (float10)FxInterpInfo_c::unk_00822380();
      fVar11 = (float)(fVar10 * (float10)-2.0);
      if (entity->m_matrix != (CMatrixLink *)0x0) {
        CMatrix::SetRotateZOnly((CMatrix *)entity->m_matrix,fVar11);
        goto LAB_005382ed;
      }
    }
    else {
      fVar10 = (float10)FxInterpInfo_c::unk_00822380();
      fVar11 = (float)(fVar10 + fVar10);
      if (entity->m_matrix != (CMatrixLink *)0x0) {
        CMatrix::SetRotateZOnly((CMatrix *)entity->m_matrix,fVar11);
        goto LAB_005382ed;
      }
    }
    (entity->m_placement).m_fHeading = fVar11;
  }
LAB_005382ed:
  fVar11 = (objInstance->m_vecPosition).z;
  fVar1 = (objInstance->m_vecPosition).y;
  fVar2 = (objInstance->m_vecPosition).x;
  if (entity->m_matrix == (CMatrixLink *)0x0) {
    (entity->m_placement).m_vPosn.x = fVar2;
    (entity->m_placement).m_vPosn.y = fVar1;
    (entity->m_placement).m_vPosn.z = fVar11;
  }
  else {
    (entity->m_matrix->m_pos).x = fVar2;
    (entity->m_matrix->m_pos).y = fVar1;
    (entity->m_matrix->m_pos).z = fVar11;
  }
  if (((objInstance->__anon0).m_nInstanceType & 0x400) != 0) {
    (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags | 0x100000;
  }
  if (((objInstance->__anon0).m_nInstanceType & 0x800) != 0) {
    (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags | 0x40000000;
  }
  if (((objInstance->__anon0).m_nInstanceType & 0x1000) != 0) {
    (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags | 0x80000000;
  }
  if (((objInstance->__anon0).m_nInstanceType & 0x100) != 0) {
    (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags | 0x20000000;
  }
  entity->m_AreaCode = *(uint8 *)&objInstance->__anon0;
  entity->__anon2 = (CEntity____anon2)objInstance->m_nLodInstanceIndex;
  if (objInstance->m_nModelId == (uint32)MI_TRAINCROSSING) {
    if (entity->m_matrix == (CMatrixLink *)0x0) {
      CPlaceable::AllocateMatrix((CPlaceable *)entity);
      CSimpleTransform::UpdateMatrix(entity->m_matrix);
    }
    CPlaceable::AllocateStaticMatrix((CPlaceable *)entity);
    if (entity->m_matrix == (CMatrixLink *)0x0) {
      CPlaceable::AllocateMatrix((CPlaceable *)entity);
      CSimpleTransform::UpdateMatrix(entity->m_matrix);
    }
    CObject::SetMatrixForTrainCrossing((CMatrix *)entity->m_matrix,1.3508849);
  }
  pCVar5 = CModelInfo::ms_modelInfoPtrs[(short)entity->m_nModelIndex]->m_pColModel;
  if (pCVar5 != (CColModel *)0x0) {
    if (((pCVar5->__anon0).m_nFlags & 1) == 0) {
      (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags & 0xfffffffe;
    }
    else {
      uVar3 = pCVar5->m_nColSlot;
      if (uVar3 != '\0') {
        restriction = (CRect *)(**(code **)((int)entity->vtable + 0x24))(auStack_20);
        this = (CRect *)CColStore::GetBoundingBox(uVar3);
        CRect::Restrict(this,restriction);
      }
    }
    if (entity->m_matrix == (CMatrixLink *)0x0) {
      pCVar9 = &entity->m_placement;
    }
    else {
      pCVar9 = (CSimpleTransform *)&entity->m_matrix->m_pos;
    }
    if (*(float *)(*(int *)(unaff_EBP + 0x14) + 8) + (pCVar9->m_vPosn).z < 0.0) {
      (entity->__anon0).m_nFlags = (entity->__anon0).m_nFlags | 0x100000;
    }
  }
  return entity;
}
#endif

// @@METHOD@@ CFileLoader::LoadObjectInstance
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadObjectInstance - converted from src/CFileLoader/LoadObjectInstance_00538690.c (decompiled @ 00538690)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
CEntity* CFileLoader::LoadObjectInstance(const char* line)
{
  CEntity *pCVar1;
  char local_40 [24];
  CFileObjectInstance local_28;
  
  sscanf(line,"%d %s %d %f %f %f %f %f %f %f %d",&local_28.m_nModelId,local_40,&local_28.__anon0,
          &local_28,&local_28.m_vecPosition.y,&local_28.m_vecPosition.z,&local_28.m_qRotation,
          (uint8 *)((int)&local_28.m_qRotation.__anon0 + 4),
          (uint8 *)((int)&local_28.m_qRotation.__anon0 + 8),
          (uint8 *)((int)&local_28.m_qRotation.__anon0 + 0xc),&local_28.m_nLodInstanceIndex);
  pCVar1 = LoadObjectInstance(&local_28,local_40);
  return pCVar1;
}
#endif

// @@METHOD@@ CFileLoader::LoadObjectTypes
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadObjectTypes - converted from src/CFileLoader/LoadObjectTypes_005b8400.c (decompiled @ 005b8400)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadObjectTypes(const char* filename)
{
  char cVar1;
  FILE *_File;
  char *line;
  int iVar2;
  int iVar3;
  int pathEntryIndex;
  int *unaff_EDI;
  int iVar4;
  bool a4;
  int local_10c;
  int local_108;
  int local_104;
  char local_100 [256];
  
  pathEntryIndex = -1;
  local_10c = -1;
  local_104 = 0x7ffffff;
  _File = (FILE *)CFileMgr::OpenFile(filename,"r");
  iVar3 = -(int)filename;
  iVar4 = 0;
  do {
    cVar1 = *filename;
    filename[(int)(local_100 + iVar3)] = cVar1;
    filename = filename + 1;
  } while (cVar1 != '\0');
  line = LoadLine((char **)_File,unaff_EDI);
  iVar3 = local_108;
  do {
    if (line == (char *)0x0) {
      CFileMgr::CloseFile(_File);
      return;
    }
    cVar1 = *line;
    if ((cVar1 == '\0') || (cVar1 == '#')) goto switchD_005b85d5_caseD_2;
    if (iVar4 == 0) {
      if ((((cVar1 == 'o') && (line[1] == 'b')) && (line[2] == 'j')) && (line[3] == 's')) {
        iVar4 = 1;
      }
      else if (((cVar1 == 't') && (line[1] == 'o')) && ((line[2] == 'b' && (line[3] == 'j')))) {
        iVar4 = 3;
      }
      else if (((cVar1 == 'w') && (line[1] == 'e')) && ((line[2] == 'a' && (line[3] == 'p')))) {
        iVar4 = 4;
      }
      else if ((((cVar1 == 'h') && (line[1] == 'i')) && (line[2] == 'e')) && (line[3] == 'r')) {
        iVar4 = 5;
      }
      else if (((cVar1 == 'a') && (line[1] == 'n')) && ((line[2] == 'i' && (line[3] == 'm')))) {
        iVar4 = 6;
      }
      else if (((cVar1 == 'c') && (line[1] == 'a')) && ((line[2] == 'r' && (line[3] == 's')))) {
        iVar4 = 7;
      }
      else if (cVar1 == 'p') {
        if (((line[1] == 'e') && (line[2] == 'd')) && (line[3] == 's')) {
          iVar4 = 8;
        }
        else {
          if (((line[1] != 'a') || (line[2] != 't')) || (line[3] != 'h')) goto LAB_005b857f;
          iVar4 = 9;
        }
      }
      else {
LAB_005b857f:
        if (((cVar1 == '2') && (line[1] == 'd')) && ((line[2] == 'f' && (line[3] == 'x')))) {
          iVar4 = 10;
        }
        else if ((((cVar1 == 't') && (line[1] == 'x')) && (line[2] == 'd')) && (line[3] == 'p')) {
          iVar4 = 0xb;
        }
      }
      goto switchD_005b85d5_caseD_2;
    }
    if (((cVar1 == 'e') && (line[1] == 'n')) && (line[2] == 'd')) {
      iVar4 = 0;
      goto switchD_005b85d5_caseD_2;
    }
    switch(iVar4) {
    case 1:
      iVar2 = LoadObject(line);
      break;
    default:
      goto switchD_005b85d5_caseD_2;
    case 3:
      iVar2 = LoadTimeObject(line);
      break;
    case 4:
      iVar2 = LoadWeaponObject(line);
      break;
    case 5:
      iVar2 = LoadClumpObject(line);
      break;
    case 6:
      iVar2 = LoadAnimatedClumpObject(line);
      break;
    case 7:
      iVar2 = LoadVehicleObject(line);
      break;
    case 8:
      iVar2 = LoadPedObject(line);
      break;
    case 9:
      if (pathEntryIndex == -1) {
        iVar3 = LoadPathHeader(line,&local_108);
        pathEntryIndex = 0;
      }
      else {
        if (local_108 == 0) {
          LoadPedPathNode(line,iVar3,pathEntryIndex);
        }
        else {
          if (local_108 == 1) {
            a4 = false;
          }
          else {
            if (local_108 != 2) goto LAB_005b86a4;
            a4 = true;
          }
          LoadCarPathNode(line,iVar3,pathEntryIndex,a4);
        }
LAB_005b86a4:
        pathEntryIndex = pathEntryIndex + 1;
        if (pathEntryIndex == 0xc) {
          pathEntryIndex = -1;
        }
      }
      goto switchD_005b85d5_caseD_2;
    case 10:
      Load2dEffect(line);
      goto switchD_005b85d5_caseD_2;
    case 0xb:
      LoadTXDParent(line);
      goto switchD_005b85d5_caseD_2;
    }
    if (iVar2 != -1) {
      if (local_10c < iVar2) {
        local_10c = iVar2;
      }
      if (iVar2 < local_104) {
        local_104 = iVar2;
      }
    }
switchD_005b85d5_caseD_2:
    line = LoadLine((char **)_File,unaff_EDI);
  } while( true );
}
#endif

// @@METHOD@@ CFileLoader::LoadObject
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CKeyGen, CModelInfo)
// CFileLoader::LoadObject - converted from src/CFileLoader/LoadObject_005b3c60.c (decompiled @ 005b3c60)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadObject(const char* line)
{
  int iVar1;
  CDamageAtomicModelInfo *this;
  uint32 uVar2;
  int local_48;
  float local_44;
  uint32 local_40;
  int local_3c;
  uint8 local_38 [4];
  uint8 local_34 [4];
  char local_30 [24];
  char local_18 [24];
  
  iVar1 = sscanf(line,"%d %s %s %f %d",&local_48,local_30,local_18,&local_44,&local_40);
  if ((iVar1 != 5) || (local_44 < 4.0)) {
    iVar1 = sscanf(line,"%d %s %s %d",&local_48,local_30,local_18,&local_3c);
    if (iVar1 != 4) {
      return -1;
    }
    if (local_3c == 1) {
      sscanf(line,"%d %s %s %d %f %d",&local_48,local_30,local_18,&local_3c,&local_44,&local_40);
    }
    else if (local_3c == 2) {
      sscanf(line,"%d %s %s %d %f %f %d",&local_48,local_30,local_18,&local_3c,&local_44,local_38,
              &local_40);
    }
    else if (local_3c == 3) {
      sscanf(line,"%d %s %s %d %f %f %f %d",&local_48,local_30,local_18,&local_3c,&local_44,
              local_38,local_34,&local_40);
    }
  }
  if ((local_40 & 0x1000) == 0) {
    this = (CDamageAtomicModelInfo *)CModelInfo::AddAtomicModel(local_48);
  }
  else {
    this = CModelInfo::AddDamageAtomicModel(local_48);
  }
  this->m_fDrawDistance = local_44;
  uVar2 = CKeyGen::GetUppercaseKey(local_30);
  this->m_nKey = uVar2;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_18);
  SetAtomicModelInfoFlags((CAtomicModelInfo *)this,local_40);
  return local_48;
}
#endif

// @@METHOD@@ CFileLoader::LoadOcclusionVolume
#if 0 // TODO(port): enable when ported (COcclusion)
// CFileLoader::LoadOcclusionVolume - converted from src/CFileLoader/LoadOcclusionVolume_005b4c80.c (decompiled @ 005b4c80)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadOcclusionVolume(const char* line, const char* filename)
{
  char *pcVar1;
  char cVar2;
  char *pcVar3;
  bool local_2c;
  uint32 local_28;
  float local_24;
  float local_20;
  float local_1c;
  float local_18;
  float local_14;
  float local_10;
  float local_c;
  float local_8;
  float local_4;
  
  local_20 = 0.0;
  local_24 = 0.0;
  local_28 = 0;
  sscanf(line,"%f %f %f %f %f %f %f %f %f %d ",&local_c,&local_8,&local_4,&local_10,&local_14,
          &local_1c,&local_18,&local_20,&local_24,&local_28);
  local_4 = local_1c * 0.5 + local_4;
  local_2c = false;
  pcVar1 = filename + 1;
  pcVar3 = filename;
  do {
    cVar2 = *pcVar3;
    pcVar3 = pcVar3 + 1;
  } while (cVar2 != '\0');
  if (((filename[(int)(pcVar3 + (-7 - (int)pcVar1))] == 'i') &&
      (filename[(int)(pcVar3 + (-6 - (int)pcVar1))] == 'n')) &&
     (filename[(int)(pcVar3 + (-5 - (int)pcVar1))] == 't')) {
    local_2c = true;
  }
  COcclusion::AddOne(local_c,local_8,local_4,local_10,local_14,local_1c,local_18,local_20,local_24,
                     local_28,local_2c);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadPathHeader
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadPathHeader - converted from src/CFileLoader/LoadPathHeader_005b41c0.c (decompiled @ 005b41c0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadPathHeader(const char* line, int32& outPathType)
{
  int local_58;
  uint8 local_54 [84];
  
  sscanf(line,"%d %d %s",outPathType,&local_58,local_54);
  return local_58;
}
#endif

// @@METHOD@@ CFileLoader::LoadPedObject
#if 0 // TODO(port): enable when ported (CAEPedSpeechAudioEntity, CAnimManager, CBaseModelInfo, CKeyGen, CModelInfo, CPedStats, CPedType, CPopulation, CTempColModels)
// CFileLoader::LoadPedObject - converted from src/CFileLoader/LoadPedObject_005b7420.c (decompiled @ 005b7420)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadPedObject(const char* line)
{
  uint8 bVar1;
  uint32 uVar2;
  eAudioPedType eVar3;
  short sVar4;
  CPedModelInfo *this;
  uint32 uVar5;
  ePedType eVar6;
  ePedStats eVar7;
  uint8 *pbVar8;
  int iVar9;
  ePedRace eVar10;
  uint16 extraout_var;
  uint8 *pbVar11;
  int unaff_EBX;
  AssocGroupId AVar12;
  AssocGroupId groupId;
  bool bVar13;
  int local_128;
  char local_124 [4];
  uint16 local_120 [2];
  uint16 local_11c [2];
  uint8 local_118 [4];
  uint8 local_114 [12];
  char acStack_108 [4];
  char local_104 [20];
  char acStack_f0 [4];
  uint8 local_ec [16];
  char acStack_dc [4];
  uint8 local_d8 [20];
  uint8 abStack_c4 [24];
  char acStack_ac [4];
  uint8 local_a8 [24];
  char local_90 [20];
  char acStack_7c [4];
  uint8 local_78 [56];
  char acStack_40 [4];
  uint8 local_3c [60];
  
  uVar2 = CAnimManager::ms_numAnimAssocDefinitions;
  local_128 = -1;
  sscanf(line,"%d %s %s %s %s %s %x %x %s %d %d %s %s %s",&local_128,local_104,local_90,local_a8,
          local_d8,abStack_c4 + 4,local_118,local_11c,local_114,local_120,local_124,local_ec,
          local_78,local_3c);
  this = CModelInfo::AddPedModel(local_128);
  uVar5 = CKeyGen::GetUppercaseKey(local_104);
  this->m_nKey = uVar5;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_90);
  (**(code **)((int)this->vtable + 0x30))(local_114);
  CBaseModelInfo::SetColModel((CBaseModelInfo *)this,&CTempColModels::ms_colModelPed1,false);
  eVar6 = CPedType::FindPedType(acStack_ac);
  this->m_nPedType = eVar6;
  eVar7 = CPedStats::GetPedStatType(acStack_dc);
  this->m_nStatType = eVar7;
  groupId = ANIM_GROUP_DEFAULT;
  AVar12 = uVar2;
  if (0 < (int)CAnimManager::ms_numAnimAssocDefinitions) {
    do {
      pbVar8 = (uint8 *)CAnimManager::GetAnimGroupName(groupId);
      pbVar11 = abStack_c4;
      do {
        bVar1 = *pbVar11;
        bVar13 = bVar1 < *pbVar8;
        if (bVar1 != *pbVar8) {
LAB_005b7548:
          iVar9 = (1 - (uint32)bVar13) - (uint32)(bVar13 != 0);
          goto LAB_005b754d;
        }
        if (bVar1 == 0) break;
        bVar1 = pbVar11[1];
        bVar13 = bVar1 < pbVar8[1];
        if (bVar1 != pbVar8[1]) goto LAB_005b7548;
        pbVar11 = pbVar11 + 2;
        pbVar8 = pbVar8 + 2;
      } while (bVar1 != 0);
      iVar9 = 0;
LAB_005b754d:
      AVar12 = groupId;
    } while ((iVar9 != 0) &&
            (groupId = groupId + ANIM_GROUP_DOOR, AVar12 = uVar2,
            groupId < (int)CAnimManager::ms_numAnimAssocDefinitions));
  }
  this->m_nAnimType = AVar12;
  this->m_nCarsCanDriveMask = local_11c[0];
  this->m_nPedFlags = local_120[0];
  this->m_nRadio2 = (char)local_128 + RADIO_CLASSIC_HIP_HOP;
  this->m_nRadio1 = local_124[0] + RADIO_CLASSIC_HIP_HOP;
  eVar10 = CPopulation::FindPedRaceFromName(acStack_108);
  this->m_nRace = (uint8)eVar10;
  eVar3 = CAEPedSpeechAudioEntity::GetAudioPedType(acStack_f0);
  this->m_nPedAudioType = eVar3;
  sVar4 = CAEPedSpeechAudioEntity::GetVoice(acStack_7c,CONCAT22(extraout_var,eVar3));
  this->m_nVoiceMin = sVar4;
  sVar4 = CAEPedSpeechAudioEntity::GetVoice(acStack_40,CONCAT22(extraout_var,eVar3));
  this->m_nVoiceMax = sVar4;
  this->m_nVoiceId = this->m_nVoiceMin;
  return unaff_EBX;
}
#endif

// @@METHOD@@ CFileLoader::LoadPedPathNode
#if 0 // TODO(port): enable when ported (CGeneral)
// CFileLoader::LoadPedPathNode - converted from src/CFileLoader/LoadPedPathNode_005b41f0.c (decompiled @ 005b41f0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadPedPathNode(const char* line, int32 objModelIndex, int32 pathEntryIndex)
{
  short sVar1;
  uint32 uVar2;
  uint32 uVar3;
  uint32 uVar4;
  uint32 uVar5;
  uint32 uVar6;
  uint32 uVar7;
  uint32 uVar8;
  uint32 uVar9;
  int *piVar10;
  uint8 *puVar11;
  float fVar12;
  float fVar13;
  int iVar14;
  uint32 uVar15;
  int local_34 [3];
  uint32 local_28;
  uint8 local_24 [4];
  uint8 local_20 [4];
  uint8 local_1c [4];
  uint32 local_18;
  uint32 local_14;
  uint32 local_10;
  uint8 local_c [4];
  uint8 local_8 [4];
  uint8 local_4 [4];
  
  puVar11 = local_1c;
  piVar10 = local_34 + 2;
  local_34[1] = 0x3f800000;
  local_34[0] = 0;
  sscanf(line,"%d %d %d %f %f %f %f %d %d %d %d %f %d",&local_14,&local_18,piVar10,puVar11,local_20
          ,local_24,&local_28,local_4,local_8,local_c,&local_10,local_34 + 1,local_34);
  uVar15 = 0;
  if (objModelIndex == -1) {
    uVar2 = local_10 >> 0xb & 0xffffff01;
    iVar14 = local_34[0];
    uVar3 = CGeneral::unk_00821b40(uVar2,local_34[0]);
    uVar7 = local_10 >> 2 & 0xffffff01;
    uVar9 = local_10 & 0xffffff01;
    uVar8 = (uint32)(local_34[2] != 0);
    sVar1 = CGeneral::unk_00821b40(local_28,uVar8,uVar9,uVar7,uVar3);
    fVar13 = (float)(int)sVar1;
    sVar1 = CGeneral::unk_00821b40(piVar10,puVar11,fVar13);
    fVar12 = (float)(int)sVar1;
    sVar1 = CGeneral::unk_00821b40(piVar10,fVar12);
    nullsub_0044d2e0(pathEntryIndex,local_14,local_18,(float)(int)sVar1,fVar12,fVar13,local_28,uVar8
                     ,uVar9,uVar7,uVar3,uVar2,iVar14,uVar15);
    return;
  }
  uVar2 = (uint32)(local_34[0] != 0);
  uVar3 = CGeneral::unk_00821b40(uVar2,0);
  uVar7 = (uint32)(local_34[2] != 0);
  uVar4 = CGeneral::unk_00821b40(local_28,uVar7,uVar3);
  uVar5 = CGeneral::unk_00821b40(uVar4);
  uVar6 = CGeneral::unk_00821b40(uVar5);
  nullsub_0044d2d0(objModelIndex,pathEntryIndex,local_14,local_18,uVar6,uVar5,uVar4,local_28,uVar7,
                   uVar3,uVar2,uVar15);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadPickup
#if 0 // TODO(port): enable when ported (CPickups)
// CFileLoader::LoadPickup - converted from src/CFileLoader/LoadPickup_005b47b0.c (decompiled @ 005b47b0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadPickup(const char* line)
{
  int iVar1;
  uint32 uVar2;
  uint32 local_1c;
  uint32 local_18;
  uint32 local_14;
  uint32 local_10;
  uint32 local_c;
  uint32 local_8;
  uint32 local_4;
  
  iVar1 = sscanf(line,"%d %f %f %f",&local_10,&local_1c,&local_18,&local_14);
  if (iVar1 == 4) {
    local_8 = local_18;
    local_c = local_1c;
    local_4 = local_14;
    switch(local_10) {
    case 4:
      uVar2 = 0x14b;
      break;
    case 5:
      uVar2 = 0x14e;
      break;
    case 6:
      uVar2 = 0x14f;
      break;
    default:
      goto switchD_005b480a_caseD_7;
    case 9:
      uVar2 = 0x14d;
      break;
    case 10:
      uVar2 = 0x150;
      break;
    case 0xb:
      uVar2 = 0x151;
      break;
    case 0xc:
      uVar2 = 0x152;
      break;
    case 0xd:
      uVar2 = 0x153;
      break;
    case 0xe:
      uVar2 = 0x155;
      break;
    case 0xf:
      uVar2 = 0x158;
      break;
    case 0x10:
      uVar2 = 0x156;
      break;
    case 0x11:
      uVar2 = 0x16b;
      break;
    case 0x12:
      uVar2 = 0x15a;
      break;
    case 0x13:
      uVar2 = 0x15b;
      break;
    case 0x14:
      uVar2 = 0x15c;
      break;
    case 0x15:
      uVar2 = 0x15d;
      break;
    case 0x16:
    case 0x2d:
      uVar2 = 0x15f;
      break;
    case 0x17:
      uVar2 = 0x174;
      break;
    case 0x18:
      uVar2 = 0x160;
      break;
    case 0x19:
      uVar2 = 0x161;
      break;
    case 0x1a:
      uVar2 = 0x163;
      break;
    case 0x1b:
      uVar2 = 0x164;
      break;
    case 0x1c:
      uVar2 = 0x165;
      break;
    case 0x1d:
      uVar2 = 0x166;
      break;
    case 0x1f:
      uVar2 = 0x169;
      break;
    case 0x20:
    case 0x2c:
      uVar2 = 0x16a;
      break;
    case 0x21:
      uVar2 = 0x141;
      break;
    case 0x22:
      uVar2 = 0x142;
      break;
    case 0x23:
      uVar2 = 0x143;
      break;
    case 0x24:
      uVar2 = 0x144;
      break;
    case 0x25:
      uVar2 = 0x145;
      break;
    case 0x26:
      uVar2 = 0x146;
      break;
    case 0x27:
      uVar2 = 0x147;
      break;
    case 0x28:
      uVar2 = 0x148;
      break;
    case 0x29:
      uVar2 = 0x14a;
      break;
    case 0x2b:
      uVar2 = 0x157;
      break;
    case 0x2e:
      uVar2 = 0x167;
      break;
    case 0x2f:
      uVar2 = 0x168;
      break;
    case 0x30:
      uVar2 = 0x16c;
      break;
    case 0x31:
      uVar2 = 0x16d;
      break;
    case 0x32:
      uVar2 = 0x16e;
      break;
    case 0x33:
      uVar2 = 0x16f;
      break;
    case 0x34:
      uVar2 = 0x170;
      break;
    case 0x35:
      uVar2 = 0x171;
      break;
    case 0x36:
      uVar2 = 0x172;
      break;
    case 0x37:
      uVar2 = 0x173;
    }
    CPickups::GenerateNewOne(local_1c,local_18,local_14,uVar2,2,0,0,0,0);
  }
switchD_005b480a_caseD_7:
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadScene
#if 0 // TODO(port): enable when ported (CIplStore, gNumLoadedBuildings)
// CFileLoader::LoadScene - converted from src/CFileLoader/LoadScene_005b8700.c (decompiled @ 005b8700)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadScene(const char* filename)
{
  char cVar1;
  FILE *_File;
  char *line;
  CEntity *pCVar2;
  uint32 *puVar3;
  uint32 uVar4;
  int iVar5;
  int *unaff_EDI;
  int iVar6;
  uint32 uVar7;
  int *outSize;
  bool a4;
  int local_4;
  
  iVar5 = 0;
  iVar6 = -1;
  gNumLoadedBuildings = 0;
  outSize = (int *)filename;
  _File = (FILE *)CFileMgr::OpenFile(filename,"r");
  line = LoadLine((char **)_File,outSize);
  while (line != (char *)0x0) {
    cVar1 = *line;
    if ((cVar1 != '\0') && (cVar1 != '#')) {
      if (iVar5 == 0) {
        if ((((cVar1 == 'i') && (line[1] == 'n')) && (line[2] == 's')) && (line[3] == 't')) {
          iVar5 = 2;
        }
        if (((cVar1 == 'm') && (line[1] == 'u')) && ((line[2] == 'l' && (line[3] == 't')))) {
          iVar5 = 3;
        }
        else if (((cVar1 == 'z') && (line[1] == 'o')) && ((line[2] == 'n' && (line[3] == 'e')))) {
          iVar5 = 4;
        }
        else if ((((cVar1 == 'c') && (line[1] == 'u')) && (line[2] == 'l')) && (line[3] == 'l')) {
          iVar5 = 5;
        }
        else if (((cVar1 == 'p') && (line[1] == 'a')) && ((line[2] == 't' && (line[3] == 'h')))) {
          iVar5 = 1;
        }
        else if (((cVar1 == 'o') && (line[1] == 'c')) && ((line[2] == 'c' && (line[3] == 'l')))) {
          iVar5 = 6;
        }
        else if ((((cVar1 == 'g') && (line[1] == 'r')) && (line[2] == 'g')) && (line[3] == 'e')) {
          iVar5 = 8;
        }
        else if (((cVar1 == 'e') && (line[1] == 'n')) && ((line[2] == 'e' && (line[3] == 'x')))) {
          iVar5 = 9;
        }
        else if (((cVar1 == 'p') && (line[1] == 'i')) && ((line[2] == 'c' && (line[3] == 'k')))) {
          iVar5 = 10;
        }
        else if ((((cVar1 == 'c') && (line[1] == 'a')) && (line[2] == 'r')) && (line[3] == 's')) {
          iVar5 = 0xb;
        }
        else if (((cVar1 == 'j') && (line[1] == 'u')) && ((line[2] == 'm' && (line[3] == 'p')))) {
          iVar5 = 0xc;
        }
        else if (((cVar1 == 't') && (line[1] == 'c')) && ((line[2] == 'y' && (line[3] == 'c')))) {
          iVar5 = 0xd;
        }
        else if ((((cVar1 == 'a') && (line[1] == 'u')) && (line[2] == 'z')) && (line[3] == 'o')) {
          iVar5 = 0xe;
        }
      }
      else if (((cVar1 == 'e') && (line[1] == 'n')) && (line[2] == 'd')) {
        iVar5 = 0;
      }
      else {
        switch(iVar5) {
        case 2:
          pCVar2 = LoadObjectInstance(line);
          gpLoadedBuildings[gNumLoadedBuildings] = pCVar2;
          gNumLoadedBuildings = gNumLoadedBuildings + 1;
          break;
        case 4:
          LoadZone(line);
          break;
        case 5:
          LoadCullZone(line);
          break;
        case 6:
          LoadOcclusionVolume(line,filename);
          break;
        case 7:
          if (iVar6 == -1) {
            LoadPathHeader(line,&local_4);
            iVar6 = 0;
          }
          else {
            if (local_4 == 0) {
              LoadPedPathNode(line,-1,iVar6);
            }
            else {
              if (local_4 == 1) {
                a4 = false;
              }
              else {
                if (local_4 != 2) goto LAB_005b89d8;
                a4 = true;
              }
              LoadCarPathNode(line,-1,iVar6,a4);
            }
LAB_005b89d8:
            iVar6 = iVar6 + 1;
            if (iVar6 == 0xc) {
              iVar6 = -1;
            }
          }
          break;
        case 8:
          LoadGarage(line);
          break;
        case 9:
          LoadEntryExit(line);
          break;
        case 10:
          LoadPickup(line);
          break;
        case 0xb:
          LoadCarGenerator(line,0);
          break;
        case 0xc:
          LoadStuntJump(line);
          break;
        case 0xd:
          LoadTimeCyclesModifier(line);
          break;
        case 0xe:
          LoadAudioZone(line);
        }
        if (iVar5 == 1) break;
      }
    }
    line = LoadLine((char **)_File,unaff_EDI);
  }
  CFileMgr::CloseFile(_File);
  iVar5 = -1;
  uVar4 = gNumLoadedBuildings;
  if (0 < (int)gNumLoadedBuildings) {
    iVar5 = CIplStore::GetNewIplEntityIndexArray(gNumLoadedBuildings);
    puVar3 = (uint32 *)CIplStore::GetIplEntityIndexArray(iVar5);
    uVar4 = gNumLoadedBuildings;
    if (0 < (int)gNumLoadedBuildings) {
      iVar6 = (int)gpLoadedBuildings - (int)puVar3;
      uVar7 = gNumLoadedBuildings;
      do {
        *puVar3 = *(uint32 *)(iVar6 + (int)puVar3);
        puVar3 = puVar3 + 1;
        uVar7 = uVar7 - 1;
      } while (uVar7 != 0);
    }
  }
  iVar6 = CIplStore::SetupRelatedIpls(filename,iVar5,gpLoadedBuildings + uVar4);
  LinkLods(iVar6);
  CIplStore::RemoveRelatedIpls(iVar5);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadStuntJump
#if 0 // TODO(port): enable when ported (CStuntJumpManager)
// CFileLoader::LoadStuntJump - converted from src/CFileLoader/LoadStuntJump_005b45d0.c (decompiled @ 005b45d0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadStuntJump(const char* line)
{
  int iVar1;
  uint32 local_7c;
  uint32 local_78;
  uint32 local_74;
  uint32 local_70;
  uint32 local_6c;
  uint32 local_68;
  uint32 local_64;
  uint32 local_60;
  uint32 local_5c;
  uint32 local_58;
  uint32 local_54;
  uint32 local_50;
  uint32 local_4c;
  uint32 local_48;
  uint32 local_44;
  uint32 local_40;
  uint32 local_3c;
  uint32 local_38;
  uint32 local_34;
  uint32 local_30;
  uint32 local_2c;
  uint32 local_28;
  uint32 local_24;
  uint32 local_20;
  uint32 local_1c;
  uint32 local_18;
  uint32 local_14;
  uint32 local_10;
  uint32 local_c;
  uint32 local_8;
  uint32 local_4;
  
  iVar1 = sscanf(line,"%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d",&local_74,&local_6c,
                  &local_64,&local_48,&local_7c,&local_78,&local_68,&local_44,&local_60,&local_40,
                  &local_70,&local_50,&local_58,&local_54,&local_4c,&local_5c);
  if (iVar1 == 0x10) {
    local_3c = local_58;
    local_38 = local_54;
    local_34 = local_4c;
    local_30 = local_68;
    local_2c = local_44;
    local_24 = local_40;
    local_28 = local_60;
    local_20 = local_70;
    local_1c = local_50;
    local_18 = local_74;
    local_14 = local_6c;
    local_c = local_48;
    local_10 = local_64;
    local_8 = local_7c;
    local_4 = local_78;
    CStuntJumpManager::AddOne(&local_18,&local_30,&local_3c,local_5c,local_74,local_6c,local_64);
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadTexDictionary
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::LoadTexDictionary - converted from src/CFileLoader/LoadTexDictionary_005b3860.c (decompiled @ 005b3860)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
RwTexDictionary* CFileLoader::LoadTexDictionary(const char* filename)
{
  int iVar1;
  int iVar2;
  RwTexDictionary *pRVar3;
  
  pRVar3 = (RwTexDictionary *)0x0;
  iVar1 = RwStreamOpen(2,1,filename);
  if (iVar1 != 0) {
    iVar2 = RwStreamFindChunk(iVar1,0x16,0,0);
    if (iVar2 != 0) {
      pRVar3 = (RwTexDictionary *)RwTexDictionaryGtaStreamRead(iVar1);
    }
    RwStreamClose(iVar1,0);
    if (pRVar3 != (RwTexDictionary *)0x0) {
      return pRVar3;
    }
  }
  pRVar3 = (RwTexDictionary *)RwTexDictionaryCreate();
  return pRVar3;
}
#endif

// @@METHOD@@ CFileLoader::LoadTimeCyclesModifier
#if 0 // TODO(port): enable when ported (CBox, CTimeCycle)
// CFileLoader::LoadTimeCyclesModifier - converted from src/CFileLoader/LoadTimeCyclesModifier_005b81d0.c (decompiled @ 005b81d0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadTimeCyclesModifier(const char* line)
{
  int iVar1;
  float local_60;
  float local_5c;
  float local_58;
  uint32 local_54;
  uint32 local_50;
  uint32 local_4c;
  uint32 local_48;
  uint32 local_44;
  uint32 local_40;
  float local_3c;
  int local_38;
  short local_34 [2];
  uint32 local_30;
  uint32 local_2c;
  uint32 local_28;
  uint32 local_24;
  uint32 local_20;
  uint32 local_1c;
  CBox local_18;
  
  local_58 = 100.0;
  local_5c = 1.0;
  local_60 = 1.0;
  iVar1 = sscanf(line,"%f %f %f %f %f %f %d %d %f %f %f %f",&local_48,&local_44,&local_40,&local_54
                  ,&local_50,&local_4c,local_34,&local_38,&local_3c,&local_58,&local_5c,&local_60);
  if (iVar1 < 0xc) {
    local_60 = local_5c;
  }
  local_30 = local_54;
  local_2c = local_50;
  local_28 = local_4c;
  local_24 = local_48;
  local_20 = local_44;
  local_1c = local_40;
  CBox::Set(&local_24,&local_30);
  CTimeCycle::AddOne(&local_18,local_34[0],local_38,local_3c,local_58,local_60);
  return;
}
#endif

// @@METHOD@@ CFileLoader::LoadTimeObject
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CKeyGen, CModelInfo, CTimeInfo)
// CFileLoader::LoadTimeObject - converted from src/CFileLoader/LoadTimeObject_005b3de0.c (decompiled @ 005b3de0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadTimeObject(const char* line)
{
  int iVar1;
  CTimeModelInfo *this;
  CTimeInfo *pCVar2;
  uint32 uVar3;
  int local_50;
  float local_4c;
  int local_48;
  uint8 local_44 [4];
  uint8 local_40 [4];
  uint32 local_3c;
  uint8 local_38 [4];
  uint8 local_34 [4];
  char local_30 [24];
  char local_18 [24];
  
  iVar1 = sscanf(line,"%d %s %s %f %d %d %d",&local_50,local_30,local_18,&local_4c,&local_3c,
                  local_40,local_44);
  if ((iVar1 != 7) || (local_4c < 4.0)) {
    iVar1 = sscanf(line,"%d %s %s %d",&local_50,local_30,local_18,&local_48);
    if (iVar1 != 4) {
      return -1;
    }
    if (local_48 == 1) {
      sscanf(line,"%d %s %s %d %f %d %d %d",&local_50,local_30,local_18,&local_48,&local_4c,
              &local_3c,local_40,local_44);
    }
    else if (local_48 == 2) {
      sscanf(line,"%d %s %s %d %f %f %d %d %d",&local_50,local_30,local_18,&local_48,&local_4c,
              local_38,&local_3c,local_40,local_44);
    }
    else if (local_48 == 3) {
      sscanf(line,"%d %s %s %d %f %f %f %d %d %d",&local_50,local_30,local_18,&local_48,&local_4c,
              local_38,local_34,&local_3c,local_40,local_44);
    }
  }
  this = CModelInfo::AddTimeModel(local_50);
  pCVar2 = (CTimeInfo *)(**(code **)((int)this->vtable + 0x14))();
  this->m_fDrawDistance = local_4c;
  uVar3 = CKeyGen::GetUppercaseKey(local_30);
  this->m_nKey = uVar3;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_18);
  pCVar2->m_nTimeOn = local_40[0];
  pCVar2->m_nTimeOff = local_44[0];
  SetAtomicModelInfoFlags((CAtomicModelInfo *)this,local_3c);
  pCVar2 = CTimeInfo::FindOtherTimeModel(pCVar2,local_30);
  if (pCVar2 != (CTimeInfo *)0x0) {
    pCVar2->m_nOtherTimeModel = (short)local_50;
  }
  return local_50;
}
#endif

// @@METHOD@@ CFileLoader::LoadTXDParent
#if 0 // TODO(port): enable when ported (CTxdStore)
// CFileLoader::LoadTXDParent - converted from src/CFileLoader/LoadTXDParent_005b75e0.c (decompiled @ 005b75e0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadTXDParent(const char* line)
{
  int iVar1;
  int iVar2;
  char local_40 [32];
  char local_20 [32];
  
  sscanf(line,"%s %s",local_40,local_20);
  iVar1 = CTxdStore::FindTxdSlot(local_40);
  if (iVar1 == -1) {
    iVar1 = CTxdStore::AddTxdSlot(local_40);
  }
  iVar2 = CTxdStore::FindTxdSlot(local_20);
  if (iVar2 == -1) {
    iVar2 = CTxdStore::AddTxdSlot(local_20);
  }
  if (*(char *)(iVar1 + *(int *)((int)CTxdStore::ms_pTxdPool + 4)) < '\0') {
    _DAT_00000006 = (short)iVar2;
    return iVar2;
  }
  *(short *)(*(int *)CTxdStore::ms_pTxdPool + iVar1 * 0xc + 6) = (short)iVar2;
  return iVar2;
}
#endif

// @@METHOD@@ CFileLoader::LoadVehicleObject
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CKeyGen, CModelInfo, CTxdStore, gHandlingDataMgr)
// CFileLoader::LoadVehicleObject - converted from src/CFileLoader/LoadVehicleObject_005b6f30.c (decompiled @ 005b6f30)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadVehicleObject(const char* line)
{
  int iVar1;
  CVehicleModelInfo *this;
  uint32 uVar2;
  int iVar3;
  char *pcVar4;
  int unaff_EBP;
  int *piVar5;
  char *pcVar6;
  bool bVar7;
  float local_a8;
  float local_a4;
  char local_a0 [4];
  uint8 local_9c [4];
  int iStack_98;
  int local_94;
  uint8 local_90 [12];
  uint8 uStack_84;
  uint32 local_80;
  tVehicleCompsUnion local_7c;
  uint8 local_78 [4];
  char local_74 [4];
  uint8 local_70 [12];
  char cStack_64;
  char acStack_63 [35];
  uint8 local_40 [16];
  char local_30 [24];
  char local_18 [24];
  
  local_94 = -1;
  local_80 = 0xffffffff;
  iVar1 = CTxdStore::FindTxdSlot("vehicle");
  if (iVar1 == -1) {
    iVar1 = CTxdStore::AddTxdSlot("vehicle");
  }
  sscanf(line,"%d %s %s %s %s %s %s %s %d %d %x %d %f %f %d",&local_94,local_30,local_18,local_9c,
          local_70,acStack_63 + 3,local_40,local_90,&local_7c,local_74,local_78,&local_a8,local_a0,
          &local_a4,&local_80);
  this = CModelInfo::AddVehicleModel(local_94);
  uVar2 = CKeyGen::GetUppercaseKey(local_30);
  this->m_nKey = uVar2;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_18);
  if (*(char *)((int)this->m_nTxdIndex + *(int *)((int)CTxdStore::ms_pTxdPool + 4)) < '\0') {
    iVar3 = 0;
  }
  else {
    iVar3 = *(int *)CTxdStore::ms_pTxdPool + this->m_nTxdIndex * 0xc;
  }
  *(short *)(iVar3 + 6) = (short)iVar1;
  (**(code **)((int)this->vtable + 0x30))(local_40);
  pcVar4 = &cStack_64;
  while (cStack_64 != '\0') {
    pcVar6 = pcVar4 + 1;
    pcVar4 = pcVar4 + 1;
    if (*pcVar6 == '_') {
      *pcVar4 = ' ';
    }
    cStack_64 = *pcVar4;
  }
  strncpy(this->m_szGameName,&cStack_64,8);
  this->m_nFlags = local_78[0];
  this->m_extraComps = local_7c;
  iVar1 = 4;
  bVar7 = true;
  pcVar4 = local_a0;
  pcVar6 = "car";
  do {
    if (iVar1 == 0) break;
    iVar1 = iVar1 + -1;
    bVar7 = *pcVar4 == *pcVar6;
    pcVar4 = pcVar4 + 1;
    pcVar6 = pcVar6 + 1;
  } while (bVar7);
  if (bVar7) {
    this->m_nVehicleType = VEHICLE_TYPE_AUTOMOBILE;
LAB_005b7263:
    this->m_nWheelModelIndex = (short)unaff_EBP;
  }
  else {
    iVar1 = 7;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "mtruck";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_MTRUCK;
      goto LAB_005b7263;
    }
    iVar1 = 5;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "quad";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_QUAD;
      goto LAB_005b7263;
    }
    iVar1 = 5;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "heli";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_HELI;
      goto LAB_005b7263;
    }
    iVar1 = 6;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "plane";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_PLANE;
      goto LAB_005b7263;
    }
    iVar1 = 5;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "boat";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_BOAT;
      goto LAB_005b726d;
    }
    iVar1 = 6;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "train";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_TRAIN;
      goto LAB_005b726d;
    }
    iVar1 = 7;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "f_heli";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_HELI;
      goto LAB_005b726d;
    }
    iVar1 = 8;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "f_plane";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nWheelModelIndex = (short)unaff_EBP;
      this->m_fWheelSizeFront = 1.0;
      this->m_fWheelSizeRear = 1.0;
      this->m_nVehicleType = VEHICLE_TYPE_FPLANE;
      goto LAB_005b726d;
    }
    iVar1 = 5;
    bVar7 = true;
    pcVar4 = local_a0;
    pcVar6 = "bike";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleType = VEHICLE_TYPE_BIKE;
      this->m_fBikeSteerAngle = (float)unaff_EBP;
    }
    else {
      iVar1 = 4;
      bVar7 = true;
      pcVar4 = local_a0;
      pcVar6 = "bmx";
      do {
        if (iVar1 == 0) break;
        iVar1 = iVar1 + -1;
        bVar7 = *pcVar4 == *pcVar6;
        pcVar4 = pcVar4 + 1;
        pcVar6 = pcVar6 + 1;
      } while (bVar7);
      if (!bVar7) {
        iVar1 = 8;
        bVar7 = true;
        pcVar4 = local_a0;
        pcVar6 = "trailer";
        do {
          if (iVar1 == 0) break;
          iVar1 = iVar1 + -1;
          bVar7 = *pcVar4 == *pcVar6;
          pcVar4 = pcVar4 + 1;
          pcVar6 = pcVar6 + 1;
        } while (bVar7);
        if (!bVar7) goto LAB_005b726d;
        this->m_nVehicleType = VEHICLE_TYPE_TRAILER;
        goto LAB_005b7263;
      }
      this->m_nVehicleType = VEHICLE_TYPE_BMX;
      this->m_fBikeSteerAngle = (float)unaff_EBP;
    }
  }
  this->m_fWheelSizeFront = local_a4;
  this->m_fWheelSizeRear = local_a8;
LAB_005b726d:
  iVar1 = cHandlingDataMgr::GetHandlingId(&gHandlingDataMgr,local_74);
  this->m_nHandlingId = (uint16)iVar1;
  iVar1 = 7;
  bVar7 = true;
  this->m_nWheelUpgradeClass = uStack_84;
  piVar5 = &local_94;
  pcVar4 = "normal";
  do {
    if (iVar1 == 0) break;
    iVar1 = iVar1 + -1;
    bVar7 = (char)*piVar5 == *pcVar4;
    piVar5 = (int *)((int)piVar5 + 1);
    pcVar4 = pcVar4 + 1;
  } while (bVar7);
  if (bVar7) {
    this->m_nVehicleClass = VEHICLE_CLASS_NORMAL;
  }
  else {
    iVar1 = 0xb;
    bVar7 = true;
    piVar5 = &local_94;
    pcVar4 = "poorfamily";
    do {
      if (iVar1 == 0) break;
      iVar1 = iVar1 + -1;
      bVar7 = (char)*piVar5 == *pcVar4;
      piVar5 = (int *)((int)piVar5 + 1);
      pcVar4 = pcVar4 + 1;
    } while (bVar7);
    if (bVar7) {
      this->m_nVehicleClass = VEHICLE_CLASS_POORFAMILY;
    }
    else {
      iVar1 = 0xb;
      bVar7 = true;
      piVar5 = &local_94;
      pcVar4 = "richfamily";
      do {
        if (iVar1 == 0) break;
        iVar1 = iVar1 + -1;
        bVar7 = (char)*piVar5 == *pcVar4;
        piVar5 = (int *)((int)piVar5 + 1);
        pcVar4 = pcVar4 + 1;
      } while (bVar7);
      if (bVar7) {
        this->m_nVehicleClass = VEHICLE_CLASS_RICHFAMILY;
      }
      else {
        iVar1 = 10;
        bVar7 = true;
        piVar5 = &local_94;
        pcVar4 = "executive";
        do {
          if (iVar1 == 0) break;
          iVar1 = iVar1 + -1;
          bVar7 = (char)*piVar5 == *pcVar4;
          piVar5 = (int *)((int)piVar5 + 1);
          pcVar4 = pcVar4 + 1;
        } while (bVar7);
        if (bVar7) {
          this->m_nVehicleClass = VEHICLE_CLASS_EXECUTIVE;
        }
        else {
          iVar1 = 7;
          bVar7 = true;
          piVar5 = &local_94;
          pcVar4 = "worker";
          do {
            if (iVar1 == 0) break;
            iVar1 = iVar1 + -1;
            bVar7 = (char)*piVar5 == *pcVar4;
            piVar5 = (int *)((int)piVar5 + 1);
            pcVar4 = pcVar4 + 1;
          } while (bVar7);
          if (bVar7) {
            this->m_nVehicleClass = VEHICLE_CLASS_WORKER;
          }
          else {
            iVar1 = 4;
            bVar7 = true;
            piVar5 = &local_94;
            pcVar4 = "big";
            do {
              if (iVar1 == 0) break;
              iVar1 = iVar1 + -1;
              bVar7 = (char)*piVar5 == *pcVar4;
              piVar5 = (int *)((int)piVar5 + 1);
              pcVar4 = pcVar4 + 1;
            } while (bVar7);
            if (bVar7) {
              this->m_nVehicleClass = VEHICLE_CLASS_BIG;
            }
            else {
              iVar1 = 5;
              bVar7 = true;
              piVar5 = &local_94;
              pcVar4 = "taxi";
              do {
                if (iVar1 == 0) break;
                iVar1 = iVar1 + -1;
                bVar7 = (char)*piVar5 == *pcVar4;
                piVar5 = (int *)((int)piVar5 + 1);
                pcVar4 = pcVar4 + 1;
              } while (bVar7);
              if (bVar7) {
                this->m_nVehicleClass = VEHICLE_CLASS_TAXI;
              }
              else {
                iVar1 = 6;
                bVar7 = true;
                piVar5 = &local_94;
                pcVar4 = "moped";
                do {
                  if (iVar1 == 0) break;
                  iVar1 = iVar1 + -1;
                  bVar7 = (char)*piVar5 == *pcVar4;
                  piVar5 = (int *)((int)piVar5 + 1);
                  pcVar4 = pcVar4 + 1;
                } while (bVar7);
                if (bVar7) {
                  this->m_nVehicleClass = VEHICLE_CLASS_MOPED;
                }
                else {
                  iVar1 = 10;
                  bVar7 = true;
                  piVar5 = &local_94;
                  pcVar4 = "motorbike";
                  do {
                    if (iVar1 == 0) break;
                    iVar1 = iVar1 + -1;
                    bVar7 = (char)*piVar5 == *pcVar4;
                    piVar5 = (int *)((int)piVar5 + 1);
                    pcVar4 = pcVar4 + 1;
                  } while (bVar7);
                  if (bVar7) {
                    this->m_nVehicleClass = VEHICLE_CLASS_MOTORBIKE;
                  }
                  else {
                    iVar1 = 0xc;
                    bVar7 = true;
                    piVar5 = &local_94;
                    pcVar4 = "leisureboat";
                    do {
                      if (iVar1 == 0) break;
                      iVar1 = iVar1 + -1;
                      bVar7 = (char)*piVar5 == *pcVar4;
                      piVar5 = (int *)((int)piVar5 + 1);
                      pcVar4 = pcVar4 + 1;
                    } while (bVar7);
                    if (bVar7) {
                      this->m_nVehicleClass = VEHICLE_CLASS_LEISUREBOAT;
                    }
                    else {
                      iVar1 = 0xb;
                      bVar7 = true;
                      piVar5 = &local_94;
                      pcVar4 = "workerboat";
                      do {
                        if (iVar1 == 0) break;
                        iVar1 = iVar1 + -1;
                        bVar7 = (char)*piVar5 == *pcVar4;
                        piVar5 = (int *)((int)piVar5 + 1);
                        pcVar4 = pcVar4 + 1;
                      } while (bVar7);
                      if (bVar7) {
                        this->m_nVehicleClass = VEHICLE_CLASS_WORKERBOAT;
                      }
                      else {
                        iVar1 = 8;
                        bVar7 = true;
                        piVar5 = &local_94;
                        pcVar4 = "bicycle";
                        do {
                          if (iVar1 == 0) break;
                          iVar1 = iVar1 + -1;
                          bVar7 = (char)*piVar5 == *pcVar4;
                          piVar5 = (int *)((int)piVar5 + 1);
                          pcVar4 = pcVar4 + 1;
                        } while (bVar7);
                        if (bVar7) {
                          this->m_nVehicleClass = VEHICLE_CLASS_BICYCLE;
                        }
                        else {
                          iVar1 = 7;
                          bVar7 = true;
                          piVar5 = &local_94;
                          pcVar4 = "ignore";
                          do {
                            if (iVar1 == 0) break;
                            iVar1 = iVar1 + -1;
                            bVar7 = (char)*piVar5 == *pcVar4;
                            piVar5 = (int *)((int)piVar5 + 1);
                            pcVar4 = pcVar4 + 1;
                          } while (bVar7);
                          if (bVar7) {
                            this->m_nVehicleClass = VEHICLE_CLASS_IGNORE;
                            return iStack_98;
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  this->m_nFrq = (uint16)local_80;
  return iStack_98;
}
#endif

// @@METHOD@@ CFileLoader::LoadWeaponObject
#if 0 // TODO(port): enable when ported (CBaseModelInfo, CKeyGen, CModelInfo, CTempColModels)
// CFileLoader::LoadWeaponObject - converted from src/CFileLoader/LoadWeaponObject_005b3fb0.c (decompiled @ 005b3fb0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
int32 CFileLoader::LoadWeaponObject(const char* line)
{
  CWeaponModelInfo *this;
  uint32 uVar1;
  int unaff_ESI;
  int local_4c;
  float local_48;
  uint8 local_44 [4];
  uint8 local_40 [16];
  char local_30 [24];
  char local_18 [24];
  
  sscanf(line,"%d %s %s %s %d %f",&local_4c,local_30,local_18,local_40,local_44,&local_48);
  this = CModelInfo::AddWeaponModel(local_4c);
  uVar1 = CKeyGen::GetUppercaseKey(local_30);
  this->m_nKey = uVar1;
  this->m_fDrawDistance = local_48;
  CBaseModelInfo::SetTexDictionary((CBaseModelInfo *)this,local_18);
  (**(code **)((int)this->vtable + 0x30))(local_40);
  CBaseModelInfo::SetColModel((CBaseModelInfo *)this,&CTempColModels::ms_colModelWeapon,false);
  return unaff_ESI;
}
#endif

// @@METHOD@@ CFileLoader::LoadZone
#if 0 // TODO(port): enable when ported (CTheZones)
// CFileLoader::LoadZone - converted from src/CFileLoader/LoadZone_005b4ab0.c (decompiled @ 005b4ab0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::LoadZone(const char* line)
{
  CVector pos1;
  CVector pos2;
  int iVar1;
  eLevelName local_44 [4];
  eZoneType local_40 [4];
  char local_3c [12];
  uint32 local_30;
  uint32 local_2c;
  float local_28;
  uint32 local_24;
  uint32 local_20;
  float local_1c;
  char local_18 [24];
  
  iVar1 = sscanf(line,"%s %d %f %f %f %f %f %f %d %s",local_18,local_40,&local_24,&local_20,
                  &local_1c,&local_30,&local_2c,&local_28,local_44,local_3c);
  if (iVar1 == 10) {
    pos1.y = (float)local_20;
    pos1.x = (float)local_24;
    pos1.z = local_1c;
    pos2.y = (float)local_2c;
    pos2.x = (float)local_30;
    pos2.z = local_28;
    CTheZones::CreateZone(local_18,local_40[0],pos1,pos2,local_44[0],local_3c);
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::ReloadPaths
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::ReloadPaths - converted from src/CFileLoader/ReloadPaths_005b6e10.c (decompiled @ 005b6e10)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::ReloadPaths(const char* filename)
{
  char cVar1;
  bool bVar2;
  FILE *_File;
  char *line;
  int pathEntryIndex;
  int *unaff_EDI;
  int objModelIndex;
  bool a4;
  int iStack_5c;
  int iStack_58;
  uint8 auStack_54 [84];
  
  bVar2 = false;
  pathEntryIndex = -1;
  _File = (FILE *)CFileMgr::OpenFile(filename,"rb");
  line = LoadLine((char **)_File,filename);
  objModelIndex = iStack_58;
  do {
    if (line == (char *)0x0) {
      CFileMgr::CloseFile(_File);
      return;
    }
    cVar1 = *line;
    if ((cVar1 != '\0') && (cVar1 != '#')) {
      if (bVar2) {
        if (((cVar1 == 'e') && (line[1] == 'n')) && (line[2] == 'd')) {
          bVar2 = false;
        }
        else if (pathEntryIndex == -1) {
          sscanf(line,"%d %d %s",&iStack_58,&iStack_5c,auStack_54);
          pathEntryIndex = 0;
          objModelIndex = iStack_5c;
        }
        else {
          if (iStack_58 == 0) {
            LoadPedPathNode(line,objModelIndex,pathEntryIndex);
          }
          else {
            if (iStack_58 == 1) {
              a4 = false;
            }
            else {
              if (iStack_58 != 2) goto LAB_005b6f05;
              a4 = true;
            }
            LoadCarPathNode(line,objModelIndex,pathEntryIndex,a4);
          }
LAB_005b6f05:
          pathEntryIndex = pathEntryIndex + 1;
          if (pathEntryIndex == 0xc) {
            pathEntryIndex = -1;
          }
        }
      }
      else if (((cVar1 == 'p') && (line[1] == 'a')) && ((line[2] == 't' && (line[3] == 'h')))) {
        bVar2 = true;
        nullsub_0044d2b0();
      }
    }
    line = LoadLine((char **)_File,unaff_EDI);
  } while( true );
}
#endif

// @@METHOD@@ CFileLoader::SaveTexDictionary
#if 0 // TODO(port): enable when ported (RenderWare layer)
// CFileLoader::SaveTexDictionary - converted from src/CFileLoader/SaveTexDictionary_005b38c0.c (decompiled @ 005b38c0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
void CFileLoader::SaveTexDictionary(RwTexDictionary* dictionary, const char* filename)
{
  int iVar1;
  
  iVar1 = RwStreamOpen(2,2,filename);
  if (iVar1 != 0) {
    RwTexDictionaryStreamWrite(dictionary,iVar1);
    RwStreamClose(iVar1,0);
  }
  return;
}
#endif

// @@METHOD@@ CFileLoader::SetRelatedModelInfoCB
#if 0 // TODO(port): enable when ported (CDamageAtomicModelInfo, CModelInfo, CVisibilityPlugins, gAtomicModelId)
// CFileLoader::SetRelatedModelInfoCB - converted from src/CFileLoader/SetRelatedModelInfoCB_00537150.c (decompiled @ 00537150)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
RpAtomic* CFileLoader::SetRelatedModelInfoCB(RpAtomic* atomic, void* data)
{
  RpAtomic *atomic_00;
  int *piVar1;
  char *name;
  CDamageAtomicModelInfo *this;
  uint32 uVar2;
  char *objName;
  RpAtomic **bIsDamageModel;
  char acStack_18 [24];
  
  piVar1 = (int *)(**(code **)((int)CModelInfo::ms_modelInfoPtrs[gAtomicModelId]->vtable + 4))();
  atomic_00 = atomic;
  bIsDamageModel = &atomic;
  objName = acStack_18;
  name = GetFrameNodeName((atomic->object).object.parent);
  GetNameAndDamage(name,objName,(bool *)bIsDamageModel);
  CVisibilityPlugins::SetAtomicRenderCallback(atomic_00,(void *)0x0);
  if ((char)atomic == '\0') {
    (**(code **)(*piVar1 + 0x3c))(atomic_00);
  }
  else {
    this = (CDamageAtomicModelInfo *)(**(code **)(*piVar1 + 8))();
    CDamageAtomicModelInfo::SetDamagedAtomic(this,atomic_00);
  }
  RpClumpRemoveAtomic(data,atomic_00);
  uVar2 = RwFrameCreate();
  RpAtomicSetFrame(atomic_00,uVar2);
  CVisibilityPlugins::SetModelInfoIndex(atomic_00,gAtomicModelId);
  return atomic_00;
}
#endif

// @@METHOD@@ CFileLoader::StartLoadClumpFile
#if 0 // TODO(port): enable when ported (CModelInfo, CVehicleModelInfo)
// CFileLoader::StartLoadClumpFile - converted from src/CFileLoader/StartLoadClumpFile_005373f0.c (decompiled @ 005373f0)
// name source: public-source | evidence: gta-reversed / plugin-sdk / MTA
bool CFileLoader::StartLoadClumpFile(RwStream* stream, uint32 modelIndex)
{
  char cVar1;
  uint8 uVar2;
  int iVar3;
  
  cVar1 = (**(code **)((int)CModelInfo::ms_modelInfoPtrs[modelIndex]->vtable + 0x10))();
  iVar3 = RwStreamFindChunk(stream,0x10,0,0);
  if (iVar3 != 0) {
    if (cVar1 == '\x06') {
      CVehicleModelInfo::UseCommonVehicleTexDicationary();
    }
    uVar2 = RpClumpGtaStreamRead1(stream);
    if (cVar1 == '\x06') {
      CVehicleModelInfo::StopUsingCommonVehicleTexDicationary();
    }
    return (bool)uVar2;
  }
  return false;
}
#endif
