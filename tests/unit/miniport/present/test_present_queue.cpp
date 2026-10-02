/* Presents queued to the device's present engine: the requests the queue
 * sends are taken by the device core's own engine over the guest's memory,
 * on its own thread, so a pixel that arrives and a page that counts it prove
 * the whole path - the registration, the fixed part under its ticket, both
 * page lists and the regions in a payload, and the completion. A present is
 * sent without waiting for anything; one whose payload is still outstanding
 * waits for it, and one the device never finishes, or never takes, is given
 * up on. */
#include "miniport/present/present_queue.hpp"
#include "miniport/present/present_surface.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "device/control/control_request.hpp"
#include "device/control/present_engine_commands.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "device/present/present_frames.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/commands/present_region.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/resources/surface_point.hpp"
#include "miniport/present/present_ticket_record.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <time.h>

using av::ControlRequest;
using av::FakeVirtqueueDevice;
using av::FenceInterrupts;
using av::GpuResponse;
using av::GuestPageList;
using av::MappedVirtioFunction;
using av::MockHost;
using av::MockPlatform;
using av::MockRequest;
using av::PresentEngine;
using av::PresentEngineCommands;
using av::PresentQueue;
using av::PresentRegion;
using av::PresentSurface;
using av::PresentTicketRecord;
using av::Status;
using av::SurfaceLayout;
using av::VirtioGpuDevice;

namespace {

constexpr std::uint32_t side = 16u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
constexpr std::uint32_t blue_first = 21u;   /* D3DDDIFMT_A8R8G8B8 */
constexpr std::uint32_t red_first = 32u;    /* D3DDDIFMT_A8B8G8R8 */
constexpr std::uint32_t epoch = 3u;
constexpr std::uint8_t  gpu[av::PresentFrames::uuid_bytes] = {4};

/* The device's side: its present engine and the commands that feed it,
 * serving the control queue when the test says or when the driver stalls. */
class EngineSide {
public:
    explicit EngineSide(MappedVirtioFunction &mapped)
        : platform_(mapped.platform()), queue_(mapped.queue_device(0u)), host_(), interrupts_(host_),
          engine_(host_, interrupts_), plane_(), snapshots_(plane_, gpu)
    {
        host_.translate_guest_with([](void *context, std::uint64_t address, std::uint64_t length) {
            return static_cast<MockPlatform *>(context)->translate_physical(address, static_cast<std::size_t>(length));
        }, &platform_);
        CHECK_STATUS(engine_.initialize(), Status::ok);
        platform_.on_stall(&EngineSide::stall, this);
    }
    ~EngineSide()
    {
        platform_.on_stall(nullptr, nullptr);
        engine_.detach();
    }
    EngineSide(const EngineSide &) = delete;
    EngineSide &operator=(const EngineSide &) = delete;

    /* A device that takes nothing more until serving again. */
    void          hold(bool held) { held_ = held; }
    /* A device that refuses the page. */
    void          refuse_page() { refuse_page_ = true; }
    void          serve() { static_cast<void>(queue_.answer_all(request_, sizeof request_, &EngineSide::answer, this)); }
    std::uint32_t presents() const { return presents_; }
    /* The host's snapshot of an application's buffer, under `key`. */
    void          frame(const av::SurfacePoint &frame, const AvFrameView &view) { plane_.serve_frame(frame, view); }

private:
    static void stall(void *context)
    {
        auto *const self = static_cast<EngineSide *>(context);
        if (self->held_) return;
        if (self->queue_.pending_type() != 0u) self->serve();
        const timespec pause = {0, 20000};
        nanosleep(&pause, nullptr);   /* the engine's thread moves meanwhile */
    }

    static std::uint32_t answer(void *context, const std::uint8_t *request, std::uint32_t length,
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
        const std::size_t   given = size < room ? size : room;
        std::memcpy(answer, bytes, given);
        return static_cast<std::uint32_t>(given);
    }

