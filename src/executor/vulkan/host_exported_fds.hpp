/* The fds this context exported from its host objects, and what an import may
 * name (feature 047, US3, research.md §10).
 *
 * A guest device is a host device, so two guest devices sharing an image share
 * it between two host devices - which is what the host's own fd extensions do,
 * inside the one process both live in. The guest's ICD therefore turns a Win32
 * handle into an import by asking for a fresh fd of the object the handle
 * names, and every fd an import carries is one this table handed out. An fd a
 * guest made up names something else in this process entirely (constitution
 * V), so it is refused rather than imported.
 *
 * The table also owns what it exported: an fd the guest asked for and never
 * imported is closed with the device, and with the context, because a handle
 * this process holds until it exits is a leak a guest could drive. Per
 * context, on that context's own thread, so it needs no lock. */
#ifndef AV_HOST_EXPORTED_FDS_HPP
#define AV_HOST_EXPORTED_FDS_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

struct HostExportedFdsProbe;

/* One exported fd and the device it came from. A type of its own so the
 * table's slots are complete (tools/header_scan.py reads a class's members
 * only to its first nested type). */
struct HostExportedFdSlot {
    VkDevice device_;
    /* Negative for an empty slot: 0 is an fd. */
    int      fd_;
};

class HostExportedFds {
public:
    /* As many as the ICD's own table of exported handles holds
     * (ExternalObjects::max_objects): a guest that exported more than this
     * could not have named them. */
    static constexpr uint32_t max_fds = 64u;

    HostExportedFds();
    ~HostExportedFds();
    HostExportedFds(const HostExportedFds &) = delete;
    HostExportedFds &operator=(const HostExportedFds &) = delete;

    /* Remembers `fd` as `device`'s, taking ownership of it. False for no fd,
     * no device, and a table with no room - in which case the fd is still the
     * caller's to close, since a table that both refused and closed would be
     * one no caller could reason about. */
    [[nodiscard]] bool keep(VkDevice device, int fd);
    /* Whether `fd` is one this context exported, which is what an import may
     * name. */
    bool               holds(int fd) const;
    /* The same, and forgotten with it: the driver an import succeeded on owns
     * the fd it was given, so this table must not close it after. A refused
     * import leaves the fd here, to be closed with its device. */
    [[nodiscard]] bool take(int fd);
    /* Every fd `device` exported and nothing imported, closed with it. */
    void               forget_device(VkDevice device);
    /* Every fd, closed, whichever device exported it: a closing context's,
     * which the next context made in its slot must not be able to name. */
    void               forget_all();
    uint32_t           live() const;

    /* A slot is empty or holds one fd of one device, and no fd appears
     * twice. */
    bool invariants_hold() const;

private:
    /* The slot holding `fd`, or -1. */
    int  index_of(int fd) const;
    void release(HostExportedFdSlot &slot);

    HostExportedFdSlot slots_[max_fds];

    friend struct HostExportedFdsProbe;
};

}  // namespace av

#endif  // AV_HOST_EXPORTED_FDS_HPP
