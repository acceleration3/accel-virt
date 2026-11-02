#include "icd/opengl/gl_error_query.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include "gl_protocol_ids.h"

namespace av::glicd {

GlErrorQuery::GlErrorQuery(ClientTransport &transport) : transport_(transport)
{
    AV_SCOPE("GlErrorQuery::GlErrorQuery");
}

uint32_t GlErrorQuery::ask()
{
    AV_SCOPE("GlErrorQuery::ask");
    WireWriter          request = transport_.request_writer();
    const SyncCallScope call{transport_};
    WireReader          reply{nullptr, 0u};
    if (transport_.send_sync(glproto::kCmd_glGetError, request, reply) != TransportResult::ok) return 0u;
    uint64_t error = 0u;
    return reply.read_varint(error) ? static_cast<uint32_t>(error) : 0u;
}

}
