#ifndef AV_DEVICE_EXECUTOR_CL_READ_AHEAD_HPP
#define AV_DEVICE_EXECUTOR_CL_READ_AHEAD_HPP

#include "executor/opencl/staging_bytes.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

class ByteLedger;

class ClReadAhead {
public:
    static constexpr size_t cache_bytes = 8u << 20;

    ClReadAhead();
    ClReadAhead(const ClReadAhead &) = delete;
    ClReadAhead &operator=(const ClReadAhead &) = delete;

    [[nodiscard]] bool read(cl_command_queue queue, cl_mem buffer, size_t offset, size_t size, void *into,
                            cl_int &result);
    void               forget();
    void use_host_ram(ByteLedger *ledger);
    void release_staging();

private:
    StagingBytes     bytes_;
    cl_command_queue queue_;
    cl_mem           buffer_;
    size_t           offset_;
    size_t           held_;
};

}

#endif
