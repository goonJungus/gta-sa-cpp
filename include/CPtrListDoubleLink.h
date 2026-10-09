// CPtrListDoubleLink.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

#include <cstddef>

// Minimal double-linked pointer list stand-in.
// TODO(port): full CPtrListDoubleLink (gta-reversed).
template<typename T>
class CPtrListDoubleLink {
public:
    // Minimal iterator for range-based for.
    class Iterator {
    public:
        T operator*() const { return nullptr; }
        Iterator& operator++() { return *this; }
        bool operator!=(const Iterator& other) const { return false; }
    };

    Iterator begin() const { return Iterator{}; }
    Iterator end() const { return Iterator{}; }
};
