#include "core/platform/log_budget.hpp"
#include "miniport/present/pixel_mover.hpp"
#include "miniport/present/present_queue.hpp"
#include "miniport/present/present_surface.hpp"
#include "miniport/present/present_ticket.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/present/engine_side.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>
#include <optional>

using av::EngineSide;
using av::GuestPageList;
using av::LogBudget;
using av::MappedVirtioFunction;
using av::MockPlatform;
using av::PixelMover;
using av::PresentQueue;
using av::PresentRegion;
using av::PresentSurface;
using av::PresentTicket;
using av::Status;
using av::SurfaceLayout;
using av::VirtioGpuDevice;

namespace {
class CountingMover final : public PixelMover {
public:
    Status copy(const PresentSurface &, const PresentSurface &, const PresentRegion *, std::uint32_t count) override
    {
        copies += count;
        return Status::ok;
    }
    Status fill(const PresentSurface &, std::uint32_t, const PresentRegion *, std::uint32_t count) override
    {
        fills += count;
        return Status::ok;
    }

    std::uint32_t copies = 0u;
    std::uint32_t fills = 0u;
};

const std::uint64_t  frame = 0x40000u;
const PresentSurface surface(SurfaceLayout(4u, 4u, 21u, 16u), GuestPageList(&frame, 1u));
const PresentRegion  region(0u, 0u, 0u, 0u, 1u, 1u);

}

TEST(present_ticket, without_a_queue_the_fallback_moves_the_pixels_and_nothing_is_waited_for)
{ MockPlatform  platform;
  LogBudget     budget(platform);
  CountingMover fallback;
  PresentTicket ticket(platform, budget, nullptr, fallback, av::SurfacePoint(), av::SurfacePair());
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, 1u);
  CHECK_EQ(fallback.fills, 1u);
  CHECK_EQ(ticket.ticket(), 0u);
  CHECK_EQ(platform.logged(), 0u); }

TEST(present_ticket, a_present_the_queue_refuses_moves_at_once_and_is_logged)
{ MappedVirtioFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  std::optional<VirtioGpuDevice> device;
  device.emplace(mapped.platform(), mapped.mappings(), mapped.layout());
  PresentQueue  queue(mapped.platform(), *device, 1u);
  LogBudget     budget(mapped.platform());
  CountingMover fallback;
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(), av::SurfacePair());
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, 1u);
  CHECK_EQ(fallback.fills, 1u);
  CHECK_EQ(ticket.ticket(), 0u);
  CHECK(mapped.platform().logged_line("present engine did not take a present, status 7")); }

TEST(present_ticket, a_snapshot_the_queue_refuses_falls_back_to_the_pages)
{ MappedVirtioFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  std::optional<VirtioGpuDevice> device;
  device.emplace(mapped.platform(), mapped.mappings(), mapped.layout());
  PresentQueue  queue(mapped.platform(), *device, 1u);
  LogBudget     budget(mapped.platform());
  CountingMover fallback;
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(0x99u, 2u), av::SurfacePair());
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, 1u);
  CHECK_EQ(fallback.fills, 1u);
  CHECK_EQ(ticket.ticket(), 0u); }

TEST(present_ticket, a_present_the_queue_takes_is_waited_for_by_its_ticket_and_moved_by_nobody_here)
{ MappedVirtioFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  std::optional<VirtioGpuDevice> device;
  device.emplace(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device->start(3u), Status::ok);
  EngineSide    engine(mapped);
  PresentQueue  queue(mapped.platform(), *device, 1u);
  CHECK_STATUS(queue.start(), Status::ok);
  engine.hold(true);
  LogBudget     budget(mapped.platform());
  CountingMover fallback;
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(), av::SurfacePair());
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_EQ(ticket.ticket(), 1u);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(ticket.ticket(), 2u);
  CHECK_EQ(fallback.copies, 0u);
  CHECK_EQ(fallback.fills, 0u);
  CHECK_EQ(mapped.platform().logged(), 0u); }

TEST(present_ticket, refusals_past_the_log_budget_still_move_the_pixels_but_are_not_logged)
{ MappedVirtioFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  std::optional<VirtioGpuDevice> device;
  device.emplace(mapped.platform(), mapped.mappings(), mapped.layout());
  PresentQueue  queue(mapped.platform(), *device, 1u);
  LogBudget     budget(mapped.platform());
  CountingMover fallback;
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(), av::SurfacePair());
  for (std::uint32_t round = 0u; round < LogBudget::lines_per_window + 8u; round++)
      CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, LogBudget::lines_per_window + 8u);
  CHECK_EQ(mapped.platform().logged(), LogBudget::lines_per_window);
  CHECK_EQ(budget.dropped(), 8u); }

TEST(present_ticket, host_surfaces_are_copied_by_their_keys_with_no_pages_and_a_refused_queue_falls_back_to_the_pages)
{ MappedVirtioFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  std::optional<VirtioGpuDevice> device;
  device.emplace(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device->start(3u), Status::ok);
  EngineSide    engine(mapped);
  PresentQueue  queue(mapped.platform(), *device, 1u);
  CHECK_STATUS(queue.start(), Status::ok);
  engine.copy_between_surfaces(false);
  engine.hold(true);
  LogBudget     budget(mapped.platform());
  CountingMover fallback;
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(), av::SurfacePair(0x71u, 0x72u));
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_EQ(ticket.ticket(), 1u);
  CHECK_EQ(fallback.copies, 0u);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(ticket.ticket(), 2u);
  CHECK_EQ(fallback.fills, 0u);
  PresentTicket unstarted(mapped.platform(), budget, nullptr, fallback, av::SurfacePoint(),
                          av::SurfacePair(0x71u, 0x72u));
  CHECK_STATUS(unstarted.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, 1u);
  CHECK_EQ(unstarted.ticket(), 0u); }

TEST(present_ticket, a_host_source_with_no_destination_key_is_read_from_its_texture_and_falls_back_with_no_queue)
{ MappedVirtioFunction mapped;
  CHECK_STATUS(mapped.map(), Status::ok);
  std::optional<VirtioGpuDevice> device;
  device.emplace(mapped.platform(), mapped.mappings(), mapped.layout());
  CHECK_STATUS(device->start(3u), Status::ok);
  EngineSide    engine(mapped);
  PresentQueue  queue(mapped.platform(), *device, 1u);
  CHECK_STATUS(queue.start(), Status::ok);
  engine.hold(true);
  LogBudget     budget(mapped.platform());
  CountingMover fallback;
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(), av::SurfacePair(0x71u, 0u));
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_EQ(ticket.ticket(), 1u);
  CHECK_EQ(fallback.copies, 0u);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(ticket.ticket(), 2u);
  CHECK_EQ(fallback.fills, 0u);
  PresentTicket unstarted(mapped.platform(), budget, nullptr, fallback, av::SurfacePoint(), av::SurfacePair(0x71u, 0u));
  CHECK_STATUS(unstarted.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, 1u); }
