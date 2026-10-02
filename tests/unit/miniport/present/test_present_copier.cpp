/* A present's pixels handed to the host: the request the copier sends is
 * answered by the device's own PresentCommands over the guest's memory, so a
 * pixel that arrives proves the whole path - the fixed part, both page lists
 * and the regions in the payload, and the answer. Whatever the host cannot
 * take moves through the fallback instead, and the first time says so in the
 * log. */
#include "miniport/present/mapped_pixels.hpp"
#include "miniport/present/present_copier.hpp"
#include "miniport/present/present_surface.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "device/control/control_request.hpp"
#include "device/control/present_commands.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/commands/present_region.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

using av::ControlRequest;
using av::FakeVirtqueueDevice;
using av::GpuResponse;
using av::GuestPageList;
using av::MappedPixels;
using av::MappedVirtioFunction;
using av::MockHost;
using av::MockPlatform;
using av::MockRequest;
using av::PresentCommands;
using av::PresentCopier;
using av::PresentCopyRequest;
using av::PresentRegion;
using av::PresentSurface;
using av::Status;
using av::SurfaceLayout;
using av::VirtioGpuDevice;

namespace {

constexpr std::uint32_t side = 16u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
constexpr std::uint32_t blue_first = 21u;   /* D3DDDIFMT_A8R8G8B8 */
constexpr std::uint32_t red_first = 32u;    /* D3DDDIFMT_A8B8G8R8 */

/* The device's side of the control queue, answering in the driver's stalls:
 * the device core's PresentCommands over the guest's own memory, or a
 * refusal, or bytes that are no answer, or nothing. */
class HostSide {
public:
    enum class Answer { commands, refusal, garbage, nothing };

    explicit HostSide(MappedVirtioFunction &mapped)
        : platform_(mapped.platform()), queue_(mapped.queue_device(0u)), commands_(host_)
    {
        host_.translate_guest_with([](void *context, std::uint64_t address, std::uint64_t length) {
            return static_cast<MockPlatform *>(context)->translate_physical(address, static_cast<std::size_t>(length));
        }, &platform_);
        platform_.on_stall(&HostSide::stall, this);
    }
    ~HostSide() { platform_.on_stall(nullptr, nullptr); }
    HostSide(const HostSide &) = delete;
    HostSide &operator=(const HostSide &) = delete;

    void          answer_with(Answer answer) { answer_ = answer; }
    std::uint32_t copies() const { return copies_; }

private:
    static void stall(void *context)
    {
        auto *const self = static_cast<HostSide *>(context);
        if (self->answer_ == Answer::nothing || self->queue_.pending_type() == 0u) return;
        static_cast<void>(self->queue_.answer_all(self->request_, sizeof self->request_, &HostSide::answer, self));
    }

    static std::uint32_t answer(void *context, const std::uint8_t *request, std::uint32_t length,
                                std::uint8_t *answer, std::uint32_t room)
    {
        auto *const   self = static_cast<HostSide *>(context);
        std::uint32_t type = 0u;
        std::memcpy(&type, request, sizeof type);
        if (type == PresentCopyRequest::type) self->copies_++;
        if (self->answer_ == Answer::garbage) {
            std::memset(answer, 0xEE, 8u);
            return 8u;
        }
        GpuResponse refusal;
        refusal.set(GpuResponse::err_invalid_parameter);
        MockRequest    mock(request, length);
        ControlRequest control(self->host_, &mock);
        const bool     answered = self->answer_ == Answer::commands && self->commands_.answer(control, type);
        const std::uint8_t *bytes = answered ? mock.response_bytes() : refusal.bytes();
        const std::size_t   size = answered ? mock.response_size() : refusal.length();
        const std::size_t   given = size < room ? size : room;
        std::memcpy(answer, bytes, given);
        return static_cast<std::uint32_t>(given);
    }

    MockPlatform       &platform_;
    FakeVirtqueueDevice queue_;
    MockHost            host_;
    PresentCommands     commands_;
    Answer              answer_ = Answer::commands;
    std::uint32_t       copies_ = 0u;
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
    }

    Status                start() { return device_->start(3u); }
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

}  // namespace

/* The host moved them: the guest never mapped a page, and the device was
 * asked once. */
TEST(present_copier, a_blt_is_moved_by_the_host)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  HostSide      host(fixture.mapped());
  MappedPixels  fallback(fixture.platform());
  PresentCopier copier(fixture.platform(), fixture.device(), fallback);
  CHECK_STATUS(copier.initialize(), Status::ok);
  Surface source(fixture.platform());
  Surface destination(fixture.platform());
  source.number();
  const PresentSurface from = source.present(red_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  regions[] = {PresentRegion(1u, 2u, 3u, 4u, 5u, 6u), PresentRegion(0u, 15u, 0u, 0u, 1u, 1u)};
  CHECK_STATUS(copier.copy(from, into, regions, 2u), Status::ok);
  CHECK_EQ(host.copies(), 1u);
  CHECK_EQ(destination.at(3u, 4u), swapped(source.at(1u, 2u)));
  CHECK_EQ(destination.at(7u, 9u), swapped(source.at(5u, 7u)));
  CHECK_EQ(destination.at(8u, 4u), 0u);
  CHECK_EQ(destination.at(0u, 0u), swapped(source.at(0u, 15u)));
  CHECK_EQ(fixture.platform().mapped_frames(), 0u);
  CHECK_EQ(fixture.platform().logged_line("would not move"), false);
  /* Surfaces of one order move byte for byte. */
  const PresentSurface same = destination.present(red_first);
  CHECK_STATUS(copier.copy(from, same, regions, 1u), Status::ok);
  CHECK_EQ(destination.at(3u, 4u), source.at(1u, 2u)); }

