// FxSystem_c - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/FxSystem/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "FxSystem.h"

FxSystem_c::FxSystem_c() {
    // TODO: decomp src/FxSystem/*.c
}

FxSystem_c::~FxSystem_c() {
    // TODO: decomp src/FxSystem/*.c
}

FxSystem_c* FxSystem_c::Constructor() {
    // TODO: decomp src/FxSystem/*.c
    return nullptr;
}

FxSystem_c* FxSystem_c::Destructor() {
    // TODO: decomp src/FxSystem/*.c
    return nullptr;
}

bool FxSystem_c::Init(FxSystemBP_c* systemBP, const RwMatrix& local, RwMatrix* parent) {
    // TODO: decomp src/FxSystem/*.c
    (void)systemBP; (void)local; (void)parent;
    return false;
}

void FxSystem_c::Exit() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::Play() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::PlayAndKill() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::Kill() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::Pause() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::Stop() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::AttachToBone(CEntity* entity, eBoneTag boneId) {
    // TODO: decomp src/FxSystem/*.c
    (void)entity; (void)boneId;
}

void FxSystem_c::AddParticle(const CVector& pos, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ, float lightMult, float lightMultLimit, bool createLocal) {
    // TODO: decomp src/FxSystem/*.c
    (void)pos; (void)vel; (void)timeSince; (void)fxMults;
    (void)rotZ; (void)lightMult; (void)lightMultLimit; (void)createLocal;
}

void FxSystem_c::AddParticle(const RwMatrix& mat, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ, float lightMult, float lightMultLimit, bool createLocal) {
    // TODO: decomp src/FxSystem/*.c
    (void)mat; (void)vel; (void)timeSince; (void)fxMults;
    (void)rotZ; (void)lightMult; (void)lightMultLimit; (void)createLocal;
}

void FxSystem_c::EnablePrim(int32 primIndex, bool enable) {
    // TODO: decomp src/FxSystem/*.c
    (void)primIndex; (void)enable;
}

void FxSystem_c::SetMatrix(RwMatrix* matrix) {
    // TODO: decomp src/FxSystem/*.c
    (void)matrix;
}

void FxSystem_c::SetOffsetPos(const CVector& pos) {
    // TODO: decomp src/FxSystem/*.c
    (void)pos;
}

void FxSystem_c::AddOffsetPos(const CVector& pos) {
    // TODO: decomp src/FxSystem/*.c
    (void)pos;
}

void FxSystem_c::SetConstTime(bool on, float time) {
    // TODO: decomp src/FxSystem/*.c
    (void)on; (void)time;
}

void FxSystem_c::SetRateMult(float mult) {
    // TODO: decomp src/FxSystem/*.c
    (void)mult;
}

void FxSystem_c::SetTimeMult(float mult) {
    // TODO: decomp src/FxSystem/*.c
    (void)mult;
}

void FxSystem_c::SetVelAdd(const CVector& velocity) {
    // TODO: decomp src/FxSystem/*.c
    (void)velocity;
}

void FxSystem_c::CopyParentMatrix() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::GetCompositeMatrix(RwMatrix* out) const {
    // TODO: decomp src/FxSystem/*.c
    (void)out;
}

eFxSystemPlayStatus FxSystem_c::GetPlayStatus() const {
    // TODO: decomp src/FxSystem/*.c
    return eFxSystemPlayStatus::FX_STOPPED;
}

uint32 FxSystem_c::ForAllParticles(void(*callback)(Particle_c*, int32, FxBox_c**), FxBox_c* data) {
    // TODO: decomp src/FxSystem/*.c
    (void)callback; (void)data;
    return 0;
}

// static
void FxSystem_c::UpdateBoundingBoxCB(Particle_c* particle, int32 arg1, FxBox_c** data) {
    // TODO: decomp src/FxSystem/*.c
    (void)particle; (void)arg1; (void)data;
}

void FxSystem_c::GetBoundingBox(FxBox_c* out) {
    // TODO: decomp src/FxSystem/*.c
    (void)out;
}

bool FxSystem_c::GetBoundingSphereWld(FxSphere_c* out) const {
    // TODO: decomp src/FxSystem/*.c
    (void)out;
    return false;
}

bool FxSystem_c::GetBoundingSphereLcl(FxSphere_c* out) const {
    // TODO: decomp src/FxSystem/*.c
    (void)out;
    return false;
}

void FxSystem_c::SetBoundingSphere(FxSphere_c* sphere) {
    // TODO: decomp src/FxSystem/*.c
    (void)sphere;
}

void FxSystem_c::ResetBoundingSphere() {
    // TODO: decomp src/FxSystem/*.c
}

void FxSystem_c::SetLocalParticles(bool enable) {
    // TODO: decomp src/FxSystem/*.c
    (void)enable;
}

void FxSystem_c::SetZTestEnable(bool enable) {
    // TODO: decomp src/FxSystem/*.c
    (void)enable;
}

void FxSystem_c::SetMustCreatePrts(bool enable) {
    // TODO: decomp src/FxSystem/*.c
    (void)enable;
}

bool FxSystem_c::IsVisible() const {
    // TODO: decomp src/FxSystem/*.c
    return false;
}

void FxSystem_c::DoFxAudio(CVector pos) {
    // TODO: decomp src/FxSystem/*.c - needs the audio subsystem (m_FireAE)
    (void)pos;
}

bool FxSystem_c::Update(RwCamera* camera, float timeDelta) {
    // TODO: decomp src/FxSystem/*.c
    (void)camera; (void)timeDelta;
    return false;
}

FxPrim_c** FxSystem_c::GetPrims(uint32& numPrims) {
    // TODO: decomp src/FxSystem/*.c - C++17 adaptation of std::span<FxPrim_c*> (see header)
    numPrims = 0;
    return nullptr;
}
