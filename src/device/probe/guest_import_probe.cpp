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

uint64_t GuestImportProbe::copy_rate(const void *window)
{
    void       *destination = nullptr;
    void *const block = host_.create_host_memory(probe_bytes, destination);
    if (block == nullptr || destination == nullptr) return 0u;
    ScopeExit destroy([&] { host_.destroy_host_memory(block); });
    const uint8_t *const from = static_cast<const uint8_t *>(window);
    uint8_t *const       to = static_cast<uint8_t *>(destination);
    const uint64_t       started = host_.monotonic_nanoseconds();
    for (uint64_t offset = 0u; offset < probe_bytes; offset++) to[offset] = from[offset];
    const uint64_t elapsed = host_.monotonic_nanoseconds() - started;
    if (elapsed == 0u) return 0u;
    return probe_bytes * nanoseconds_per_second / elapsed;
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
    uint8_t *const bytes = static_cast<uint8_t *>(window);
    for (uint64_t offset = 0u; offset < probe_bytes; offset++) bytes[offset] = expected(offset);
    int32_t     host_error = 0;
    void *const imported = host_.import_guest_memory(window, probe_bytes, host_error);
    if (imported == nullptr) {
        static_cast<void>(result_.record_failure(Status::not_supported, host_error));
        result_.measure(copy_rate(window));
        return;
    }
    ScopeExit release([&] { host_.release_guest_memory(imported); });
    Status carried = matches(imported, 0u);
    if (carried == Status::ok) carried = matches(imported, probe_bytes - sample_bytes);
    static_cast<void>(carried == Status::ok ? result_.record_pass(probe_bytes)
                                            : result_.record_failure(carried, 0));
}

const ProbeResult &GuestImportProbe::result() const
{
    return result_;
}

}  // namespace av
