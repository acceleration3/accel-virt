#ifndef AV_CLICD_CL_KERNEL_ARGS_HPP
#define AV_CLICD_CL_KERNEL_ARGS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

class ClObject;

class ClKernelArgs {
public:
    enum class Kind : uint8_t { value, object, local };

    struct Arg {
        uint32_t  index;
        Kind      kind;
        uint32_t  offset;
        uint32_t  bytes;
        ClObject *object;
    };

    explicit ClKernelArgs(BlockSource &memory);
    ClKernelArgs(const ClKernelArgs &) = delete;
    ClKernelArgs &operator=(const ClKernelArgs &) = delete;

    [[nodiscard]] Status set_value(uint32_t index, const void *value, size_t bytes);
    [[nodiscard]] Status set_object(uint32_t index, ClObject *object);
    [[nodiscard]] Status set_local(uint32_t index, size_t bytes);
    [[nodiscard]] Status copy_from(const ClKernelArgs &other);

    uint32_t       count() const;
    const Arg     &at(uint32_t position) const;
    const uint8_t *bytes_of(const Arg &arg) const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status place(const Arg &arg);

    GrowableArray<Arg>     args_;
    GrowableArray<uint8_t> bytes_;
};

}

#endif
