#include "guest/suite/adapter_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

AdapterScenario::AdapterScenario(Kmt &kmt, GuestReport &report, AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "adapter")
    {
        AV_SCOPE("AdapterScenario::AdapterScenario");
    }

void AdapterScenario::run()
{
    AV_SCOPE("AdapterScenario::run");
    if (steps_.check("find", search_.find_avgpu(0u)) != Status::ok) return;
    uint32_t     version = 0u;
    const Status queried = kmt_.query_driver_version(search_.adapter(), version);
    steps_.check("driver_version", queried == Status::ok && version != wddm_3_2 ? Status::not_supported : queried);
}

}
