// CAERadioTrackManager.cpp - GTA SA 1.0 clean-room C++ conversion
// Method stubs. Decompiled reference: src/CAERadioTrackManager/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CAERadioTrackManager.h"

// Static member definitions (were `static inline auto& x = StaticRef<T>(0xADDR)`
// in gta-reversed). Original GTA SA 1.0 addresses kept as comments.
// TODO: re-resolve these for the clean-room build.
CAERadioTrackManager::DJBanterIndexHistory CAERadioTrackManager::m_nDJBanterIndexHistory[RADIO_COUNT]{};   // 0xB61D78
CAERadioTrackManager::AdvertIndexHistory   CAERadioTrackManager::m_nAdvertIndexHistory[RADIO_COUNT]{};     // 0xB620C0
CAERadioTrackManager::IdentIndexHistory    CAERadioTrackManager::m_nIdentIndexHistory[RADIO_COUNT]{};      // 0xB62980
CAERadioTrackManager::MusicTrackHistory    CAERadioTrackManager::m_nMusicTrackIndexHistory[RADIO_COUNT]{}; // 0xB62B40

uint8_t CAERadioTrackManager::m_nStatsLastHitTimeOutHours{};   // 0xB62C58
uint8_t CAERadioTrackManager::m_nStatsLastHitGameClockHours{}; // 0xB62C59
uint8_t CAERadioTrackManager::m_nStatsLastHitGameClockDays{};  // 0xB62C5A
uint8_t CAERadioTrackManager::m_nStatsStartedCrash1{};         // 0xB62C5B
uint8_t CAERadioTrackManager::m_nStatsStartedCat2{};           // 0xB62C5C
uint8_t CAERadioTrackManager::m_nStatsStartedBadlands{};       // 0xB62C5D
uint8_t CAERadioTrackManager::m_nStatsPassedVCrash2{};         // 0xB62C5E
uint8_t CAERadioTrackManager::m_nStatsPassedTruth2{};          // 0xB62C5F
uint8_t CAERadioTrackManager::m_nStatsPassedSweet2{};          // 0xB62C60
uint8_t CAERadioTrackManager::m_nStatsPassedStrap4{};          // 0xB62C61
uint8_t CAERadioTrackManager::m_nStatsPassedSCrash1{};         // 0xB62C62
uint8_t CAERadioTrackManager::m_nStatsPassedRiot1{};           // 0xB62C63
uint8_t CAERadioTrackManager::m_nStatsPassedRyder2{};          // 0xB62C64
uint8_t CAERadioTrackManager::m_nStatsPassedMansion2{};        // 0xB62C65
uint8_t CAERadioTrackManager::m_nStatsPassedLAFin2{};          // 0xB62C66
uint8_t CAERadioTrackManager::m_nStatsPassedFarlie3{};         // 0xB62C67
uint8_t CAERadioTrackManager::m_nStatsPassedDesert10{};        // 0xB62C68
uint8_t CAERadioTrackManager::m_nStatsPassedDesert8{};         // 0xB62C69
uint8_t CAERadioTrackManager::m_nStatsPassedDesert5{};         // 0xB62C6A
uint8_t CAERadioTrackManager::m_nStatsPassedDesert3{};         // 0xB62C6B
uint8_t CAERadioTrackManager::m_nStatsPassedDesert1{};         // 0xB62C6C
uint8_t CAERadioTrackManager::m_nStatsPassedCat1{};            // 0xB62C6D
uint8_t CAERadioTrackManager::m_nStatsPassedCasino10{};        // 0xB62C6E
uint8_t CAERadioTrackManager::m_nStatsPassedCasino6{};         // 0xB62C6F
uint8_t CAERadioTrackManager::m_nStatsPassedCasino3{};         // 0xB62C70
uint8_t CAERadioTrackManager::m_nStatsCitiesPassed{};          // 0xB62C71
uint8_t CAERadioTrackManager::m_nSpecialDJBanterIndex{};       // 0xB62C72
uint8_t CAERadioTrackManager::m_nSpecialDJBanterPending{};     // 0xB62C73

