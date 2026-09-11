/* The executor library the glue loaded, or none: its operations table once
 * checked, and the calls into it. The table outlives the device. */
#ifndef AV_EXECUTOR_LIBRARY_HPP
#define AV_EXECUTOR_LIBRARY_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary {
public:
    ExecutorLibrary();

    /* Null leaves the device without an executor. invalid_parameter for a
     * table of another ABI version or missing an operation; invalid_state once
     * a table is accepted. */
    [[nodiscard]] Status accept(const RvkExecutorOps *ops);
    bool                 available() const;
    /* The sub-APIs the library offers that the protocol knows; 0 without one. */
    uint32_t             sub_apis() const;
    /* Only on an available library. */
    RvkExecutor         *create(SubApi api, const uint8_t *gpu_uuid) const;
    void                 destroy(RvkExecutor *executor) const;
    uint32_t             execute(RvkExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                                 uint8_t *reply, size_t reply_capacity, size_t &reply_size) const;

private:
    static constexpr uint32_t known_sub_apis = 7u;

    const RvkExecutorOps *ops_;
};

}  // namespace av

#endif  // AV_EXECUTOR_LIBRARY_HPP
