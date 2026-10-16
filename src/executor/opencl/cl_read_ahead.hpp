/* A guest's blocking read, served from one larger read the host made ahead of
 * it. The guest reads a buffer in pieces its reply slot holds - 60 KiB each,
 * over a hundred for 8 MiB - and each piece the host read on its own was a
 * GPU round trip; here the first piece reads up to cache_bytes of the buffer
 * and the pieces after it are copies. The copy is dropped the moment any other
 * command arrives (OpenClBackend::run), so no work can have changed the buffer
 * behind it. */
#ifndef AV_DEVICE_EXECUTOR_CL_READ_AHEAD_HPP
#define AV_DEVICE_EXECUTOR_CL_READ_AHEAD_HPP

#include "executor/opencl/staging_bytes.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClReadAhead {
public:
    static constexpr size_t cache_bytes = 8u << 20;

    ClReadAhead();
    ClReadAhead(const ClReadAhead &) = delete;
    ClReadAhead &operator=(const ClReadAhead &) = delete;

    /* `size` bytes of `buffer` at `offset` into `into`, through `queue`, with
     * `result` the read's status; false for a read this does not serve, which
     * the caller makes itself. */
    [[nodiscard]] bool read(cl_command_queue queue, cl_mem buffer, size_t offset, size_t size, void *into,
                            cl_int &result);
    /* What is held may be stale from now on. */
    void               forget();

private:
    /* What the read ahead lands in, grown to the largest made. */
    StagingBytes     bytes_;
    cl_command_queue queue_;
    cl_mem           buffer_;
    size_t           offset_;
    size_t           held_;
};

}  // namespace av

#endif  // AV_DEVICE_EXECUTOR_CL_READ_AHEAD_HPP