    MockPlatform       &platform_;
    FakeVirtqueueDevice queue_;
    MockHost            host_;
    FenceInterrupts     interrupts_;
    PresentEngine       engine_;
    av::RecordingContextPlane plane_;
    av::PresentFrames      snapshots_;
    bool                held_ = false;
    bool                refuse_page_ = false;
    std::uint32_t       presents_ = 0u;
    std::uint8_t        request_[0x9000] = {};
};

/* One 16x16 surface in a page of the guest's. */
class Surface {
public:
    explicit Surface(MockPlatform &platform) : platform_(platform)
    {
        block_ = static_cast<std::uint8_t *>(platform_.allocate_contiguous(GuestPageList::page_bytes));
        CHECK(block_ != nullptr);
        std::memset(block_, 0, GuestPageList::page_bytes);
        frame_ = platform_.physical_address(block_);
    }
    ~Surface() { platform_.release_contiguous(block_, GuestPageList::page_bytes); }
    Surface(const Surface &) = delete;
    Surface &operator=(const Surface &) = delete;

    PresentSurface present(std::uint32_t format) const
    {
        return PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList(&frame_, 1u));
    }
    void number()
    {
        for (std::uint32_t y = 0u; y < side; y++)
            for (std::uint32_t x = 0u; x < side; x++) {
                const std::uint32_t value = 0x80000000u | (y << 8u) | x;
                std::memcpy(block_ + y * pitch + x * 4u, &value, sizeof value);
            }
    }
    std::uint32_t at(std::uint32_t x, std::uint32_t y) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, block_ + y * pitch + x * 4u, sizeof value);
        return value;
    }

private:
    MockPlatform &platform_;
    std::uint8_t *block_ = nullptr;
    std::uint64_t frame_ = 0u;
};

class Fixture {
public:
    Fixture()
    {
        CHECK_STATUS(mapped_.map(), Status::ok);
        device_.emplace(mapped_.platform(), mapped_.mappings(), mapped_.layout());
        CHECK_STATUS(device_->start(3u), Status::ok);
    }

    MappedVirtioFunction &mapped() { return mapped_; }
    MockPlatform         &platform() { return mapped_.platform(); }
    VirtioGpuDevice      &device() { return *device_; }

private:
    MappedVirtioFunction           mapped_;
    std::optional<VirtioGpuDevice> device_;
};

std::uint32_t swapped(std::uint32_t pixel)
{
    return (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
}

/* Waits up to two seconds for the queue to see `ticket` finished. */
bool finishes(PresentQueue &queue, std::uint64_t ticket)
{
    const timespec pause = {0, 100000};
    for (std::uint32_t waited = 0u; waited < 20000u; waited++) {
        if (queue.completed() >= ticket) return true;
        nanosleep(&pause, nullptr);
    }
    return false;
}

}  // namespace

