// RenderWare.h - RenderWare 3.x declarations for the clean-room C++ port.
//
// This is a PORT layer: it provides the RW types, enums, and function
// prototypes the ported game code needs. It is NOT RenderWare itself.
// All structs are forward-declared unless the ported code accesses members
// (RwMatrix, RwRGBA, RwSphere, RwMatrixWeights, RpHAnimHierarchy).
//
// Do NOT redeclare here: RwV3d (CVector.h), RwSurfaceProperties /
// tCompSearchStructByName / tCompSearchStructById (CClumpModelInfo.h),
// RwObjectNameIdAssocation (CClumpModelInfo.h),
// RwObject / RwMatrix / RpClump / RpAtomic / RwFrame fwd-decls (CBaseModelInfo.h).
#pragma once

#include <cstdint>
#include "CVector.h" // RwV3d

class C2dEffect;

// --- Forward declarations (opaque to the ported code) ---
struct RpAtomic; // defined below (members accessed)
struct RwTexDictionary;
struct RwMatrix; // defined below (members accessed)
struct RpMorphTarget;
struct RpSkin;
struct RpHAnimHierarchy; // defined below (members accessed)
struct RtAnimInterpolator;
struct RpLight;
struct RpWorld;
struct RwCamera;
struct RwRaster;
struct RwStream;

// --- Basic value types ---
struct RwRGBA {
    uint8_t red, green, blue, alpha;
};

struct RwSphere {
    RwV3d center;
    float radius;
};

struct RwMatrix {
    RwV3d    right; uint32_t flags;
    RwV3d    up;    uint32_t pad1;
    RwV3d    at;    uint32_t pad2;
    RwV3d    pos;   uint32_t pad3;
};

struct RwMatrixWeights {
    float w0, w1, w2, w3;
};

// Minimal: only the members the ported code touches.
struct RpHAnimHierarchy {
    int32_t flags;
    int32_t numNodes;
    void* _pad[3];
    RtAnimInterpolator* currentAnim;
};

// --- Defined RW structs (ported code accesses members) ---
// Layouts per RW SDK 3.7 (rpmtrl.h / rwcore.h); only accessed members shown.
// TODO: replace with the real RenderWare conversion, then delete this section.
// TODO(port): RwObject/RwLLLink/RpClump/RwFrame minimal layouts added 2026-10-09
//   for CPed (goggles render reads clump->object.parent and frame matrices).
struct RwObject {
    uint8_t type, subType, flags, privateFlags;
    void* parent;
};

// RpAtomic minimally defined (added 2026-10-09 for CPed: `object`/`geometry` accessed).
// TODO(port): real RenderWare RpAtomic layout.
struct RpAtomic {
    RwObject object;             // TODO(port): minimal; decomp sets object.flags
    struct RpGeometry* geometry; // TODO(port): minimal
};
struct RwLLLink {
    void* next;
    void* prev;
};
struct RpClump {
    RwObject object;
    // TODO(port): rest of RpClump (atomicList, lightList, cameraList).
};
struct RwFrame {
    RwObject object;            // 0x00
    RwLLLink inDirtyListLink;   // 0x08
    RwMatrix modelling;         // 0x10
    RwMatrix ltm;               // 0x50
};
struct RwTexture {
    char name[32]; // rwTEXTURENAMESIZE
    char mask[32];
};

struct RpMaterial {
    RwTexture* texture;
    RwRGBA     color;
};

struct RpMaterialList {
    RpMaterial** materials;
    int32_t      numMaterials;
    int32_t      space;
};

struct RpMeshHeader {
    uint32_t flags;
    uint16_t numMeshes;
    uint16_t serialNum;
    uint32_t totalIndicesInMesh;
    uint32_t firstMeshOffset;
};

struct RpMesh {
    RpMaterial* material;
    void*       indices;
    int32_t     numIndices;
};

