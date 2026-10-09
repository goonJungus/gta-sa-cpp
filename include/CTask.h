#pragma once
// CTask - minimal base class for the task hierarchy.
// The full task system (gta-reversed Tasks.h) lands with the tasks batch.
// TODO(port): full CTask port (added 2026-10-09 for CPed).
class CTask {
public:
    virtual ~CTask() = default; // TODO(port): stub
    // TODO(port): full task type system; added 2026-10-09 for CPed::KillPedWithCar.
    //   Original called vtable+0x10 (GetTaskType in the binary). Returns eTaskType
    //   as int32_t to avoid pulling the enum into this header.
    virtual int32_t GetTaskType() const { return -1; } // TASK_INVALID
};
