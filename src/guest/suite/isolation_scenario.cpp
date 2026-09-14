#include "guest/suite/isolation_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

IsolationScenario::IsolationScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : steps_(kmt, report, "isolation"), link_(kmt, search, steps_, SubApi::vulkan) {}

void IsolationScenario::run()
{
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    if (steps_.check("healthy_before", exercise_healthy()) != Status::ok) return;
    {
        KmtTransport doomed(link_.connection(), SubApi::vulkan, ScenarioTransport::ring_order);
        if (steps_.check("fault", fault(doomed)) != Status::ok) return;
        uint32_t answered = 0u;
        /* The transport refuses every send once a wait has failed, so this
         * proves the fault stuck to the context rather than asking the host. */
        steps_.check_refused("faulted_send", doomed.nop(answered));
    }
    if (steps_.check("healthy_after", exercise_healthy()) != Status::ok) return;
    KmtTransport relinked(link_.connection(), SubApi::vulkan, ScenarioTransport::ring_order);
    Status       relink = relinked.initialize();
    uint32_t     answered = 0u;
    if (relink == Status::ok) relink = relinked.nop(answered);
    static_cast<void>(steps_.check("relink", relink));
}

Status IsolationScenario::exercise_healthy()
{
    KmtTransport &transport = link_.transport();
    uint32_t      answered = 0u;
    const Status  before = transport.nop(answered);
    if (before != Status::ok) return before;
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    uint64_t instance = 0u;
    if (vkproto::rvk_vkCreateInstance(transport, &info, nullptr, &instance) != VK_SUCCESS) return Status::invalid_state;
    vkproto::rvk_vkDestroyInstance(transport, reinterpret_cast<VkInstance>(instance), nullptr);
    return transport.nop(answered);
}

/* The structure type and nothing after it: a create-info truncated at its
 * first field boundary, which no decoder can take for an instance's (research
 * section 6), and which reaches no host ICD. */
Status IsolationScenario::fault(KmtTransport &doomed)
{
    const Status opened = doomed.initialize();
    if (opened != Status::ok) return opened;
    WireWriter request = doomed.request_writer();
    request.write_u32(static_cast<uint32_t>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO));
    WireReader            answer(nullptr, 0u);
    const TransportResult sent = doomed.send_sync(vkproto::kCmd_vkCreateInstance, request, answer);
    return sent == TransportResult::failed ? Status::ok : Status::conflict;
}

}  // namespace av
