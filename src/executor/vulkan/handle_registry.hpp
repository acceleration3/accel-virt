/* Every host handle this executor made, and nothing else.
 *
 * A handle crosses the wire as a number, and on the host that number is an
 * attacker's: the generated decoder used to hand it to the driver as a pointer,
 * so a guest naming any address had it dereferenced. Found by
 * tests/fuzz/fuzz_executor_vulkan.cpp on its first run, in vkDestroyInstance.
 *
 * So the host answers a handle only if it made it. Everything a guest can name
 * is in here, put there when the command that created it succeeded and taken
 * out when the command that destroyed it ran. A number that is not is not a
 * handle: it is a frame this host refuses.
 *
 * Taken out with it is everything made through it. A driver that destroys a
 * device frees the device's buffers, and a buffer this registry still knew
 * would be a freed pointer a guest could name (feature 044, T031). */
#ifndef AV_HANDLE_REGISTRY_HPP
#define AV_HANDLE_REGISTRY_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

/* What a handle is, for the five the loader dispatches on. Everything else is
 * `other`: the loader passes a non-dispatchable handle to the driver as the
 * number it is, while a dispatchable one it dereferences to find the table to
 * call through - so a guest that names a queue where a device belongs aborts
 * the host process rather than getting an error back. Found by
 * tests/fuzz/fuzz_executor_vulkan.cpp in vkAllocateMemory.
 *
 * And an image made with the memory source's handle type, the one kind of
 * image its memory may back (VulkanBackend::on_vkCreateImage): kept with the
 * handle, it ends when the image does. */
enum class HandleKind : uint8_t {
    other,
    instance,
    physical_device,
    device,
    queue,
    command_buffer,
    external_image
};

class HandleRegistry {
public:
    /* A Vulkan context of any size lives inside this: it is objects the guest
     * created, not commands it sent, and a guest that wants more is refused
     * rather than served. A game keeps a buffer per chunk section and a view
     * per texture: Minecraft's Vulkan renderer passed 4096 while its world was
     * still loading. */
    static constexpr uint32_t max_handles = 65536u;

    HandleRegistry();
    ~HandleRegistry();
    HandleRegistry(const HandleRegistry &) = delete;
    HandleRegistry &operator=(const HandleRegistry &) = delete;
    HandleRegistry(HandleRegistry &&) = delete;
    HandleRegistry &operator=(HandleRegistry &&) = delete;

    /* Remembers `handle`, which this executor just made through `parent`: the
     * null handle or one it does not know for a handle made through nothing.
     * One already known keeps the parent it was made through. False when full,
     * which the caller turns into a refusal rather than a silent forget. */
    [[nodiscard]] bool remember(uint64_t handle, uint64_t parent, HandleKind kind = HandleKind::other);
    /* Whether this executor made `handle`. The null handle is always known:
     * the API defines it and a guest may legitimately send it. */
    bool               knows(uint64_t handle) const;
    /* Whether this executor made `handle` as `kind`. A handle it made as
     * something else is no more this one than a number it never made, and
     * the null handle is no kind of handle: no dispatchable one, and no image
     * made for the memory source. */
    bool               knows(uint64_t handle, HandleKind kind) const;
    /* Forgets one it made, and everything made through it at any depth.
     * Nothing for one it did not. */
    void               forget(uint64_t handle);
    /* Forgets everything made through `handle`, at any depth, and not
     * `handle` itself. */
    void               forget_children(uint64_t handle);
    /* Forgets everything and gives the table's memory back. */
    void               forget_all();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct HandleRegistryProbe;

    /* No slot: a lookup that found none, and the parent of a slot made
     * through nothing or free. */
    static constexpr uint32_t no_slot = 0xFFFFFFFFu;
    /* Twice the slots, so the index is never more than half full and a probe
     * always ends at an empty bucket. */
    static constexpr uint32_t index_capacity = 2u * max_handles;
    static constexpr uint32_t min_buckets = 64u;
    /* 2^64 over the golden ratio: host handles are aligned pointers, whose low
     * bits are all alike, and multiplying by this spreads the ones that differ
     * into the bits a bucket is taken from. */
    static constexpr uint64_t spread = 0x9E3779B97F4A7C15ull;

    /* Every array below, in one mapping: 1.9 MB at full size. */
    static constexpr size_t table_bytes = max_handles * (sizeof(uint64_t) + 3u * sizeof(uint32_t) + sizeof(HandleKind)) +
                                          index_capacity * sizeof(uint32_t);

    /* Maps the table on the first handle a context makes. */
    bool     reserve();
    /* The slot holding `handle`, or no_slot. */
    uint32_t slot_of(uint64_t handle) const;
    /* The bucket holding `handle`, or the empty one it would go in. */
    uint32_t bucket_of(uint64_t handle) const;
    uint32_t home_of(uint64_t handle) const;
    /* Empties `bucket`, moving back the entries after it that probed past it,
     * so no lookup ever needs a tombstone to keep going. */
    void     unindex(uint32_t bucket);
    /* The index at `buckets`, from the live slots. */
    void     rebuild(uint32_t buckets);
    void     free_slot(uint32_t slot);
    /* Frees every slot whose parent is free, until none is. */
    void     sweep_orphans();

    /* The table lives in memory mapped for the context that uses it, not in
     * the object: every executor session holds one of these whether or not
     * its guest ever speaks Vulkan, and a table that size in each of them was
     * 120 MB of QEMU for nothing. A fresh mapping is zero and its pages cost
     * nothing until touched, so a context pays for the handles it made.
     *
     * Slots below `end_` hold a handle, or zero for a free one on `free_`;
     * nothing at or above `end_` is ever read. A parent is a slot rather than
     * a handle so that finding the orphans of a free slot is one pass, not one
     * search per entry, and `children_` says whether a slot has any to find:
     * forgetting a leaf, which most handles are, touches nothing else. The
     * index maps a handle to its slot in the first `buckets_` buckets, a slot
     * plus one each, zero for empty. */
    void       *table_;
    uint64_t   *handles_;
    uint32_t   *parents_;
    uint32_t   *children_;
    uint32_t   *free_;
    uint32_t   *index_;
    HandleKind *kinds_;
    uint32_t    buckets_;
    uint32_t    free_count_;
    uint32_t    end_;
    uint32_t    count_;
};

}  // namespace av

#endif  // AV_HANDLE_REGISTRY_HPP
