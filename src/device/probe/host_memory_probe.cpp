#include "device/probe/host_memory_probe.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

HostMemoryProbe::HostMemoryProbe(Host &host) : host_(host), result_()
{
    AV_SCOPE("HostMemoryProbe::HostMemoryProbe");
}

uint8_t HostMemoryProbe::expected(uint64_t offset) const
{
    AV_SCOPE("HostMemoryProbe::expected");
    return static_cast<uint8_t>(offset * 31u + 7u);
}

Status HostMemoryProbe::matches(uint64_t offset)
{
    AV_SCOPE("HostMemoryProbe::matches");
    uint8_t      seen[sample_bytes] = {};
    const Status read = host_.read_hostmem(offset, seen, sample_bytes);
    if (read != Status::ok) return read;
    for (uint64_t index = 0u; index < sample_bytes; index++)
        if (seen[index] != expected(offset + index)) return Status::malformed;
    return Status::ok;
}

void HostMemoryProbe::run()
{
    AV_SCOPE("HostMemoryProbe::run");
    host_.require_main_loop("HostMemoryProbe::run");
    if (result_.ran()) return;
    if (host_.hostmem_size() < probe_bytes) {
        static_cast<void>(result_.record_failure(Status::not_supported, 0));
        return;
    }
    void   *pointer = nullptr;
    int32_t host_error = 0;
    void *const exported = host_.export_gpu_memory(probe_bytes, pointer, host_error);
    if (exported == nullptr) {
        static_cast<void>(result_.record_failure(Status::not_supported, host_error));
        return;
    }
    /* Armed before anything else can fail: a handle is the host's to take back
     * even when what came with it is useless. */
    ScopeExit release([&] { host_.release_gpu_memory(exported); });
    if (pointer == nullptr) {
        static_cast<void>(result_.record_failure(Status::not_supported, host_error));
        return;
    }
    const Status placed = host_.place_in_hostmem(exported, 0u);
    if (placed != Status::ok) {
        static_cast<void>(result_.record_failure(placed, 0));
        return;
    }
    ScopeExit remove([&] { host_.remove_from_hostmem(0u, probe_bytes); });
    uint8_t *const bytes = static_cast<uint8_t *>(pointer);
    for (uint64_t offset = 0u; offset < probe_bytes; offset++) bytes[offset] = expected(offset);
    Status carried = matches(0u);
    if (carried == Status::ok) carried = matches(probe_bytes - sample_bytes);
    static_cast<void>(carried == Status::ok ? result_.record_pass(probe_bytes)
                                            : result_.record_failure(carried, 0));
}

const ProbeResult &HostMemoryProbe::result() const
{
    AV_SCOPE("HostMemoryProbe::result");
    return result_;
}

}  // namespace av
