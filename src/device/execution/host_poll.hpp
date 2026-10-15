/* What a context's executor says when the worker polls it between batches
 * (AvExecutorOps::poll): nothing left to publish, results still awaited from
 * the GPU, or the host device it runs on lost - which faults the context. */
#ifndef AV_HOST_POLL_HPP
#define AV_HOST_POLL_HPP

#include <stdint.h>

namespace av {

enum class HostPoll : uint8_t { idle, pending, lost };

}  // namespace av

#endif  // AV_HOST_POLL_HPP
