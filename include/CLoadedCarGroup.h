// CLoadedCarGroup - adapted from gta-reversed for clean-room C++ build
// See src/CLoadedCarGroup/*.c for decompiled bodies.
// TODO: verify each method against decomp.

#pragma once
#include <cstdint>
// Base.h (plugin-sdk) replacements - gta-reversed integer typedefs
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
#include <array>

// eModelID: canonical minimal enum in eModelID.h (deduped 2026-10-09).
#include "eModelID.h"
class CLoadedCarGroup {
public:

    CLoadedCarGroup() { Clear(); }

    void SortBasedOnUsage();

    /*!
    * Pick a random car model ID from the models in this group.
    * This is a weighted random pick, that is, the higher the `CVehicleModelInfo::m_nFrq` of a model is
    * the higher chance it has to get picked.
    *
    * @param bNotTooManyInTheWorld Only return models whose refcnt is <= 2
    * @param bOnlyPickNormalCars   Return only actual vehicles (No boats, etc)
    */
    eModelID PickRandomCar(bool bNotTooManyInTheWorld, bool bOnlyPickNormalCars);

    /*!
    * Pick a model with the least amount of refs and uses
    *
    * @param maxTimesUsed The maximum times the returned model is used (If it was used more than this `MODEL_INVALID` is returned)
    */
    eModelID PickLeastUsedModel(int32 maxTimesUsed);

    //! Get the `idx`-th model
    eModelID GetMember(uint32 count) const;

    //! Get number of models (Please don't us this in a for loop's condition, it's O(N) - Use `GetAllModels` for iteration)
    uint32 CountMembers() const;

    //! Check if there are models at all
    bool Empty() const;

    //! Remove all models
    void Clear();

    //! Remove a model - Does nothing if model not in the group
    void RemoveMember(eModelID modelIndex);

    //! Add a model - Does nothing if already in the group
    void  AddMember(eModelID member);

    //! Get all models from this group
    // TODO: GetAllModels() used rng::views::take (range-v3, plugin-sdk) - reimplement without rng
    // auto GetAllModels() const { return m_models | rng::views::take(CountMembers()); }
    // auto GetAllModels()       { return m_models | rng::views::take(CountMembers()); } // Same, but constless
private:
    std::array<int16, 23> m_models{}; //< Model IDs. Empty slots are marked by `SENTINEL_VALUE_OF_UNUSED` (See cpp file) (NOTE: Use `GetAllModels()` when iterating, etc!)
};
// TODO: static_assert(sizeof(CLoadedCarGroup) == 0x2E) - verify layout