struct RpGeometry {
    RpMeshHeader* mesh;
    // (remaining RW members omitted; add when ported code needs them)
};

// Original: typedef RwTexture* (*RwTextureCallBackFind)(const RwChar* name);
using RwTextureCallBackFind = RwTexture* (*)(const char* name);

// --- Enums ---
enum RwOpCombineType {
    rwCOMBINEREPLACE   = 0,
    rwCOMBINEPRECONCAT = 1,
    rwCOMBINEPOSTCONCAT = 2,
};

enum RwTextureFilterMode {
    rwFILTERNAFILTERMODE = 0,
    rwFILTERNEAREST      = 1,
    rwFILTERLINEAR       = 2,
    rwFILTERMIPNEAREST   = 3,
    rwFILTERMIPLINEAR    = 4,
    rwFILTERLINEARMIPNEAREST = 5,
    rwFILTERLINEARMIPLINEAR  = 6,
};

// Matrix flag bits (used unscoped: rwMATRIXINTERNALIDENTITY).
static constexpr uint32_t rwMATRIXINTERNALIDENTITY = 0x00020000;
static constexpr uint32_t rwMATRIXTYPEORTHONORMAL  = 0x00000004;

enum class RpAtomicFlag : uint32_t {
    rpATOMICRENDER          = 0x04,
    rpATOMICCOLLISIONTEST   = 0x08,
    rpATOMICRENDERREFLECTION = 0x10,
};

enum class RpMatFXMaterialFlags : int32_t {
    rpMATFXEFFECTNULL      = 0,
    rpMATFXEFFECTBUMPMAP   = 1,
    rpMATFXEFFECTENVMAP    = 2,
    rpMATFXEFFECTBUMPENVMAP = 3,
    rpMATFXEFFECTDUAL      = 4,
};

// --- Callback typedefs ---
using RwFrameCallBack   = RwFrame*   (*)(RwFrame* frame, void* data);
using RpAtomicCallBack  = RpAtomic*  (*)(RpAtomic* atomic, void* data);
using RwObjectCallBack  = RwObject*  (*)(RwObject* object, void* data);
using RpMaterialCallBack = RpMaterial* (*)(RpMaterial* material, void* data);
using RpClumpCallBack   = RpClump*   (*)(RpClump* clump, void* data);

// --- Atomic ---
RpAtomic* RpAtomicClone(RpAtomic* atomic);
void      RpAtomicDestroy(RpAtomic* atomic);
RwFrame*  RpAtomicGetFrame(RpAtomic* atomic);
RpGeometry* RpAtomicGetGeometry(RpAtomic* atomic);
void*     RpAtomicGetRenderCallBack(RpAtomic* atomic);
void      RpAtomicSetFlags(RpAtomic* atomic, uint32_t flags);
void      RpAtomicSetFlags(RpAtomic* atomic, RpAtomicFlag flags);
// RpAtomicGetFlags (added 2026-10-09 for CAutomobile). TODO(port): real RenderWare.
inline uint32_t RpAtomicGetFlags(const void* atomic) { (void)atomic; return 0; }
// rpATOMICRENDER flag (added 2026-10-09 for CAutomobile). TODO(port): real RenderWare value.
constexpr uint32_t rpATOMICRENDER = 0x4;
void      RpAtomicSetFrame(RpAtomic* atomic, RwFrame* frame);
void      RpAtomicSetRenderCallBack(RpAtomic* atomic, void* renderFunc);

// --- Clump ---
RpClump*  RpClumpClone(RpClump* clump);
void      RpClumpDestroy(RpClump* clump);
RwFrame*  RpClumpGetFrame(RpClump* clump);
void      RpClumpAddAtomic(RpClump* clump, RpAtomic* atomic);
void      RpClumpRemoveAtomic(RpClump* clump, RpAtomic* atomic);
RpClump*  RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* data);

