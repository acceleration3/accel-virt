#include "device/probe/guest_import_probe.hpp"

#include "core/support/scope_exit.hpp"
#include "device/host/host.hpp"

namespace av {

GuestImportProbe::GuestImportProbe(Host &host) : host_(host), result_() {}

uint8_t GuestImportProbe::expected(uint64_t offset) const
{
    return static_cast<uint8_t>(offset * 17u + 3u);
}

Status GuestImportProbe::matches(void *imported, uint64_t offset)
{
    uint8_t      seen[sample_bytes] = {};
    const Status read = host_.read_imported_memory(imported, offset, seen, sample_bytes);
    if (read != Status::ok) return read;
    for (uint64_t index = 0u; index < sample_bytes; index++)
        if (seen[index] != expected(offset + index)) return Status::malformed;
    return Status::ok;
}

uint64_t GuestImportProbe::copy(uint8_t *to, const uint8_t *from)
{
    const uint64_t started = host_.monotonic_nanoseconds();
    for (uint64_t offset = 0u; offset < probe_bytes; offset++) to[offset] = from[offset];
    const uint64_t elapsed = host_.monotonic_nanoseconds() - started;
    return elapsed == 0u ? 0u : probe_bytes * nanoseconds_per_second / elapsed;
}

void GuestImportProbe::run(uint64_t guest_address)
{
    host_.require_main_loop("GuestImportProbe::run");
    if (result_.ran()) return;
    uint64_t length = probe_bytes;
    bool     direct = false;
    void *const window = host_.map_guest(guest_address, length, direct);
    if (window == nullptr || length < probe_bytes) {
        if (window != nullptr) host_.unmap_guest(window, length);
        static_cast<void>(result_.record_failure(Status::not_found, 0));
        return;
    }
    ScopeExit unmap([&] { host_.unmap_guest(window, length); });
    /* A copy the guest cannot see is not the guest's memory. */
    if (!direct) {
        static_cast<void>(result_.record_failure(Status::not_supported, 0));
        return;
    }
    /* Somewhere to keep the guest's own bytes while the pattern is in their
     * place; copying them there is the fallback, and its rate is the answer a
     * refusal carries. */
    void       *kept = nullptr;
    void *const block = host_.create_host_memory(probe_bytes, kept);
    if (block == nullptr) {
        static_cast<void>(result_.record_failure(Status::no_memory, 0));
        return;
    }
    /* Armed before the pointer is judged: a handle is the host's to take back
     * even when nothing came with it. */
    ScopeExit destroy([&] { host_.destroy_host_memory(block); });
    if (kept == nullptr) {
        static_cast<void>(result_.record_failure(Status::no_memory, 0));
        return;
    }
    uint8_t *const bytes = static_cast<uint8_t *>(window);
    const uint64_t rate = copy(static_cast<uint8_t *>(kept), bytes);
    ScopeExit restore([&] {
        for (uint64_t offset = 0u; offset < probe_bytes; offset++)
            bytes[offset] = static_cast<const uint8_t *>(kept)[offset];
    });
    for (uint64_t offset = 0u; offset < probe_bytes; offset++) bytes[offset] = expected(offset);
    int32_t     host_error = 0;
    void *const imported = host_.import_guest_memory(window, probe_bytes, host_error);
    if (imported == nullptr) {
        static_cast<void>(result_.record_failure(Status::not_supported, host_error));
        result_.measure(rate);
        return;
    }
    ScopeExit release([&] { host_.release_guest_memory(imported); });
    Status carried = matches(imported, 0u);
    if (carried == Status::ok) carried = matches(imported, probe_bytes - sample_bytes);
    if (carried == Status::ok) {
        static_cast<void>(result_.record_pass(probe_bytes));
        return;
    }
    static_cast<void>(result_.record_failure(carried, 0));
    result_.measure(rate);
}

const ProbeResult &GuestImportProbe::result() const
{
    return result_;
}

}  // namespace av