TEST(present_queue, a_queued_blt_is_sent_unwaited_and_moved_by_the_device_s_thread)
{ Fixture      fixture;
  EngineSide   device(fixture.mapped());
  PresentQueue queue(fixture.platform(), fixture.device(), epoch);
  CHECK(queue.invariants_hold());
  CHECK_STATUS(queue.start(), Status::ok);
  CHECK(queue.invariants_hold());
  CHECK_EQ(queue.completed(), 0u);
  Surface source(fixture.platform());
  Surface destination(fixture.platform());
  source.number();
  const PresentSurface from = source.present(red_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  regions[] = {PresentRegion(1u, 2u, 3u, 4u, 5u, 6u), PresentRegion(0u, 15u, 0u, 0u, 1u, 1u)};
  device.hold(true);
  std::uint64_t ticket = 0u;
  CHECK_STATUS(queue.queue(from.blt_into(into, 2u), &from, into, regions, ticket), Status::ok);
  CHECK_EQ(ticket, 1u);
  CHECK_EQ(device.presents(), 0u);   /* sent, and not waited for */
  CHECK_EQ(destination.at(3u, 4u), 0u);
  device.serve();
  CHECK(finishes(queue, 1u));
  CHECK_EQ(destination.at(3u, 4u), swapped(source.at(1u, 2u)));
  CHECK_EQ(destination.at(7u, 9u), swapped(source.at(5u, 7u)));
  CHECK_EQ(destination.at(0u, 0u), swapped(source.at(0u, 15u)));
  CHECK_EQ(destination.at(8u, 4u), 0u);
  const PresentRegion row(0u, 0u, 0u, 12u, side, 1u);
  CHECK_STATUS(queue.queue(into.fill_with(0xFF00FF00u, 1u), nullptr, into, &row, ticket), Status::ok);
  CHECK_EQ(ticket, 2u);
  device.serve();
  CHECK(finishes(queue, 2u));
  CHECK_EQ(destination.at(side - 1u, 12u), 0xFF00FF00u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

/* A packet waits only on a present this queue queued: its epoch, a ticket
 * sent. Private data a process forged names nothing it can park the node on. */
TEST(present_queue, it_holds_only_the_tickets_it_queued_under_its_epoch)
{ Fixture      fixture;
  EngineSide   device(fixture.mapped());
  PresentQueue queue(fixture.platform(), fixture.device(), epoch);
  CHECK_EQ(queue.epoch(), epoch);
  CHECK_STATUS(queue.start(), Status::ok);
  CHECK(!queue.holds(PresentTicketRecord(epoch, 1u)));   /* nothing queued yet */
  Surface              destination(fixture.platform());
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 0u, 0u, 1u, 1u);
  std::uint64_t        ticket = 0u;
  CHECK_STATUS(queue.queue(into.fill_with(7u, 1u), nullptr, into, &region, ticket), Status::ok);
  CHECK(queue.holds(PresentTicketRecord(epoch, 1u)));
  CHECK(!queue.holds(PresentTicketRecord(epoch, 2u)));
  CHECK(!queue.holds(PresentTicketRecord(epoch + 1u, 1u)));
  CHECK(!queue.holds(PresentTicketRecord())); }

/* The fifth present reuses the first's payload, so it waits for the first
 * to finish - and only while the device takes nothing does it give up. */
TEST(present_queue, a_present_waits_for_its_payload_and_gives_up_on_a_device_that_never_finishes)
{ Fixture      fixture;
  EngineSide   device(fixture.mapped());
  PresentQueue queue(fixture.platform(), fixture.device(), epoch);
  CHECK_STATUS(queue.start(), Status::ok);
  Surface              destination(fixture.platform());
  const PresentSurface into = destination.present(blue_first);
  device.hold(true);
  std::uint64_t ticket = 0u;
  for (std::uint32_t row = 0u; row < PresentQueue::slots; row++) {
      const PresentRegion region(0u, 0u, 0u, row, side, 1u);
      CHECK_STATUS(queue.queue(into.fill_with(row + 1u, 1u), nullptr, into, &region, ticket), Status::ok);
  }
  const PresentRegion region(0u, 0u, 0u, 9u, side, 1u);
  CHECK_STATUS(queue.queue(into.fill_with(9u, 1u), nullptr, into, &region, ticket), Status::not_found);
  CHECK_EQ(ticket, 0u);
  device.hold(false);   /* the stall serves the queue now */
  CHECK_STATUS(queue.queue(into.fill_with(9u, 1u), nullptr, into, &region, ticket), Status::ok);
  CHECK_EQ(ticket, PresentQueue::slots + 1u);
  device.serve();
  CHECK(finishes(queue, ticket));
  CHECK_EQ(destination.at(0u, 0u), 1u);
  CHECK_EQ(destination.at(0u, 9u), 9u); }

TEST(present_queue, a_present_too_large_for_a_payload_or_before_start_is_not_queued)
{ Fixture      fixture;
  EngineSide   device(fixture.mapped());
  PresentQueue queue(fixture.platform(), fixture.device(), epoch);
  Surface              destination(fixture.platform());
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 0u, 0u, 1u, 1u);
  std::uint64_t        ticket = 7u;
  CHECK_STATUS(queue.queue(into.fill_with(1u, 1u), nullptr, into, &region, ticket), Status::invalid_state);
  CHECK_EQ(ticket, 0u);
  CHECK_STATUS(queue.start(), Status::ok);
  CHECK_STATUS(queue.start(), Status::invalid_state);
  /* Every page its own run, and more of them than a payload holds. */
  constexpr std::uint32_t pages = 20000u;
  static std::uint64_t    frames[pages];
  for (std::uint32_t page = 0u; page < pages; page++) frames[page] = 0x100000u + 2u * page;
  const PresentSurface scattered(SurfaceLayout(side, pages, blue_first, 4096u), GuestPageList(frames, pages));
  CHECK_STATUS(queue.queue(scattered.fill_with(1u, 1u), nullptr, scattered, &region, ticket), Status::too_large);
  CHECK_EQ(device.presents(), 0u);
  CHECK_STATUS(queue.queue(into.fill_with(1u, 1u), nullptr, into, &region, ticket), Status::ok);
  CHECK_EQ(ticket, 1u); }   /* the refused one took no ticket */

TEST(present_queue, a_device_that_refuses_the_page_or_a_host_without_memory_starts_nothing)
{ Fixture fixture;
  EngineSide device(fixture.mapped());
  {
      PresentQueue queue(fixture.platform(), fixture.device(), epoch);
      device.refuse_page();
      CHECK_STATUS(queue.start(), Status::not_supported);
      CHECK(queue.invariants_hold());
  }
  {
      PresentQueue queue(fixture.platform(), fixture.device(), epoch);
      fixture.platform().fail_allocation_at(3u);   /* the second payload */
      CHECK_STATUS(queue.start(), Status::no_memory);
      CHECK(queue.invariants_hold());
  } }

/* Every control slot in flight and none answered: the present harvests what
 * the device answers while it waits, and gives up on a device that answers
 * nothing. */
TEST(present_queue, a_present_with_no_control_slot_waits_for_one)
{ Fixture      fixture;
  EngineSide   device(fixture.mapped());
  PresentQueue queue(fixture.platform(), fixture.device(), epoch);
  CHECK_STATUS(queue.start(), Status::ok);
  device.hold(true);
  const std::uint8_t display_info[24] = {0x00u, 0x01u};
  for (std::uint32_t slot = 0u; slot < 16u; slot++)
      CHECK_STATUS(fixture.device().send_control(display_info, sizeof display_info), Status::ok);
  Surface              destination(fixture.platform());
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 0u, 0u, 1u, 1u);
  std::uint64_t        ticket = 0u;
  CHECK_STATUS(queue.queue(into.fill_with(5u, 1u), nullptr, into, &region, ticket), Status::not_found);
  device.hold(false);
  CHECK_STATUS(queue.queue(into.fill_with(5u, 1u), nullptr, into, &region, ticket), Status::ok);
  device.serve();
  CHECK(finishes(queue, ticket));
  CHECK_EQ(destination.at(0u, 0u), 5u); }