// --- Geometry / material ---
void      RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack callback, void* data);
RpMaterial* RpGeometryGetMaterial(RpGeometry* geometry, int32_t index);
RpMesh*   RpGeometryGetMesh(RpGeometry* geometry, int32_t meshIndex);
RpMorphTarget* RpGeometryGetMorphTarget(RpGeometry* geometry, int32_t index);
int32_t   RpGeometryGetNumVertices(RpGeometry* geometry);
// 2dfx plugin (game data, not real RW):
int32_t   RpGeometryGet2dFxCount(RpGeometry* geometry);
C2dEffect* RpGeometryGet2dFxAtIndex(RpGeometry* geometry, int32_t index);
RwRGBA*   RpMaterialGetColor(RpMaterial* material);
struct RwSurfaceProperties; // defined in CVehicleModelInfo.h
RwSurfaceProperties* RpMaterialGetSurfaceProperties(RpMaterial* material);
RwTexture* RpMaterialGetTexture(RpMaterial* material);
void      RpMaterialSetSurfaceProperties(RpMaterial* material, const RwSurfaceProperties* props);
void      RpMaterialSetTexture(RpMaterial* material, RwTexture* texture);
RwSphere* RpMorphTargetGetBoundingSphere(RpMorphTarget* morphTarget);

// --- Skin plugin ---
void      RpSkinAtomicSetHAnimHierarchy(RpAtomic* atomic, RpHAnimHierarchy* hierarchy);
RpSkin*   RpSkinGeometryGetSkin(RpGeometry* geometry);
RwMatrixWeights* RpSkinGetVertexBoneWeights(RpSkin* skin);

// --- HAnim plugin ---
RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy);
int32_t   RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, int32_t id);
void      RpAnimBlendClumpInit(RpClump* clump);
void*     RpAnimBlendCreateAnimationForHierarchy(RpHAnimHierarchy* hierarchy, int32_t animId);

// --- Clump ---
// TODO(port): real RenderWare implementation (rwcore.h).
RpClump*  RpClumpRender(RpClump* clump);
// gta-reversed Plugins/RpAnimBlendPlugin/RpAnimBlend.h:139
// CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, uint32 animId);
// TODO(port): needs CAnimBlendAssociation type; using void* for now
// Added 2026-10-09 for CPed.
class CAnimBlendAssociation;
CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, uint32_t animId);
// gta-reversed Plugins/RpAnimBlendPlugin/RpAnimBlend.h
// TODO(port): needs CAnimBlendAssociation type; using void* for now
// Added 2026-10-09 for CPed.
CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociation(RpClump* clump, uint32_t flags = 0); // TODO(port): default arg added 2026-10-09; decomp calls with 1 arg
CAnimBlendAssociation* RpAnimBlendGetNextAssociation(CAnimBlendAssociation* assoc, uint32_t flags = 0); // TODO(port): default arg added 2026-10-09; decomp calls with 1 arg

// --- MatFX plugin ---
RpMatFXMaterialFlags RpMatFXMaterialGetEffects(RpMaterial* material);
void      RpMatFXMaterialSetEnvMapCoefficient(RpMaterial* material, float coeff);
void      RpMatFXMaterialSetEnvMapFrame(RpMaterial* material, RwFrame* frame);

// --- Anim interpolator ---
void RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* interp, void* anim);

// --- Frame ---
RwFrame*  RwFrameCreate();
void      RwFrameDestroy(RwFrame* frame);
void      RwFrameAddChild(RwFrame* parent, RwFrame* child);
void      RwFrameRemoveChild(RwFrame* child);
RwFrame*  RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callback, void* data);
RwObject* RwFrameForAllObjects(RwFrame* frame, RwObjectCallBack callback, void* data);
RwMatrix* RwFrameGetLTM(RwFrame* frame);
RwMatrix* RwFrameGetMatrix(RwFrame* frame);
RwFrame*  RwFrameGetParent(RwFrame* frame);
void      RwFrameTransform(RwFrame* frame, const RwMatrix* matrix, RwOpCombineType combineOp);
void      RwFrameUpdateObjects(RwFrame* frame);

