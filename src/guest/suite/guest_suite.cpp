#include "guest/suite/guest_suite.hpp"

#include "guest/kmt/kmt.hpp"
#include "guest/suite/guest_report.hpp"

namespace av {

GuestSuite::GuestSuite(Kmt &kmt, GuestReport &report, uint32_t stress_seconds)
    : kmt_(kmt), report_(report), search_(kmt), adapter_(kmt, report, search_), allocation_(kmt, report, search_),
      mapping_(kmt, report, search_), submission_(kmt, report, search_),
      host_visible_(kmt, report, search_),
      vsync_(kmt, report, search_), stress_(kmt, report, search_, stress_seconds),
      timeout_(kmt, report, search_) {}

uint32_t GuestSuite::run_timeout()
{
    adapter_.run();
    timeout_.run();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

uint32_t GuestSuite::run()
{
    adapter_.run();
    allocation_.run();
    mapping_.run();
    submission_.run();
    host_visible_.run();
    vsync_.run();
    stress_.run();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

}  // namespace av
