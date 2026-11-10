#include "icd/opengl/debug_delivery_transport.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/gl_debug_messages.hpp"

namespace av::glicd {

DebugDeliveryTransport::DebugDeliveryTransport(GlDebugMessages &messages) : messages_(messages), inner_(nullptr)
{
    AV_SCOPE("DebugDeliveryTransport::DebugDeliveryTransport");
}

void DebugDeliveryTransport::bind(ClientTransport &inner)
{
    AV_SCOPE("DebugDeliveryTransport::bind");
    inner_ = &inner;
}

WireWriter DebugDeliveryTransport::request_writer()
{
    AV_SCOPE("DebugDeliveryTransport::request_writer");
    return inner_->request_writer();
}

WireWriter DebugDeliveryTransport::request_writer_with_room(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("DebugDeliveryTransport::request_writer_with_room");
    return inner_->request_writer_with_room(sizes, count);
}

TransportResult DebugDeliveryTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("DebugDeliveryTransport::send_async");
    const TransportResult sent = inner_->send_async(command_id, writer);
    deliver();
    return sent;
}

TransportResult DebugDeliveryTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("DebugDeliveryTransport::send_sync");
    return inner_->send_sync(command_id, writer, reply);
}

void DebugDeliveryTransport::end_call()
{
    AV_SCOPE("DebugDeliveryTransport::end_call");
    inner_->end_call();
    deliver();
}

WireArena &DebugDeliveryTransport::decode_arena()
{
    AV_SCOPE("DebugDeliveryTransport::decode_arena");
    return inner_->decode_arena();
}

ClientBlobArena &DebugDeliveryTransport::blobs()
{
    AV_SCOPE("DebugDeliveryTransport::blobs");
    return inner_->blobs();
}

bool DebugDeliveryTransport::make_room_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("DebugDeliveryTransport::make_room_for_blobs");
    return inner_->make_room_for_blobs(sizes, count);
}

ClientBlobLease DebugDeliveryTransport::open_region(uint64_t bytes)
{
    AV_SCOPE("DebugDeliveryTransport::open_region");
    return inner_->open_region(bytes);
}

void DebugDeliveryTransport::close_region(const ClientBlobLease &region)
{
    AV_SCOPE("DebugDeliveryTransport::close_region");
    inner_->close_region(region);
}

const TransferRoom &DebugDeliveryTransport::room() const
{
    AV_SCOPE("DebugDeliveryTransport::room");
    return inner_->room();
}

void DebugDeliveryTransport::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("DebugDeliveryTransport::note");
    inner_->note(counter, amount);
}

void DebugDeliveryTransport::deliver()
{
    AV_SCOPE("DebugDeliveryTransport::deliver");
    messages_.deliver(*inner_);
}

bool DebugDeliveryTransport::invariants_hold() const
{
    AV_SCOPE("DebugDeliveryTransport::invariants_hold");
    return inner_ != nullptr;
}

}
