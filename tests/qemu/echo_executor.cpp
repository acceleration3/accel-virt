// An executor library for the qtest suites (src/device/abi/rvk_executor.h): it
// offers the Vulkan sub-API and runs a handful of commands, so the device's
// execution path can be driven on the real device without render-server.
//
//   0x10001 echo         the payload is the answer
//   0x10002 undecodable  RVK_UNDECODABLE
//   0x10003 lose device  RVK_DEVICE_LOST
//   0x10004 count        the answer is u64 commands this executor ran before
//                        it, then u32 1 when every call so far came from the
//                        thread that created it
//
// Anything else is RVK_UNKNOWN. With RVK_ECHO_FAIL_CREATE set, create fails.
#include "rvk_executor.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

namespace {

constexpr std::uint32_t echo = 0x10001u;
constexpr std::uint32_t undecodable = 0x10002u;
constexpr std::uint32_t lose_device = 0x10003u;
constexpr std::uint32_t count = 0x10004u;

struct Executor {
    pthread_t     thread;
    std::uint64_t executed;
    bool          one_thread;
};

std::uint32_t sub_apis(void *) { return RVK_EXECUTOR_SUB_API_VULKAN; }

RvkExecutor *create(void *, std::uint32_t, const std::uint8_t *)
{
    if (std::getenv("RVK_ECHO_FAIL_CREATE") != nullptr) return nullptr;
    auto *const executor = static_cast<Executor *>(std::calloc(1u, sizeof(Executor)));
    executor->thread = pthread_self();
    executor->one_thread = true;
    return static_cast<RvkExecutor *>(static_cast<void *>(executor));
}

void destroy(void *, RvkExecutor *handle) { std::free(handle); }

std::uint32_t execute(void *, RvkExecutor *handle, std::uint32_t id, const std::uint8_t *payload, std::size_t size,
                      std::uint8_t *reply, std::size_t capacity, std::size_t *reply_size)
{
    auto *const executor = static_cast<Executor *>(static_cast<void *>(handle));
    executor->one_thread = executor->one_thread && pthread_equal(executor->thread, pthread_self()) != 0;
    *reply_size = 0u;
    switch (id) {
    case echo:
        *reply_size = size;
        if (size <= capacity && size != 0u) std::memcpy(reply, payload, size);
        break;
    case undecodable: return RVK_UNDECODABLE;
    case lose_device: return RVK_DEVICE_LOST;
    case count: {
        const std::uint32_t flag = executor->one_thread ? 1u : 0u;
        *reply_size = 12u;
        if (capacity >= 12u) {
            std::memcpy(reply, &executor->executed, 8u);
            std::memcpy(reply + 8, &flag, 4u);
        }
        break;
    }
    default: return RVK_UNKNOWN;
    }
    executor->executed++;
    return RVK_EXECUTED;
}

const RvkExecutorOps table = {RVK_EXECUTOR_ABI_VERSION, nullptr, sub_apis, create, destroy, execute};

}  // namespace

extern "C" {

__attribute__((visibility("default")))
const RvkExecutorOps *rvk_executor_ops(void)
{ return &table; }

}