// Global instance.
// Original GTA SA 1.0 address (from gta-reversed StaticRef) kept as comment.
// TODO: re-resolve for the clean-room build.
CAERadioTrackManager AERadioTrackManager; // 0x8CB6F8

CAERadioTrackManager::CAERadioTrackManager(int32_t hwClientHandle) :
    m_HwClientHandle(hwClientHandle)
{
    // TODO: src/CAERadioTrackManager/CAERadioTrackManager_*.c - verify body
}

bool CAERadioTrackManager::Initialise(int32_t channelId) {
    // TODO: src/CAERadioTrackManager/Initialise_*.c
    return false;
}

void CAERadioTrackManager::InitialiseRadioStationID(eRadioID id) {
    // TODO: src/CAERadioTrackManager/InitialiseRadioStationID_*.c
}

void CAERadioTrackManager::Reset() {
    // TODO: src/CAERadioTrackManager/Reset_*.c
}

void CAERadioTrackManager::ResetStatistics() {
    // TODO: src/CAERadioTrackManager/ResetStatistics_*.c
}

bool CAERadioTrackManager::IsRadioOn() const {
    // TODO: src/CAERadioTrackManager/IsRadioOn_*.c
    return false;
}

bool CAERadioTrackManager::HasRadioRetuneJustStarted() const {
    // TODO: src/CAERadioTrackManager/HasRadioRetuneJustStarted_*.c
    return false;
}

eRadioID CAERadioTrackManager::GetCurrentRadioStationID() const {
    // TODO: src/CAERadioTrackManager/GetCurrentRadioStationID_*.c
    return RADIO_OFF;
}

int32_t* CAERadioTrackManager::GetRadioStationListenTimes() {
    // TODO: src/CAERadioTrackManager/GetRadioStationListenTimes_*.c
    return nullptr;
}

void CAERadioTrackManager::SetRadioAutoRetuneOnOff(bool enable) {
    // TODO: src/CAERadioTrackManager/SetRadioAutoRetuneOnOff_*.c
}

void CAERadioTrackManager::SetBassEnhanceOnOff(bool enable) {
    // TODO: src/CAERadioTrackManager/SetBassEnhanceOnOff_*.c
}

void CAERadioTrackManager::SetBassSetting(eBassSetting bassSetting, float bassGrain) {
    // TODO: src/CAERadioTrackManager/SetBassSetting_*.c
}

void CAERadioTrackManager::RetuneRadio(eRadioID radioId) {
    // TODO: src/CAERadioTrackManager/RetuneRadio_*.c
}

void CAERadioTrackManager::DisplayRadioStationName() {
    // TODO: src/CAERadioTrackManager/DisplayRadioStationName_*.c
}

const GxtChar* CAERadioTrackManager::GetRadioStationName(eRadioID id) {
    // TODO: src/CAERadioTrackManager/GetRadioStationName_*.c
    return nullptr;
}

void CAERadioTrackManager::GetRadioStationNameKey(eRadioID id, char* outStr) {
    // TODO: src/CAERadioTrackManager/GetRadioStationNameKey_*.c
}

bool CAERadioTrackManager::IsVehicleRadioActive() {
    // TODO: src/CAERadioTrackManager/IsVehicleRadioActive_*.c
    return false;
}

void CAERadioTrackManager::StartTrackPlayback() {
    // TODO: src/CAERadioTrackManager/StartTrackPlayback_*.c
}

void CAERadioTrackManager::UpdateRadioVolumes() {
    // TODO: src/CAERadioTrackManager/UpdateRadioVolumes_*.c
}

void CAERadioTrackManager::PlayRadioAnnouncement(uint32_t) {
    // TODO: src/CAERadioTrackManager/PlayRadioAnnouncement_*.c
}

void CAERadioTrackManager::StartRadio(eRadioID id, eBassSetting bassSetting, float bassGain, bool skipTrack) {
    // TODO: src/CAERadioTrackManager/StartRadio_*.c
}

void CAERadioTrackManager::StartRadio(const tVehicleAudioSettings& settings) {
    // TODO: src/CAERadioTrackManager/StartRadio_*.c
}

