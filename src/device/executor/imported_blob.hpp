/* One guest blob, held inside one host API.
 *
 * The handle the device hands over is whatever that API can take without a
 * copy - a host pointer on every GPU measured for Vulkan, a dma-buf where
 * OpenGL and OpenCL can take one - and this is where it is checked before any
 * of them sees it. Everything in it came from outside. */
#ifndef AV_IMPORTED_BLOB_HPP
#define AV_IMPORTED_BLOB_HPP

#include "core/support/status.hpp"

#include <stdint.h>

struct RvkBlobHandle;

namespace av {

class ImportedBlob {
public:
    /* The handle's `size` is a whole number of these. Vulkan's
     * minImportedHostPointerAlignment was 0x1000 on every GPU measured, and
     * the device's own blob granularity is the same. */
    static constexpr uint64_t page_bytes = 4096u;

    ImportedBlob();
    ImportedBlob(const ImportedBlob &) = delete;
    ImportedBlob &operator=(const ImportedBlob &) = delete;

    /* Takes `handle` for `blob_id`. invalid_parameter for a handle that breaks
     * any of the ABI's rules - no kind or two, a size that is not whole pages,
     * a pointer or a descriptor where the kind says there should be none - and
     * invalid_state for a slot already holding one. The reserved field is the
     * memory type it asks to be imported into, which only the backend can
     * judge. */
    [[nodiscard]] Status take(uint64_t blob_id, const RvkBlobHandle &handle);
    /* Gives the slot back. Safe on one that holds nothing. */
    void                 give_back();

    bool     holds(uint64_t blob_id) const;
    bool     live() const;
    uint64_t id() const;
    uint32_t kind() const;
    uint64_t size() const;
    void    *pointer() const;
    int      descriptor() const;
    uint32_t memory_type() const;

    /* What the host API made of it - a VkDeviceMemory, a GL memory object, a
     * cl_mem - as the one number every API's handle fits in, and the number
     * the guest names it by in that API's commands, 0 where it names it by
     * nothing. The backend records both when it adopts the blob and records
     * none when it gives them back. */
    void     record(uint64_t object, uint64_t wire_handle);
    uint64_t object() const;
    uint64_t wire_handle() const;

    bool invariants_hold() const;

private:
    friend struct ImportedBlobProbe;

    uint64_t id_;
    uint32_t kind_;
    uint64_t size_;
    void    *pointer_;
    int      descriptor_;
    uint32_t memory_type_;
    uint64_t object_;
    uint64_t wire_handle_;
    bool     live_;
};

}  // namespace av

#endif  // AV_IMPORTED_BLOB_HPP
