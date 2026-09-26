#include "guest/suite/guest_suite.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/suite/guest_report.hpp"

namespace av {

GuestSuite::GuestSuite(Kmt &kmt, GuestReport &report, uint32_t stress_seconds)
    : kmt_(kmt), report_(report), search_(kmt), adapter_(kmt, report, search_), allocation_(kmt, report, search_),
      mapping_(kmt, report, search_), submission_(kmt, report, search_),
      vsync_(kmt, report, search_), transport_(kmt, report, search_, SubApi::vulkan, "transport"), stress_(kmt, report, search_, stress_seconds),
      timeout_(kmt, report, search_), opengl_(kmt, report, search_, SubApi::opengl, "opengl"),
      vulkan_(kmt, report, search_), opencl_(kmt, report, search_), host_import_(kmt, report, search_),
      isolation_(kmt, report, search_), submit_(kmt, report, search_)
      {
          AV_SCOPE("GuestSuite::GuestSuite");
      }

uint32_t GuestSuite::run_timeout()
{
    AV_SCOPE("GuestSuite::run_timeout");
    adapter_.run();
    timeout_.run();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

uint32_t GuestSuite::run_rvk()
{
    AV_SCOPE("GuestSuite::run_rvk");
    adapter_.run();
    transport_.run();
    /* What user-mode submission bought, on the boot it ran on (feature 049,
     * US3): before anything that faults a context, since it measures a context
     * of its own that must be healthy. */
    submit_.run();
    vulkan_.run();
    opencl_.run();
    host_import_.run();
    opengl_.run();
    /* Last: the context it faults is its own, and a scenario after it would
     * be judging the recovery rather than its own work. */
    isolation_.run();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

/* How long one answered command takes from the guest's side, alone: what
 * every synchronous call any ICD makes pays on top of its work. */
uint32_t GuestSuite::run_round_trip()
{
    AV_SCOPE("GuestSuite::run_round_trip");
    adapter_.run();
    /* Which adapter answered: with two rvkgpu adapters, each is its own device
     * and worker on the host. */
    uint64_t luid = 0u;
    static_cast<void>(kmt_.adapter_luid(search_.adapter(), luid));
    report_.measure("trip", "adapter_luid", Status::ok, static_cast<uint32_t>(luid));
    submit_.round_trips();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

uint32_t GuestSuite::run()
{
    AV_SCOPE("GuestSuite::run");
    adapter_.run();
    allocation_.run();
    mapping_.run();
    submission_.run();
    vsync_.run();
    /* After every scenario of the driver's own calls: a transport that fails
     * rolls back before its step is reported, which would stand between a
     * refused call and the NTSTATUS those scenarios report for it. */
    transport_.run();
    stress_.run();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

}  // namespace av
