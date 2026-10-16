#include "guest/suite/isolation_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/context_fault.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av {

IsolationScenario::IsolationScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "isolation"), link_(kmt, search, steps_, SubApi::vulkan), staging_{}
    {
        AV_SCOPE("IsolationScenario::IsolationScenario");
    }

void IsolationScenario::run()
{
    AV_SCOPE("IsolationScenario::run");
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    if (steps_.check("healthy_before", exercise_healthy()) != Status::ok) return;
    {
        KmtTransport doomed(link_.connection(), SubApi::vulkan, ScenarioTransport::ring_order,
                            ScenarioTransport::reply_bytes, staging_, sizeof staging_);
        if (steps_.check("fault", fault(doomed)) != Status::ok) return;
        uint32_t answered = 0u;
        /* The transport refuses every send once a call has failed, so this
         * proves the fault stuck to the context rather than asking the host. */
        steps_.check_refused("faulted_send", doomed.nop(answered));
    }
    if (steps_.check("healthy_after", exercise_healthy()) != Status::ok) return;
    KmtTransport relinked(link_.connection(), SubApi::vulkan, ScenarioTransport::ring_order,
                          ScenarioTransport::reply_bytes, staging_, sizeof staging_);
    Status       relink = relinked.initialize();
    uint32_t     answered = 0u;
    if (relink == Status::ok) relink = relinked.nop(answered);
    static_cast<void>(steps_.check("relink", relink));
}

Status IsolationScenario::exercise_healthy()
{
    AV_SCOPE("IsolationScenario::exercise_healthy");
    KmtTransport &transport = link_.transport();
    uint32_t      answered = 0u;
    const Status  before = transport.nop(answered);
    if (before != Status::ok) return before;
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    uint64_t instance = 0u;
    if (vkproto::av_vkCreateInstance(transport, handles_, &info, nullptr, &instance) != VK_SUCCESS) return Status::invalid_state;
    vkproto::av_vkDestroyInstance(transport, handles_, reinterpret_cast<VkInstance>(instance), nullptr);
    return transport.nop(answered);
}

Status IsolationScenario::fault(KmtTransport &doomed)
{
    AV_SCOPE("IsolationScenario::fault");
    const Status opened = doomed.initialize();
    if (opened != Status::ok) return opened;
    return ContextFault(doomed).send();
}

}  // namespace av