TEST(present_copier, a_fill_is_moved_by_the_host)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  HostSide      host(fixture.mapped());
  MappedPixels  fallback(fixture.platform());
  PresentCopier copier(fixture.platform(), fixture.device(), fallback);
  CHECK_STATUS(copier.initialize(), Status::ok);
  Surface              destination(fixture.platform());
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 2u, 2u, 3u, 1u);
  CHECK_STATUS(copier.fill(into, 0xFF00FF00u, &region, 1u), Status::ok);
  CHECK_EQ(host.copies(), 1u);
  CHECK_EQ(destination.at(2u, 2u), 0xFF00FF00u);
  CHECK_EQ(destination.at(4u, 2u), 0xFF00FF00u);
  CHECK_EQ(destination.at(5u, 2u), 0u); }

/* A device that refuses, answers what is no answer, or answers nothing: the
 * pixels still move, through the miniport, and the log says so once. */
TEST(present_copier, what_the_host_will_not_move_the_fallback_does)
{ Fixture fixture;
  CHECK_STATUS(fixture.start(), Status::ok);
  HostSide      host(fixture.mapped());
  MappedPixels  fallback(fixture.platform());
  PresentCopier copier(fixture.platform(), fixture.device(), fallback);
  CHECK_STATUS(copier.initialize(), Status::ok);
  Surface source(fixture.platform());
  Surface destination(fixture.platform());
  source.number();
  const PresentSurface from = source.present(blue_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 0u, 0u, side, side);
  const HostSide::Answer answers[] = {HostSide::Answer::refusal, HostSide::Answer::garbage,
                                      HostSide::Answer::nothing};
  for (const HostSide::Answer answer : answers) {
      host.answer_with(answer);
      std::uint32_t value = 0u;
      CHECK_STATUS(copier.copy(from, into, &region, 1u), Status::ok);
      CHECK_EQ(destination.at(9u, 9u), source.at(9u, 9u));
      CHECK_STATUS(copier.fill(into, value, &region, 1u), Status::ok);
      CHECK_EQ(destination.at(9u, 9u), 0u);
  }
  CHECK(fixture.platform().logged_line("would not move"));
  const std::uint32_t logged = fixture.platform().logged();
  CHECK_STATUS(copier.copy(from, into, &region, 1u), Status::ok);
  CHECK_EQ(fixture.platform().logged(), logged);
  CHECK_EQ(fixture.platform().mapped_frames(), 0u); }

/* No payload, no running device, or more pages than the payload holds: the
 * host is not asked, and the fallback answers. */
TEST(present_copier, a_present_the_host_cannot_be_handed_goes_to_the_fallback)
{ Fixture       fixture;
  MappedPixels  fallback(fixture.platform());
  PresentCopier copier(fixture.platform(), fixture.device(), fallback);
  Surface source(fixture.platform());
  Surface destination(fixture.platform());
  source.number();
  const PresentSurface from = source.present(blue_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 0u, 0u, side, side);
  /* Not initialized: no payload. */
  CHECK_STATUS(copier.copy(from, into, &region, 1u), Status::ok);
  CHECK_EQ(destination.at(5u, 5u), source.at(5u, 5u));
  CHECK_STATUS(copier.initialize(), Status::ok);
  CHECK_STATUS(copier.initialize(), Status::invalid_state);
  /* A device that is not running. */
  CHECK_STATUS(copier.fill(into, 0x7u, &region, 1u), Status::ok);
  CHECK_EQ(destination.at(5u, 5u), 0x7u);
  /* A page list past the payload: every page a run of its own. */
  CHECK_STATUS(fixture.start(), Status::ok);
  HostSide host(fixture.mapped());
  constexpr std::uint32_t scattered = PresentCopier::payload_bytes / PresentCopyRequest::entry_size + 1u;
  static std::uint64_t    frames[scattered];
  for (std::uint32_t index = 0u; index < scattered; index++)
      frames[index] = 0x100000000u + std::uint64_t{index} * 2u * GuestPageList::page_bytes;
  const PresentSurface huge(SurfaceLayout(side, side, blue_first, pitch), GuestPageList(frames, scattered));
  CHECK(copier.copy(huge, into, &region, 1u) != Status::ok);
  CHECK_EQ(host.copies(), 0u); }

TEST(present_copier, no_contiguous_memory_is_no_payload)
{ Fixture       fixture;
  MappedPixels  fallback(fixture.platform());
  PresentCopier copier(fixture.platform(), fixture.device(), fallback);
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(copier.initialize(), Status::no_memory);
  CHECK_STATUS(copier.initialize(), Status::ok); }
