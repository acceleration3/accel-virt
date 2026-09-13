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
//
// Two controls make a real timeout happen without rebuilding anything
// (feature 040, FR-006), read from the environment at load:
//
//   RVK_ECHO_STALL_MS      every command sleeps this long before answering
//   RVK_ECHO_FAULT_AFTER   the Nth command on each executor answers
//                          RVK_DEVICE_LOST, whatever it asked for
//   RVK_ECHO_ONLY_NTH      both apply only to the Nth executor created, so one
//                          context can stall or fault while another completes
//
// A stall longer than TdrDelay is what dxgkrnl times out on; the guest suite's
// tdr scenario sets them for the run that needs them.
#include "rvk_executor.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>

namespace {

constexpr std::uint32_t echo = 0x10001u;
constexpr std::uint32_t undecodable = 0x10002u;
constexpr std::uint32_t lose_device = 0x10003u;
constexpr std::uint32_t count = 0x10004u;

struct Executor {
    pthread_t     thread;
    std::uint64_t executed;
    /* Which executor this is, counted from one in the order they were created:
     * what RVK_ECHO_ONLY_NTH names. */
    std::uint64_t ordinal;
    bool          one_thread;
};

std::uint64_t created = 0u;

/* A number the environment names, or 0. */
std::uint64_t from_environment(const char *name)
{
    const char *const value = std::getenv(name);
    if (value == nullptr) return 0u;
    std::uint64_t number = 0u;
    for (const char *digit = value; *digit >= '0' && *digit <= '9'; digit++)
        number = number * 10u + static_cast<std::uint64_t>(*digit - '0');
    return number;
}

void stall(std::uint64_t milliseconds)
{
    if (milliseconds == 0u) return;
    const timespec sleep = {static_cast<time_t>(milliseconds / 1000u),
                            static_cast<long>((milliseconds % 1000u) * 1000000u)};
    static_cast<void>(nanosleep(&sleep, nullptr));
}

std::uint32_t sub_apis(void *) { return RVK_EXECUTOR_SUB_API_VULKAN; }

RvkExecutor *create(void *, std::uint32_t, const std::uint8_t *)
{
    if (std::getenv("RVK_ECHO_FAIL_CREATE") != nullptr) return nullptr;
    auto *const executor = static_cast<Executor *>(std::calloc(1u, sizeof(Executor)));
    executor->thread = pthread_self();
    executor->ordinal = ++created;
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
    /* The host taking too long, and the host losing the device: what a guest
     * cannot ask for and a test has to be able to produce. Either is aimed at
     * one executor where the run named one, so that what happens to a context
     * can be told apart from what happens to the device. */
    const std::uint64_t only = from_environment("RVK_ECHO_ONLY_NTH");
    const bool steered = only == 0u || only == executor->ordinal;
    if (steered) stall(from_environment("RVK_ECHO_STALL_MS"));
    const std::uint64_t fault_after = from_environment("RVK_ECHO_FAULT_AFTER");
    if (steered && fault_after != 0u && executor->executed + 1u >= fault_after) return RVK_DEVICE_LOST;
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