void CAERadioTrackManager::StopRadio(tVehicleAudioSettings* settings, bool bDuringPause) {
    // TODO: src/CAERadioTrackManager/StopRadio_*.c
}

void CAERadioTrackManager::Service(int32_t playTime) {
    // TODO: src/CAERadioTrackManager/Service_*.c
}

void CAERadioTrackManager::Load() {
    // TODO: src/CAERadioTrackManager/Load_*.c
}

void CAERadioTrackManager::Save() {
    // TODO: src/CAERadioTrackManager/Save_*.c
}

void CAERadioTrackManager::AddMusicTrackIndexToHistory(eRadioID id, int8_t trackIndex) {
    // TODO: src/CAERadioTrackManager/AddMusicTrackIndexToHistory_*.c
}

void CAERadioTrackManager::AddIdentIndexToHistory(eRadioID id, int8_t trackIndex) {
    // TODO: src/CAERadioTrackManager/AddIdentIndexToHistory_*.c
}

void CAERadioTrackManager::AddAdvertIndexToHistory(eRadioID id, int8_t trackIndex) {
    // TODO: src/CAERadioTrackManager/AddAdvertIndexToHistory_*.c
}

void CAERadioTrackManager::AddDJBanterIndexToHistory(eRadioID id, int8_t trackIndex) {
    // TODO: src/CAERadioTrackManager/AddDJBanterIndexToHistory_*.c
}

void CAERadioTrackManager::ChooseTracksForStation(eRadioID id) {
    // TODO: src/CAERadioTrackManager/ChooseTracksForStation_*.c
}

int32_t CAERadioTrackManager::ChooseIdentIndex(eRadioID id) {
    // TODO: src/CAERadioTrackManager/ChooseIdentIndex_*.c
    return -1;
}

int32_t CAERadioTrackManager::ChooseAdvertIndex(eRadioID id) {
    // TODO: src/CAERadioTrackManager/ChooseAdvertIndex_*.c
    return -1;
}

int32_t CAERadioTrackManager::ChooseDJBanterIndex(eRadioID id) {
    // TODO: src/CAERadioTrackManager/ChooseDJBanterIndex_*.c
    return -1;
}

int32_t CAERadioTrackManager::ChooseDJBanterIndexFromList(eRadioID id, int32_t** list) {
    // TODO: src/CAERadioTrackManager/ChooseDJBanterIndexFromList_*.c
    return -1;
}

int8_t CAERadioTrackManager::ChooseMusicTrackIndex(eRadioID id) {
    // TODO: src/CAERadioTrackManager/ChooseMusicTrackIndex_*.c
    return -1;
}

int8_t CAERadioTrackManager::ChooseTalkRadioShow() {
    // TODO: src/CAERadioTrackManager/ChooseTalkRadioShow_*.c
    return -1;
}

void CAERadioTrackManager::CheckForTrackConcatenation() {
    // TODO: src/CAERadioTrackManager/CheckForTrackConcatenation_*.c
}

void CAERadioTrackManager::CheckForMissionStatsChanges() {
    // TODO: src/CAERadioTrackManager/CheckForMissionStatsChanges_*.c
}

void CAERadioTrackManager::CheckForStationRetune() {
    // TODO: src/CAERadioTrackManager/CheckForStationRetune_*.c
}

void CAERadioTrackManager::CheckForStationRetuneDuringPause() {
    // TODO: src/CAERadioTrackManager/CheckForStationRetuneDuringPause_*.c
}

void CAERadioTrackManager::CheckForPause() {
    // TODO: src/CAERadioTrackManager/CheckForPause_*.c
}

bool CAERadioTrackManager::QueueUpTracksForStation(eRadioID id, int8_t* iTrackCount, int8_t radioState, tRadioSettings& settings) {
    // TODO: src/CAERadioTrackManager/QueueUpTracksForStation_*.c
    return false;
}

bool CAERadioTrackManager::TrackRadioStation(eRadioID id, bool skipTrack) {
    // TODO: src/CAERadioTrackManager/TrackRadioStation_*.c
    return false;
}
