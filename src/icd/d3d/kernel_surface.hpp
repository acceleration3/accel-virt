#ifndef AV_KERNEL_SURFACE_HPP
#define AV_KERNEL_SURFACE_HPP

#include "core/support/status.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_allocation_data.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {
enum class SharedWith { another_adapter };

class KernelSurface {
public:
    static constexpr size_t allocation_bytes = SharedAllocationData::encoded_size;
    static constexpr size_t resource_bytes = 2u * sizeof(uint64_t);

    KernelSurface();
    KernelSurface(uint32_t width, uint32_t height, uint32_t format);
    KernelSurface(SharedWith, uint32_t width, uint32_t height, uint32_t format);
    KernelSurface(uint32_t width, uint32_t height, uint32_t format, const ScanoutTarget &scanout);
    KernelSurface(const KernelSurface &) = delete;
    KernelSurface &operator=(const KernelSurface &) = delete;

    [[nodiscard]] Status make();
    [[nodiscard]] Status open(const void *resource, size_t resource_size, const void *allocation,
                              size_t allocation_size);

    uint8_t        *allocation_data();
    const uint64_t *resource_data() const;
    uint32_t        pitch() const;
    uint64_t        bytes() const;
    uint64_t        key() const;
    bool            primary() const;
    uint32_t        source() const;

private:
    static constexpr uint64_t mark = 0x53564B52u;

    uint32_t kernel_format() const;

    SharedAllocationData allocation_;
    uint64_t      resource_[2];
    uint32_t      width_;
    uint32_t      rows_;
    uint32_t      format_;
    uint32_t      pitch_;
    uint64_t      bytes_;
    ScanoutTarget scanout_;
    bool          primary_;
    bool          cross_adapter_;
};

}

#endif
