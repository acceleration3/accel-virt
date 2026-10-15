/* What each parameter of a kernel in a SPIR-V module is, read off the module
 * itself: the answer the server needs to set an argument (a buffer's handle,
 * a sampler's, a local size, or bytes) where the host's OpenCL keeps less.
 *
 * A host need not keep an argument's type name for a program made from IL -
 * rusticl answers its address space and nothing more - and a private argument
 * of eight bytes is then a sampler or a long with nothing to tell them apart.
 * The module says: a parameter whose type is OpTypeSampler is a sampler, a
 * pointer into CrossWorkgroup or UniformConstant storage is memory, into
 * Workgroup storage local, an image or a pipe an object, and every other type
 * bytes. Only what the decision needs is read, and a module that is not
 * well formed - an instruction running past the end, an id past its bound -
 * answers nothing. */
#ifndef AV_EXECUTOR_SPIRV_ARGUMENT_KINDS_HPP
#define AV_EXECUTOR_SPIRV_ARGUMENT_KINDS_HPP

#include "executor/opencl/staging_bytes.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SpirvArgumentKinds {
public:
    SpirvArgumentKinds();
    SpirvArgumentKinds(const SpirvArgumentKinds &) = delete;
    SpirvArgumentKinds &operator=(const SpirvArgumentKinds &) = delete;

    /* What parameter `index` of the kernel entry point `kernel` of the module
     * in `words` (`count` of them) is. False for a module this cannot read,
     * one with no such kernel, or a kernel with no such parameter. */
    [[nodiscard]] bool kind(const uint32_t *words, size_t count, const char *kernel, uint32_t index,
                            clproto::KernelArgument &argument);

private:
    static constexpr uint32_t magic = 0x07230203u;
    static constexpr size_t   header_words = 5u;
    /* More ids than any module a kernel compiler writes; a module claiming
     * more is not one this reads. */
    static constexpr uint32_t most_ids = 1u << 24;

    /* The kind a parameter of type `id` is, from the instruction declaring
     * it; nothing recorded is a scalar, a vector or a structure: bytes. */
    void record_type(const uint32_t *instruction, uint32_t length);
    /* A literal string an instruction carries: UTF-8 packed into words, low
     * byte first, ended by a zero byte inside them. */
    class Literal {
    public:
        Literal(const uint32_t *words, uint32_t count);
        /* Whether it spells `text` exactly, its terminator inside its words. */
        bool spells(const char *text) const;

    private:
        const uint32_t *words_;
        uint32_t        count_;
    };

    /* One byte per id of the module: 0 for none recorded, else a
     * KernelArgument plus one. */
    StagingBytes types_;
    uint32_t     bound_;
};

}  // namespace av

#endif  // AV_EXECUTOR_SPIRV_ARGUMENT_KINDS_HPP
