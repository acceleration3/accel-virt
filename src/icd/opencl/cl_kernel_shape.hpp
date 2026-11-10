#ifndef AV_CLICD_CL_KERNEL_SHAPE_HPP
#define AV_CLICD_CL_KERNEL_SHAPE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

struct ClKernelShapeProbe;

struct ClKernelLimits {
    size_t   group_size_;
    size_t   compiled_group_[3];
    uint64_t static_local_bytes_;
    size_t   device_group_size_;
    size_t   item_sizes_[3];
    cl_uint  item_dimensions_;
    uint64_t device_local_bytes_;
};


class ClKernelShape {
public:
    enum class ArgKind : uint8_t { unknown, value, buffer, local, image, sampler };

    using Limits = ClKernelLimits;

    explicit ClKernelShape(BlockSource &memory);
    ClKernelShape(const ClKernelShape &) = delete;
    ClKernelShape &operator=(const ClKernelShape &) = delete;

    [[nodiscard]] Status size_to(uint32_t count);
    void                 describe(uint32_t index, ArgKind kind, uint32_t bytes);
    void                 limit_by(const Limits &limits);
    [[nodiscard]] Status copy_from(const ClKernelShape &other);

    uint32_t count() const;
    ArgKind  kind(uint32_t index) const;

    [[nodiscard]] cl_int judge_argument(uint32_t index, size_t bytes, const void *value) const;
    void                 note_set(uint32_t index, size_t bytes, const void *value);
    void                 note_set_pointer(uint32_t index);
    [[nodiscard]] cl_int judge_launch(cl_uint dimensions, const size_t *offset, const size_t *global,
                                      const size_t *local, bool non_uniform) const;

    bool invariants_hold() const;

private:
    friend struct ClKernelShapeProbe;

    struct Arg {
        ArgKind  kind_;
        bool     set_;
        uint32_t bytes_;
        uint64_t local_bytes_;
    };

    bool     all_set() const;
    uint64_t local_bytes() const;

    GrowableArray<Arg> args_;
    Limits             limits_;
};

}

#endif
