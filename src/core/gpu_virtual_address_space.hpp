/* The GPU virtual address space of one device.
 *
 * The driver reports GPU MMU support, so VidMm hands out GPU virtual addresses
 * and asks the driver to maintain the translation. There is no real page
 * table: a virtual range translates to (device object, offset), which the QEMU
 * device resolves to host memory.
 * See docs/kb/04-memory.md. */
#ifndef AV_GPU_VIRTUAL_ADDRESS_SPACE_HPP
#define AV_GPU_VIRTUAL_ADDRESS_SPACE_HPP

#include "alignment.hpp"
#include "byte_range.hpp"
#include "platform.hpp"
#include "status.hpp"

namespace av {

struct GpuVirtualAddressSpaceProbe;

/* One reserved, and possibly bound, span of the address space. */
class VirtualRange {
public:
    VirtualRange();
    explicit VirtualRange(const ByteRange &span);

    const ByteRange &span() const;
    bool             is_mapped() const;
    uint32_t    object() const;
    uint64_t    object_offset() const;

    [[nodiscard]] Status bind(uint32_t object, uint64_t object_offset);
    [[nodiscard]] Status unbind();

    bool translate(uint64_t address, uint32_t &object,
                   uint64_t &object_offset) const;

    bool invariants_hold(const ByteRange &bounds, const Alignment &page) const;

private:
    ByteRange     span_;
    uint32_t object_;
    uint64_t object_offset_;
    bool          mapped_;

    friend struct GpuVirtualAddressSpaceProbe;
};

class GpuVirtualAddressSpace {
public:
    static constexpr uint64_t page_size            = 4096u;
    static constexpr uint32_t maximum_range_count  = 65536u;

    explicit GpuVirtualAddressSpace(Platform &platform);
    ~GpuVirtualAddressSpace();
    GpuVirtualAddressSpace(const GpuVirtualAddressSpace &) = delete;
    GpuVirtualAddressSpace &operator=(const GpuVirtualAddressSpace &) = delete;

    [[nodiscard]] Status initialize(const ByteRange &bounds);

    [[nodiscard]] Status reserve(uint64_t size, const Alignment &alignment,
                                 uint64_t &reserved_base);
    [[nodiscard]] Status map(uint64_t base, uint64_t size,
                             uint32_t object, uint64_t object_offset);
    [[nodiscard]] Status unmap(uint64_t base);
    [[nodiscard]] Status release(uint64_t base);

    /* On the submission path: must not allocate or block. */
    [[nodiscard]] Status translate(uint64_t address, uint32_t &object,
                                   uint64_t &object_offset) const;

    bool          invariants_hold() const;
    uint32_t range_count() const;

    /* Lowered by tests so the table-exhaustion path is reachable without
     * allocating 64K ranges. */
    void set_range_ceiling(uint32_t ceiling);

private:
    [[nodiscard]] Status grow();
    void                 release_table();
    [[nodiscard]] Status insert(uint32_t index, const VirtualRange &range);
    uint32_t        lower_bound(uint64_t base) const;
    VirtualRange        *find_exact(uint64_t base);

    Platform     &platform_;
    VirtualRange *ranges_;
    uint32_t count_;
    uint32_t capacity_;
    uint32_t ceiling_;
    ByteRange     bounds_;
    Alignment     page_;

    friend struct GpuVirtualAddressSpaceProbe;
};

}  // namespace av

#endif  // AV_GPU_VIRTUAL_ADDRESS_SPACE_HPP
