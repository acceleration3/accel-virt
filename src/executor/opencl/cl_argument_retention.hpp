/* Whether the host's OpenCL still knows what a kernel's arguments are once its
 * program was made from the driver's own binary.
 *
 * The server classifies every argument a guest sets by asking the host
 * (clGetKernelArgInfo): eight bytes for a buffer are a handle to resolve, and
 * eight bytes for anything else are data that must never be taken for one. A
 * program built from source is built with -cl-kernel-arg-info, so the host can
 * always say. A program made from a binary keeps that only if the driver's
 * binary carries it, which rusticl's does and a driver need not; where it does
 * not, no buffer could ever be set on such a kernel. So a binary is taken only
 * from a host that keeps it, which is measured once rather than assumed: a
 * one-kernel program round-tripped through its own binary on the guest's
 * context. */
#ifndef AV_EXECUTOR_CL_ARGUMENT_RETENTION_HPP
#define AV_EXECUTOR_CL_ARGUMENT_RETENTION_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class ClArgumentRetention {
public:
    ClArgumentRetention();
    ClArgumentRetention(const ClArgumentRetention &) = delete;
    ClArgumentRetention &operator=(const ClArgumentRetention &) = delete;

    /* Measured on `context` and `device` the first time it is asked, and the
     * same answer every time after. A host that cannot even build the
     * measuring program keeps nothing. */
    bool kept(cl_context context, cl_device_id device);

private:
    enum class Answer : uint8_t { unmeasured, kept, lost };

    static constexpr const char *source = "__kernel void kept(__global int *argument) { argument[0] = 0; }\n";

    Answer answer_;
};

}  // namespace av

#endif  // AV_EXECUTOR_CL_ARGUMENT_RETENTION_HPP
