/* The objects this process exported, and the identities they were published
 * under (feature 047 US3, feature 053 US2; contracts/external_objects.md).
 *
 * A handle is an NT handle over a carrier allocation, minted by the kernel and
 * resolved by `icd::SharedHandles`, so the refusal of "a handle this process
 * did not export" lives there and this table does not guess at it. What is
 * left here is what only the ICD knows of an object it exported: which host
 * object a handle names *in this process*, which identity it was published
 * under, and the carrier allocation that identity was minted into, which
 * nothing else will destroy. What this process opened from a handle is its
 * connection's (icd::OpenedObjects), found by the handle the device answered,
 * which is the Vulkan object itself.
 *
 * The lock is its own: an export happens on the calling application's thread,
 * and nothing it does reaches back into the root. */
#ifndef AV_ICD_EXTERNAL_OBJECTS_HPP
#define AV_ICD_EXTERNAL_OBJECTS_HPP

#include <stdint.h>

#include "core/support/object_pool.hpp"

#include <vulkan/vulkan_core.h>

namespace av::icd {

struct ExternalObjectsProbe;

}  // namespace av::icd

namespace av {
class BlockSource;
class ThreadLock;
}

namespace av::icd {

/* One object this process exported. A type of its own so the table's slots are
 * complete (tools/header_scan.py reads a class's members only to its first
 * nested type). */
struct ExternalObjectSlot {
    VkDevice     device_;
    /* Exactly one of the two is set; the other is null. */
    VkDeviceMemory memory_;
    VkSemaphore    semaphore_;
    /* What the object is called on the wire and in every other process. */
    uint64_t       object_id_;
    /* The bytes it was published as - whole pages, and 0 for a fence, which is
     * what the wire body demands of each kind. */
    uint64_t       size_;
    /* The NT handle that names `object_id_` to another process. */
    uint64_t       handle_;
    /* The one-page allocation the identity was minted into, which this ICD
     * created and must destroy. */
    uint32_t       carrier_;
};

class ExternalObjects {
public:
    /* Rows are kept in `memory`: as many as the process shares, with no
     * count past which one is refused. */
    ExternalObjects(ThreadLock &lock, BlockSource &memory);
    ExternalObjects(const ExternalObjects &) = delete;
    ExternalObjects &operator=(const ExternalObjects &) = delete;

    /* An object this process exported, with the identity it was published
     * under, the handle that names it and the carrier the identity was minted
     * into. Exactly one of `memory` and `semaphore` is set. False for neither
     * object or both, an id, a handle or a carrier of 0, an object already
     * held, and no memory for a row - the one a caller that passes a valid
     * row can meet. */
    [[nodiscard]] bool remember(VkDevice device, VkDeviceMemory memory, VkSemaphore semaphore,
                                uint64_t object_id, uint64_t size, uint64_t handle, uint32_t carrier);
    /* The NT handle `memory` was published under, or 0 for memory this process
     * did not share - which is what an application asking for a handle to
     * memory it never declared exportable is told. */
    uint64_t           handle_of_memory(VkDeviceMemory memory) const;
    uint64_t           handle_of_semaphore(VkSemaphore semaphore) const;
    /* The row published under `object_id`, copied into `row`; false for an id
     * this process did not export - which is what an object another process
     * published looks like from here. */
    [[nodiscard]] bool holder_of(uint64_t object_id, ExternalObjectSlot &row) const;
    /* The row naming the object, dropped and copied into `row`, so its caller
     * can release what it names. False for an object this table does not hold,
     * which leaves `row` alone. */
    [[nodiscard]] bool take_memory(VkDeviceMemory memory, ExternalObjectSlot &row);
    [[nodiscard]] bool take_semaphore(VkSemaphore semaphore, ExternalObjectSlot &row);
    /* One row of `device`, dropped: what a device's teardown releases them all
     * through. False once it holds none. */
    [[nodiscard]] bool take_any_of_device(VkDevice device, ExternalObjectSlot &row);

    uint32_t live() const;

    /* A row names exactly one object of one device under one non-zero id,
     * handle and carrier, and no object, id or handle appears twice. */
    bool invariants_hold() const;

private:
    /* The slot naming the object, or null. The caller holds the lock. */
    ExternalObjectSlot       *find(VkDeviceMemory memory, VkSemaphore semaphore);
    const ExternalObjectSlot *find(VkDeviceMemory memory, VkSemaphore semaphore) const;
    /* The row copied out and dropped. The caller holds the lock. */
    void                      give_back(ExternalObjectSlot &slot, ExternalObjectSlot &row);

    ThreadLock                             &lock_;
    mutable ObjectPool<ExternalObjectSlot> slots_;

    friend struct ExternalObjectsProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_EXTERNAL_OBJECTS_HPP
