#ifndef AV_CONTEXT_CAPACITY_HPP
#define AV_CONTEXT_CAPACITY_HPP

#include <stdint.h>

namespace av {

struct ContextCapacity {
    static constexpr uint32_t contexts = 256u;
    static constexpr uint32_t fence_queue_first = 2u;
    static constexpr uint32_t special_lines = 2u;
    static constexpr uint32_t queues = fence_queue_first + contexts + special_lines;
    static constexpr uint32_t most_queues = 1024u;
    static constexpr uint32_t doorbell_page_bytes = 4096u;
    static constexpr uint32_t most_msix_vectors = 2048u;

    static_assert(queues <= most_queues, "the contexts, their fence lines and the two display lines fit one virtio device");
    static_assert(queues <= most_msix_vectors, "every queue can have its own vector");
};

}

#endif