// --- Matrix ---
RwMatrix* RwMatrixCreate();
void      RwMatrixDestroy(RwMatrix* matrix);
RwV3d*    RwMatrixGetAt(RwMatrix* matrix);
RwV3d*    RwMatrixGetPos(RwMatrix* matrix);
RwV3d*    RwMatrixGetRight(RwMatrix* matrix);
RwV3d*    RwMatrixGetUp(RwMatrix* matrix);
RwMatrix* RwMatrixInvert(RwMatrix* dst, const RwMatrix* src);
RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, float angle, RwOpCombineType combineOp);
RwV3d*    RwV3dTransformPoints(RwV3d* out, const RwV3d* in, int32_t count, const RwMatrix* matrix);
RwMatrix* RwMatrixTransform(RwMatrix* dst, const RwMatrix* src, RwOpCombineType combineOp);

// --- Texture / tex dictionary ---
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const char* name);
RwTexDictionary* RwTexDictionaryGetCurrent();
void      RwTextureDestroy(RwTexture* texture);
char*       RwTextureGetName(RwTexture* texture); // RW SDK returns non-const RwChar*
RwTexture* RwTextureRead(const char* name, const char* maskName);
void      RwTextureSetFilterMode(RwTexture* texture, RwTextureFilterMode mode);
RwTextureCallBackFind RwTextureGetFindCallBack();
void      RwTextureSetFindCallBack(RwTextureCallBackFind callback);

// --- Object flags ---
uint32_t rwObjectTestFlags(const RwObject* object, RpAtomicFlag flag);
uint32_t rwObjectTestFlags(const RpAtomic* atomic, RpAtomicFlag flag); // RpAtomic* convenience overload

// --- RW material-list internals (rpmtrl.h) ---
void _rpMaterialListDeinitialize(RpMaterialList* matList);
void _rpMaterialListAppendMaterial(RpMaterialList* matList, RpMaterial* material);

// --- Game-specific frame/atomics helpers (implemented in the game, not RW) ---
// These walk frame hierarchies using the game plugins (frame extension data).
RpAtomic* GetFirstAtomic(RpClump* clump);
// AtomicRemoveAnimFromSkinCB (added 2026-10-09 for CPed). TODO(port): real RenderWare/skin callback.
// Inline stub so users link; the RenderWare batch replaces it.
inline RpAtomic* AtomicRemoveAnimFromSkinCB(RpAtomic* atomic, void* data) { (void)data; return atomic; }
RwObject* GetFirstObject(RwFrame* frame);
RwFrame*  GetFirstChild(RwFrame* frame);
RpHAnimHierarchy* GetAnimHierarchyFromClump(RpClump* clump);
RpHAnimHierarchy* GetAnimHierarchyFromSkinClump(RpClump* clump);
RpHAnimHierarchy* GetAnimHierarchyFromFrame(RwFrame* frame);
const char* GetFrameNodeName(RwFrame* frame);
RpAtomic* Get2DEffectAtomic(RpAtomic* atomic, void* data);

// --- MatFX stubs (added 2026-10-09 for CAutomobile) ---
// TODO(port): real RenderWare MatFX implementation.
inline void RpMatFXMaterialSetEffects(RpMaterial* material, RpMatFXMaterialFlags flags) { (void)material; (void)flags; }
// Unscoped alias for the port's old-style usage (gta-reversed uses rpMATFXEFFECTNULL).
constexpr auto rpMATFXEFFECTNULL = RpMatFXMaterialFlags::rpMATFXEFFECTNULL;

// --- Texture stubs (added 2026-10-09 for CAutomobile) ---
// TODO(port): real RenderWare texture implementation.
inline void RwTextureAddRef(RwTexture* texture) { (void)texture; }
