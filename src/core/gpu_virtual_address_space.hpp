/* The GPU virtual address space of one device.
 *
 * The driver reports GPU MMU support, so VidMm hands out GPU virtual addresses
 * and asks the driver to maintain the translation. There is no real page
 * table: a virtual range translates to (device object, offset), which the QEMU
 * device resolves to host memory.
 * See ../claude-scratch/wddm-kb/kb/04-memory.md. */
#ifndef AV_GPU_VIRTUAL_ADDRESS_SPACE_HPP
#define AV_GPU_VIRTUAL_ADDRESS_SPACE_HPP

#include "alignment.hpp"
#include "byte_range.hpp"
#include "platform.hpp"

namespace av {

struct GpuVirtualAddressSpaceProbe;

/* One reserved, and possibly bound, span of the address space. */
class VirtualRange {
public:
    VirtualRange();
    explicit VirtualRange(const ByteRange &span);

    const ByteRange &span() const;
    bool             is_mapped() const;
    std::uint32_t    object() const;
    std::uint64_t    object_offset() const;

    [[nodiscard]] Status bind(std::uint32_t object, std::uint64_t object_offset);
    [[nodiscard]] Status unbind();

    bool translate(std::uint64_t address, std::uint32_t &object,
                   std::uint64_t &object_offset) const;

    bool invariants_hold(const ByteRange &bounds, const Alignment &page) const;

private:
    ByteRange     span_;
    std::uint32_t object_;
    std::uint64_t object_offset_;
    bool          mapped_;

    friend struct GpuVirtualAddressSpaceProbe;
};

class GpuVirtualAddressSpace {
public:
    static constexpr std::uint64_t page_size            = 4096u;
    static constexpr std::uint32_t maximum_range_count  = 65536u;

    explicit GpuVirtualAddressSpace(Platform &platform);
    ~GpuVirtualAddressSpace();
    GpuVirtualAddressSpace(const GpuVirtualAddressSpace &) = delete;
    GpuVirtualAddressSpace &operator=(const GpuVirtualAddressSpace &) = delete;

    [[nodiscard]] Status initialize(const ByteRange &bounds);

    [[nodiscard]] Status reserve(std::uint64_t size, const Alignment &alignment,
                                 std::uint64_t &reserved_base);
    [[nodiscard]] Status map(std::uint64_t base, std::uint64_t size,
                             std::uint32_t object, std::uint64_t object_offset);
    [[nodiscard]] Status unmap(std::uint64_t base);
    [[nodiscard]] Status release(std::uint64_t base);

    /* On the submission path: must not allocate or block. */
    [[nodiscard]] Status translate(std::uint64_t address, std::uint32_t &object,
                                   std::uint64_t &object_offset) const;

    bool          invariants_hold() const;
    std::uint32_t range_count() const;

    /* Lowered by tests so the table-exhaustion path is reachable without
     * allocating 64K ranges. */
    void set_range_ceiling(std::uint32_t ceiling);

private:
    [[nodiscard]] Status grow();
    [[nodiscard]] Status insert(std::uint32_t index, const VirtualRange &range);
    std::uint32_t        lower_bound(std::uint64_t base) const;
    VirtualRange        *find_exact(std::uint64_t base);

    Platform     &platform_;
    VirtualRange *ranges_;
    std::uint32_t count_;
    std::uint32_t capacity_;
    std::uint32_t ceiling_;
    ByteRange     bounds_;
    Alignment     page_;

    friend struct GpuVirtualAddressSpaceProbe;
};

}  // namespace av

#endif  // AV_GPU_VIRTUAL_ADDRESS_SPACE_HPP
