#include "harness/present/engine_side.hpp"

#include "device/control/control_request.hpp"
#include "device/control/present_engine_commands.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstring>
#include <time.h>

namespace av {

namespace {

constexpr std::uint8_t gpu[PresentFrames::uuid_bytes] = {4};

}

EngineSide::EngineSide(MappedVirtioFunction &mapped)
    : platform_(mapped.platform()), queue_(mapped.queue_device(0u)), host_(), interrupts_(host_),
      engine_(host_, interrupts_), plane_(), snapshots_(plane_, gpu)
{
    host_.translate_guest_with([](void *context, std::uint64_t address, std::uint64_t length) {
        return static_cast<MockPlatform *>(context)->translate_physical(address, static_cast<std::size_t>(length));
    }, &platform_);
    CHECK_STATUS(engine_.initialize(), Status::ok);
    platform_.on_stall(&EngineSide::stall, this);
}

EngineSide::~EngineSide()
{
    platform_.on_stall(nullptr, nullptr);
    engine_.detach();
}

void EngineSide::hold(bool held) { held_ = held; }
void EngineSide::refuse_page() { refuse_page_ = true; }
void EngineSide::accept_page()
{
    refuse_page_ = false;
    truncate_page_ = false;
}
void EngineSide::truncate_page() { truncate_page_ = true; }
void EngineSide::serve()
{
    static_cast<void>(queue_.answer_all(request_, sizeof request_, &EngineSide::answer, this));
}
std::uint32_t EngineSide::presents() const { return presents_; }
void          EngineSide::frame(const SurfacePoint &frame, const AvFrameView &view) { plane_.serve_frame(frame, view); }
void          EngineSide::texture(std::uint64_t surface, const AvFrameView &view) { plane_.serve_texture(surface, view); }
void          EngineSide::copy_between_surfaces(bool refuse) { plane_.copy_through(refuse); }
SurfacePair   EngineSide::copied() const { return plane_.copied(); }
std::uint32_t EngineSide::copied_regions() const { return plane_.copied_regions(); }

void EngineSide::stall(void *context)
{
    auto *const self = static_cast<EngineSide *>(context);
    if (self->held_) return;
    if (self->queue_.pending_type() != 0u) self->serve();
    const timespec pause = {0, 20000};
    nanosleep(&pause, nullptr);
}

std::uint32_t EngineSide::answer(void *context, const std::uint8_t *request, std::uint32_t length,
                                 std::uint8_t *answer, std::uint32_t room)
{
    auto *const   self = static_cast<EngineSide *>(context);
    std::uint32_t type = 0u;
    std::memcpy(&type, request, sizeof type);
    if (type == 0x02F7u) self->presents_++;
    GpuResponse refusal;
    refusal.set(GpuResponse::err_invalid_parameter);
    MockRequest    mock(request, length);
    ControlRequest control(self->host_, &mock);
    const bool     refused = type == 0x02F6u && self->refuse_page_;
    const bool     answered =
        !refused && PresentEngineCommands(self->host_, self->engine_, self->snapshots_).answer(control, type);
    const std::uint8_t *bytes = answered ? mock.response_bytes() : refusal.bytes();
    const std::size_t   size = answered ? mock.response_size() : refusal.length();
    const std::size_t   whole = size < room ? size : room;
    const std::size_t   given = type == 0x02F6u && self->truncate_page_ ? 2u : whole;
    std::memcpy(answer, bytes, given);
    return static_cast<std::uint32_t>(given);
}

}
