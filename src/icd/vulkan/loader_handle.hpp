#ifndef AV_ICD_LOADER_HANDLE_HPP
#define AV_ICD_LOADER_HANDLE_HPP

#include <stdint.h>

namespace av {
template <typename T>
class ObjectPool;
}

namespace av::icd {

class CommandBatch;
class Connection;
class DeviceMemory;
struct LoaderHandleProbe;

class LoaderHandle {
public:
    enum class Kind : uint32_t { instance, physical_device, device, queue, command_buffer };

    static constexpr uintptr_t loader_magic = 0x01CDC0DEu;

    LoaderHandle(Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent);
    ~LoaderHandle();
    LoaderHandle(const LoaderHandle &) = delete;
    LoaderHandle &operator=(const LoaderHandle &) = delete;

    Kind          kind() const;
    uint64_t      host() const;
    Connection   *connection() const;
    LoaderHandle *parent() const;
    bool          names(Kind kind, uint64_t host, const Connection *connection) const;
    void          record_into(ObjectPool<CommandBatch> &batches, CommandBatch &batch);
    CommandBatch *batch() const;
    void          allocate_from(ObjectPool<DeviceMemory> &memories, DeviceMemory &memory);
    DeviceMemory *device_memory() const;
    void          keep_idle_fence(uint64_t fence);
    uint64_t      idle_fence() const;
    void          mark_idle(uint64_t submissions);
    uint64_t      idle_at() const;

    bool invariants_hold() const;

private:
    void         *loader_data_;
    uint64_t      host_;
    Connection   *connection_;
    Kind          kind_;
    LoaderHandle *parent_;
    ObjectPool<CommandBatch> *batches_;
    CommandBatch             *batch_;
    ObjectPool<DeviceMemory> *memories_;
    DeviceMemory             *memory_;
    uint64_t                  idle_fence_;
    uint64_t                  idle_at_;

    friend struct LoaderHandleProbe;
};

}

#endif
