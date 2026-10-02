#include "guest/transport/context_fault.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/client/sync_call_scope.hpp"

#include <vulkan/vulkan_core.h>

namespace av {

ContextFault::ContextFault(ClientTransport &transport)
    : transport_(transport), command_(vkproto::kCmd_vkCreateInstance), prefix_bytes_(sizeof(uint32_t))
{
    AV_SCOPE("ContextFault::ContextFault");
}

ContextFault::ContextFault(ClientTransport &transport, uint32_t command)
    : transport_(transport), command_(command), prefix_bytes_(0u)
{
    AV_SCOPE("ContextFault::ContextFault");
}

Status ContextFault::send()
{
    AV_SCOPE("ContextFault::send");
    WireWriter request = transport_.request_writer();
    if (prefix_bytes_ != 0u) request.write_u32(static_cast<uint32_t>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO));
    WireReader          answer(nullptr, 0u);
    const SyncCallScope call(transport_);
    const TransportResult sent = transport_.send_sync(command_, request, answer);
    return sent == TransportResult::failed ? Status::ok : Status::conflict;
}

}  // namespace av
