#include "harness/executor/d3d12_session_loopback.hpp"

#include "harness/executor/fake_device.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

struct D3d12HostDeviceProbe {
    static void place(D3d12HostDevice &host, ID3D12Device *device)
    {
        host.runtime_ = reinterpret_cast<const D3dRuntime *>(&host);
        host.device_ = device;
        host.capabilities_.caps_.descriptor_sizes[0] = 32u;
        host.capabilities_.caps_.descriptor_sizes[1] = 32u;
        host.capabilities_.caps_.descriptor_sizes[2] = 32u;
        host.capabilities_.caps_.descriptor_sizes[3] = 32u;
    }
};

struct D3d12SessionProbe {
    static D3d12QueryHeaps &queries(D3d12Session &session) { return session.queries_; }
    static D3d12ObjectHolds &holds(D3d12Session &session) { return session.holds_; }
    static void hurry(D3d12Session &session, uint64_t nanoseconds) { session.hang_ns_ = nanoseconds; }
};

void D3d12SessionLoopback::place(FakeDevice &device)
{
    storage_.object().shut_down();
    placed_ = &device;
    D3d12HostDeviceProbe::place(host_, &device);
    storage_.object().initialize(imports_, &host_);
}

D3d12ObjectHolds &D3d12SessionLoopback::holds()
{
    return D3d12SessionProbe::holds(storage_.object());
}

void D3d12SessionLoopback::hurry(uint64_t nanoseconds)
{
    D3d12SessionProbe::hurry(storage_.object(), nanoseconds);
}

D3d12QueryHeaps &D3d12SessionLoopback::queries()
{
    return D3d12SessionProbe::queries(storage_.object());
}

D3d12SessionLoopback::D3d12SessionLoopback()
    : RoomedTransport(command_bytes, reply_bytes), blocks_(heap_), objects_(blocks_), lines_(sink_, "d3d12: "),
      imports_(heap_), host_(lines_), guest_memory_{}, command_{}, reply_{}, arena_block_{}, arena_(arena_block_, arena_bytes),
      decode_arena_(nullptr, 0u), refused_(0u), placed_(nullptr)
{
    static_cast<void>(guest_.bind(0u, guest_memory_, sizeof guest_memory_));
    static_cast<void>(storage_.construct(guest_, objects_, tenant_, fence_wake_, blocks_, lines_));
    storage_.object().initialize(imports_, nullptr);
}

D3d12SessionLoopback::~D3d12SessionLoopback()
{
    if (placed_ != nullptr)
        for (FakeFence &fence : placed_->fences) fence.completed = ~UINT64{0};
    storage_.object().shut_down();
    storage_.destroy();
}

D3d12Session &D3d12SessionLoopback::session()
{
    return storage_.object();
}

D3dObjectTable &D3d12SessionLoopback::objects()
{
    return objects_;
}

ImportTable &D3d12SessionLoopback::imports()
{
    return imports_;
}

uint32_t D3d12SessionLoopback::refused() const
{
    return refused_;
}

WireWriter D3d12SessionLoopback::request_writer()
{
    return WireWriter(command_, command_bytes);
}

bool D3d12SessionLoopback::run(uint32_t command_id, WireWriter &writer, bool awaited, std::size_t &answered)
{
    answered = 0u;
    if (writer.status() != Status::ok) return false;
    WireReader payload(command_, writer.offset());
    WireWriter reply(awaited ? reply_ : nullptr, awaited ? reply_bytes : 0u);
    arena_.reset();
    if (!storage_.object().run(command_id, payload, reply, arena_)) {
        refused_++;
        return false;
    }
    answered = reply.offset();
    return true;
}

TransportResult D3d12SessionLoopback::send_async(uint32_t command_id, WireWriter &writer)
{
    std::size_t answered = 0u;
    return run(command_id, writer, false, answered) ? TransportResult::ok : TransportResult::failed;
}

TransportResult D3d12SessionLoopback::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    std::size_t answered = 0u;
    if (!run(command_id, writer, true, answered)) return TransportResult::failed;
    reply = WireReader(reply_, answered);
    return TransportResult::ok;
}

void D3d12SessionLoopback::end_call()
{
}

WireArena &D3d12SessionLoopback::decode_arena()
{
    return decode_arena_;
}

ClientBlobArena &D3d12SessionLoopback::blobs()
{
    return blobs_;
}

}
