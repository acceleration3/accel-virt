#include "icd/d3d/d3d_device_selection.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/d3d_command_stream.hpp"

namespace av::d3d {

D3dDeviceSelection::D3dDeviceSelection(D3dCommandStream &stream, uint32_t device) : stream_(stream), device_(device)
{
    AV_SCOPE("D3dDeviceSelection::D3dDeviceSelection");
}

WireWriter D3dDeviceSelection::request_writer()
{
    AV_SCOPE("D3dDeviceSelection::request_writer");
    static_cast<void>(stream_.speak_as(device_));
    return stream_.request_writer();
}

TransportResult D3dDeviceSelection::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dDeviceSelection::send_async");
    return stream_.send_async(command_id, writer);
}

TransportResult D3dDeviceSelection::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("D3dDeviceSelection::send_sync");
    return stream_.send_sync(command_id, writer, reply);
}

void D3dDeviceSelection::end_call()
{
    AV_SCOPE("D3dDeviceSelection::end_call");
    stream_.end_call();
}

WireArena &D3dDeviceSelection::decode_arena()
{
    AV_SCOPE("D3dDeviceSelection::decode_arena");
    return stream_.decode_arena();
}

ClientBlobArena &D3dDeviceSelection::blobs()
{
    AV_SCOPE("D3dDeviceSelection::blobs");
    return stream_.blobs();
}

bool D3dDeviceSelection::make_room_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("D3dDeviceSelection::make_room_for_blobs");
    return stream_.make_room_for_blobs(sizes, count);
}

ClientBlobLease D3dDeviceSelection::open_region(uint64_t bytes)
{
    AV_SCOPE("D3dDeviceSelection::open_region");
    return stream_.open_region(bytes);
}

void D3dDeviceSelection::close_region(const ClientBlobLease &region)
{
    AV_SCOPE("D3dDeviceSelection::close_region");
    stream_.close_region(region);
}

const TransferRoom &D3dDeviceSelection::room() const
{
    AV_SCOPE("D3dDeviceSelection::room");
    return stream_.room();
}

void D3dDeviceSelection::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("D3dDeviceSelection::note");
    stream_.note(counter, amount);
}

}
