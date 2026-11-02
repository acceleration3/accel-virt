#include "core/platform/log_budget.hpp"
#include "miniport/present/pixel_mover.hpp"
#include "miniport/present/present_queue.hpp"
#include "miniport/present/present_surface.hpp"
#include "miniport/present/present_ticket.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "proto/resources/surface_point.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>
#include <optional>

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
  PresentTicket ticket(platform, budget, nullptr, fallback, av::SurfacePoint());
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
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint());
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
  PresentTicket ticket(mapped.platform(), budget, &queue, fallback, av::SurfacePoint(0x99u, 2u));
  CHECK_STATUS(ticket.copy(surface, surface, &region, 1u), Status::ok);
  CHECK_STATUS(ticket.fill(surface, 7u, &region, 1u), Status::ok);
  CHECK_EQ(fallback.copies, 1u);
  CHECK_EQ(fallback.fills, 1u);
  CHECK_EQ(ticket.ticket(), 0u); }
