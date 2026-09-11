#include "guest/suite/guest_suite.hpp"

#include "guest/kmt/kmt.hpp"
#include "guest/suite/guest_report.hpp"

namespace av {

GuestSuite::GuestSuite(Kmt &kmt, GuestReport &report)
    : kmt_(kmt), report_(report), search_(kmt), adapter_(kmt, report, search_), allocation_(kmt, report, search_),
      mapping_(kmt, report, search_), submission_(kmt, report, search_) {}

uint32_t GuestSuite::run()
{
    adapter_.run();
    allocation_.run();
    mapping_.run();
    submission_.run();
    report_.check("suite", "close_adapter", search_.close(), kmt_.last_code());
    report_.finish();
    return report_.failed();
}

}  // namespace av
