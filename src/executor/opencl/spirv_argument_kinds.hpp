#ifndef AV_EXECUTOR_SPIRV_ARGUMENT_KINDS_HPP
#define AV_EXECUTOR_SPIRV_ARGUMENT_KINDS_HPP

#include "executor/opencl/staging_bytes.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ByteLedger;

class SpirvArgumentKinds {
public:
    SpirvArgumentKinds();
    SpirvArgumentKinds(const SpirvArgumentKinds &) = delete;
    SpirvArgumentKinds &operator=(const SpirvArgumentKinds &) = delete;

    void use_host_ram(ByteLedger *ledger);
    void release_staging();

    [[nodiscard]] bool kind(const uint32_t *words, size_t count, const char *kernel, uint32_t index,
                            clproto::KernelArgument &argument);

private:
    static constexpr uint32_t magic = 0x07230203u;
    static constexpr size_t   header_words = 5u;
    static constexpr uint32_t most_ids = 1u << 24;

    void record_type(const uint32_t *instruction, uint32_t length);
    class Literal {
    public:
        Literal(const uint32_t *words, uint32_t count);
        bool spells(const char *text) const;

    private:
        const uint32_t *words_;
        uint32_t        count_;
    };

    StagingBytes types_;
    uint32_t     bound_;
};

}

#endif