/* A blt from a present snapshot names no source pages: the device moves the
 * snapshot's pixels, swapped where the source's order is not the
 * destination's. */
TEST(present_queue, a_blt_from_a_snapshot_is_moved_out_of_it_by_the_device)
{ Fixture      fixture;
  EngineSide   device(fixture.mapped());
  PresentQueue queue(fixture.platform(), fixture.device(), epoch);
  CHECK_STATUS(queue.start(), Status::ok);
  static std::uint32_t pixels[side * side];
  for (std::uint32_t at = 0u; at < side * side; at++) pixels[at] = 0x80000000u | at;
  device.frame(av::SurfacePoint(0x4242u, 7u), AvFrameView{reinterpret_cast<const std::uint8_t *>(pixels), pitch, side, side, 0u});
  Surface              source(fixture.platform());
  Surface              destination(fixture.platform());
  const PresentSurface from = source.present(red_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(2u, 3u, 0u, 0u, 4u, 1u);
  std::uint64_t        ticket = 0u;
  CHECK_STATUS(queue.queue_image(av::SurfacePoint(0x4242u, 7u), from.blt_into(into, 1u), into, &region, ticket), Status::ok);
  CHECK_EQ(ticket, 1u);
  device.serve();
  CHECK(finishes(queue, 1u));
  CHECK_EQ(destination.at(0u, 0u), swapped(pixels[3u * side + 2u]));
  CHECK_EQ(destination.at(3u, 0u), swapped(pixels[3u * side + 5u]));
  CHECK_EQ(destination.at(4u, 0u), 0u);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }
