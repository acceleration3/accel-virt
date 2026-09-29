/* The OpenGL executors a device's contexts may share objects with (feature 050,
 * contracts/context_options.md §3): each published by the worker that made it,
 * by its context's id and GPU. A worker making another context finds one here
 * and creates its executor with it while holding the lock, so the executor it
 * shares is not withdrawn - and so not destroyed - until that create returns.
 * Any thread. One per device. */
#ifndef AV_EXECUTOR_SHARES_HPP
#define AV_EXECUTOR_SHARES_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class ExecutorLibrary;
class Host;
struct ExecutorSharesProbe;

class ExecutorShares {
public:
    /* One per context a device can hold (DeviceContextTable::max_contexts). */
    static constexpr uint32_t capacity = 64u;
    static constexpr uint32_t uuid_bytes = 16u;

    ExecutorShares(Host &host, const ExecutorLibrary &library);
    ~ExecutorShares();
    ExecutorShares(const ExecutorShares &) = delete;
    ExecutorShares &operator=(const ExecutorShares &) = delete;

    /* The lock: invalid_state once made, no_memory when the host has none. */
    [[nodiscard]] Status initialize();
    /* Context `id`'s executor on the GPU `uuid` names may be shared from now
     * on. invalid_state before initialize; invalid_parameter for id 0 or no
     * executor; conflict for an id published already; no_space when full. */
    [[nodiscard]] Status publish(uint32_t id, const uint8_t *uuid, RvkExecutor *executor);
    /* Not any more; returns once no create is using it. */
    void                 withdraw(uint32_t id);
    /* An executor of `api` made with `options` sharing context `share_id`'s
     * OpenGL objects on the same GPU - an OpenGL one sharing lists, an OpenCL
     * one under cl_khr_gl_sharing; null, with nothing asked of the library,
     * when there is no such executor, and null when the library makes none. */
    RvkExecutor         *create_sharing(SubApi api, uint32_t share_id, const uint8_t *uuid,
                                        const RvkContextOptions &options);
    uint32_t             published();

    bool invariants_hold() const;

private:
    static constexpr uint32_t none = capacity;

    /* The index publishing `id`, or none; id 0 finds a free one. */
    uint32_t index_of(uint32_t id) const;
    bool     on_gpu(uint32_t index, const uint8_t *uuid) const;

    Host                  &host_;
    const ExecutorLibrary &library_;
    void                  *lock_;
    /* One published executor per index: its context, its GPU, itself. */
    uint32_t               ids_[capacity];
    uint8_t                uuids_[capacity][uuid_bytes];
    RvkExecutor           *executors_[capacity];

    friend struct ExecutorSharesProbe;
};

}  // namespace av

#endif  // AV_EXECUTOR_SHARES_HPP
