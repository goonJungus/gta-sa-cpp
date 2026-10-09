// CPool.h - GTA SA 1.0 clean-room C++ conversion
// Adapted from gta-reversed (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Simplified standalone version - removes plugin-sdk dependencies
// (Base.h, reversiblebugfixes, rng::, VALIDATE_SIZE).

#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>

#define INVALID_POOL_SLOT (-1)

/*
    R* terminology      Our terminology
    JustIndex           Index
    Index               Id
    Ref                 Handle/Ref
    Size                Capacity
*/

template<class T, class S = T>
class CPool {
    using StorageType = unsigned char[sizeof(S)];

private:
    struct SlotState {
        uint8_t data{ 0 }; // bit 7: IsEmpty, bits 0-6: Ref count

        bool IsEmpty() const { return (data & 0x80) != 0; }
        void SetEmpty(bool empty) {
            if (empty) data |= 0x80;
            else data &= ~0x80;
        }
        uint8_t GetRef() const { return data & 0x7F; }
        void SetRef(uint8_t ref) { data = (data & 0x80) | (ref & 0x7F); }
        void IncRef() { SetRef(GetRef() + 1); }
    };

public:
    CPool() = default;

    // Initializes pool, owning memory
    CPool(size_t capacity, const char* name = "Unspecified") :
        m_Storage{ new StorageType[capacity] },
        m_SlotState{ new SlotState[capacity] },
        m_Capacity{ static_cast<int32_t>(capacity) },
        m_OwnsAllocations{ true },
        m_Name{ name }
    {
        assert(m_Storage);
        assert(m_SlotState);
        for (size_t i = 0; i < capacity; i++) {
            m_SlotState[i].SetEmpty(true);
            m_SlotState[i].SetRef(0);
        }
    }

    // Initialises a pool with pre-allocated memory (non-owning)
    CPool(size_t capacity, void* storage, void* states) :
        m_Storage{ static_cast<StorageType*>(storage) },
        m_SlotState{ static_cast<SlotState*>(states) },
        m_Capacity{ static_cast<int32_t>(capacity) },
        m_OwnsAllocations{ false }
    {
    }

    ~CPool() {
        if (m_OwnsAllocations) {
            delete[] m_Storage;
            delete[] m_SlotState;
        }
    }

    // No copy
    CPool(const CPool&) = delete;
    CPool& operator=(const CPool&) = delete;

    int32_t GetCapacity() const { return m_Capacity; }
    int32_t GetNoOfUsedSpaces() const { return m_UsedCount; }
    int32_t GetNoOfFreeSpaces() const { return m_Capacity - m_UsedCount; }
    bool IsFull() const { return m_UsedCount >= m_Capacity; }
    bool IsEmpty() const { return m_UsedCount == 0; }
    const char* GetName() const { return m_Name; }

    // Allocate a new object in the pool, returns nullptr if full
    T* New() {
        for (int32_t i = 0; i < m_Capacity; i++) {
            if (m_SlotState[i].IsEmpty()) {
                m_SlotState[i].SetEmpty(false);
                m_SlotState[i].IncRef();
                m_UsedCount++;
                T* obj = reinterpret_cast<T*>(&m_Storage[i]);
                ::new (obj) T(); // placement new (default construct); ::new bypasses any class-specific operator new
                return obj;
            }
        }
        return nullptr;
    }

    // Allocate at a specific index
    T* New(int32_t index) {
        if (index < 0 || index >= m_Capacity) return nullptr;
        if (!m_SlotState[index].IsEmpty()) return nullptr;
        m_SlotState[index].SetEmpty(false);
        m_SlotState[index].IncRef();
        m_UsedCount++;
        T* obj = reinterpret_cast<T*>(&m_Storage[index]);
        ::new (obj) T(); // ::new: bypass class-specific operator new (see above)
        return obj;
    }

    // Delete an object
    void Delete(T* obj) {
        int32_t index = GetIndex(obj);
        if (index < 0) return;
        obj->~T();
        m_SlotState[index].SetEmpty(true);
        m_UsedCount--;
    }

    // Get object at index (no validation)
    T* GetAt(int32_t index) {
        if (index < 0 || index >= m_Capacity) return nullptr;
        if (m_SlotState[index].IsEmpty()) return nullptr;
        return reinterpret_cast<T*>(&m_Storage[index]);
    }
    const T* GetAt(int32_t index) const {
        if (index < 0 || index >= m_Capacity) return nullptr;
        if (m_SlotState[index].IsEmpty()) return nullptr;
        return reinterpret_cast<const T*>(&m_Storage[index]);
    }

    // Get index of an object
    int32_t GetIndex(const T* obj) const {
        if (!obj) return INVALID_POOL_SLOT;
        auto idx = static_cast<int32_t>(
            (reinterpret_cast<const unsigned char*>(obj) -
             reinterpret_cast<const unsigned char*>(m_Storage)) / sizeof(StorageType));
        if (idx < 0 || idx >= m_Capacity) return INVALID_POOL_SLOT;
        return idx;
    }

    // Check if a slot is free
    bool IsFreeSlotAtIndex(int32_t index) const {
        if (index < 0 || index >= m_Capacity) return false;
        return m_SlotState[index].IsEmpty();
    }

    // Clear all
    void Clear() {
        for (int32_t i = 0; i < m_Capacity; i++) {
            if (!m_SlotState[i].IsEmpty()) {
                reinterpret_cast<T*>(&m_Storage[i])->~T();
                m_SlotState[i].SetEmpty(true);
            }
        }
        m_UsedCount = 0;
    }

private:
    StorageType* m_Storage{ nullptr };
    SlotState*   m_SlotState{ nullptr };
    int32_t      m_Capacity{ 0 };
    int32_t      m_UsedCount{ 0 };
    bool         m_OwnsAllocations{ false };
    const char*  m_Name{ "Unspecified" };
};
