/* One context's host executor: configured at setup, created by the context's
 * worker when its ring attaches and destroyed by it before the worker exits,
 * so a host API with thread affinity (a current GL context) sees one thread. */
#ifndef AV_CONTEXT_EXECUTOR_HPP
#define AV_CONTEXT_EXECUTOR_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary;
class Host;
struct ContextExecutorProbe;

class ContextExecutor {
public:
    static constexpr size_t uuid_bytes = 16u;

    ContextExecutor(Host &host, const ExecutorLibrary &library);
    /* An executor the worker never destroyed is released here, off its thread. */
    ~ContextExecutor();
    ContextExecutor(const ContextExecutor &) = delete;
    ContextExecutor &operator=(const ContextExecutor &) = delete;

    /* Main loop, at setup: the executor create() makes. */
    void                 configure(SubApi api, const uint8_t *gpu_uuid);
    /* Worker. invalid_state when created or not configured; no_memory when
     * the library makes none. */
    [[nodiscard]] Status create();
    /* Worker. */
    void                 destroy();
    /* Worker. ok when it ran, with `reply_size` set; not_found for a command
     * the library does not know; malformed for one it cannot decode;
     * invalid_state for a lost device, an answer no library may give, or no
     * executor. */
    [[nodiscard]] Status execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                                 size_t reply_capacity, size_t &reply_size);
    /* The API half (id >> 16) of the commands this context runs. */
    uint16_t             command_api() const;
    bool                 created() const;

    bool invariants_hold() const;

private:
    /* render-server's API ids (rvk/wire/api_id.hpp), not the capset's bits. */
    static constexpr uint16_t vulkan_commands = 1u;
    static constexpr uint16_t opencl_commands = 2u;
    static constexpr uint16_t opengl_commands = 3u;

    Host                  &host_;
    const ExecutorLibrary &library_;
    RvkExecutor           *executor_;
    SubApi                 api_;
    uint8_t                uuid_[uuid_bytes];
    bool                   configured_;

    friend struct ContextExecutorProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_EXECUTOR_HPP
