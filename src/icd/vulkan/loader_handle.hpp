/* One dispatchable Vulkan object as the loader holds it: an instance, a physical
 * device, a device, a queue or a command buffer (contracts/loader_interface.md).
 *
 * The loader treats the address as a structure whose first pointer-sized field
 * is its own: it checks ICD_LOADER_MAGIC there and then overwrites it with its
 * dispatch table. So that word is the first data member, the class has no
 * virtual function to put a vtable pointer in front of it, and nothing here reads
 * it again. Everything after it is the ICD's: the host's handle for the object,
 * the connection its commands cross, and the handle it was made through, whose
 * kind is always a lower one - which is what makes a chain of parents finite. A command buffer also owns the
 * batch its commands record into, and a device the memory it allocated, each of
 * which goes when the handle does, however it is destroyed: by itself, or with
 * its device or instance. */
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
    /* In the order an object is made through another: a parent's kind is lower. */
    enum class Kind : uint32_t { instance, physical_device, device, queue, command_buffer };

    static constexpr uintptr_t loader_magic = 0x01CDC0DEu;

    LoaderHandle(Kind kind, uint64_t host, Connection *connection, LoaderHandle *parent);
    /* Destroys the batch it owns in the pool it came from. */
    ~LoaderHandle();
    LoaderHandle(const LoaderHandle &) = delete;
    LoaderHandle &operator=(const LoaderHandle &) = delete;

    Kind          kind() const;
    uint64_t      host() const;
    Connection   *connection() const;
    LoaderHandle *parent() const;
    /* Whether this is the object `host` names on `connection`, as a `kind`. */
    bool          names(Kind kind, uint64_t host, const Connection *connection) const;
    /* Whether `ancestor` is on the chain of parents above this. */
    bool          descends_from(const LoaderHandle &ancestor) const;
    /* Takes `batch`, made in `batches`, as the batch this command buffer's
     * commands record into. */
    void          record_into(ObjectPool<CommandBatch> &batches, CommandBatch &batch);
    /* The batch it owns; null for anything but a command buffer given one. */
    CommandBatch *batch() const;
    /* Takes `memory`, made in `memories`, as the memory this device allocates
     * from. */
    void          allocate_from(ObjectPool<DeviceMemory> &memories, DeviceMemory &memory);
    /* The memory it owns; null for anything but a device given one. */
    DeviceMemory *device_memory() const;
    /* A queue's fence for vkQueueWaitIdle, the host's handle; 0 until made. */
    void          keep_idle_fence(uint64_t fence);
    uint64_t      idle_fence() const;

    /* An instance has no parent and anything else has one of a lower kind on
     * the same connection; the host never names an object 0; only a command
     * buffer owns a batch and only a device owns memory, each together with the
     * pool it came from. */
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

    friend struct LoaderHandleProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_LOADER_HANDLE_HPP
