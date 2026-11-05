#include "proto/client/client_transport.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WireWriter ClientTransport::request_writer_with_room(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("ClientTransport::request_writer_with_room");
    static_cast<void>(make_room_for_blobs(sizes, count));
    return request_writer();
}

}
