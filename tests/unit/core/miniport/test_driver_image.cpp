#include "core/ddi/entry_points.hpp"
#include "core/miniport/ddi_table.hpp"
#include "core/miniport/driver_image.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::DriverImage;
using av::FakeDxgkrnl;
using av::MockPlatform;
using av::Status;

namespace av {

template <>
struct DriverImageProbe<MockPlatform> {
    explicit DriverImageProbe(DriverImage<MockPlatform> &target) : image(target) {}
    Status occupy_miniport_storage(MockPlatform &platform) { return image.miniport_.construct(platform); }
    void   release_miniport_storage() { image.miniport_.destroy(); }
    DriverImage<MockPlatform> &image;
};

}  // namespace av

using av::DriverImageProbe;

namespace {

int driver_object = 0;
int registry_path = 0;

/* An image lives in static storage in the kernel; here each test gets one and
 * tears it down the way the shim never has to. */
class Entered {
public:
    ~Entered() { image_.leave(); }
    DriverImage<MockPlatform> &image() { return image_; }

private:
    DriverImage<MockPlatform> image_;
};

}  // namespace

TEST(driver_image_enter, registers_the_miniport_table_and_binds_the_root)
{ Entered entered;
  CHECK_EQ(entered.image().enter(&driver_object, &registry_path), 0);
  MockPlatform &platform = entered.image().platform();
  CHECK(platform.registered_driver_object() == &driver_object);
  CHECK(platform.registered_registry_path() == &registry_path);
  CHECK(av::miniport_binding.miniport() != nullptr);
  FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(platform.registered_table()), Status::ok);
  CHECK_EQ(platform.logged(), 1u); }

TEST(driver_image_enter, a_registration_dxgkrnl_refuses_unbinds_and_returns_its_status)
{ Entered entered;
  CHECK_EQ(static_cast<std::uint32_t>(entered.image().enter(&driver_object, &registry_path, 0xC0000059u)),
           0xC0000059u);
  CHECK(av::miniport_binding.miniport() == nullptr); }

TEST(driver_image_enter, a_second_entry_is_refused)
{ Entered entered;
  CHECK_EQ(entered.image().enter(&driver_object, &registry_path), 0);
  CHECK_EQ(static_cast<std::uint32_t>(entered.image().enter(&driver_object, &registry_path)), 0xC0000184u); }

TEST(driver_image_enter, occupied_root_storage_is_refused)
{ Entered entered;
  MockPlatform scratch;
  DriverImageProbe<MockPlatform> probe(entered.image());
  CHECK_STATUS(probe.occupy_miniport_storage(scratch), Status::ok);
  CHECK_EQ(static_cast<std::uint32_t>(entered.image().enter(&driver_object, &registry_path)), 0xC0000184u);
  CHECK(av::miniport_binding.miniport() == nullptr);
  probe.release_miniport_storage(); }

TEST(driver_image_leave, unbinds_and_releases_everything)
{ Entered entered;
  CHECK_EQ(entered.image().enter(&driver_object, &registry_path), 0);
  int pdo = 0; void *context = nullptr;
  CHECK_EQ(AvDdiAddDevice(&pdo, &context), 0);
  entered.image().leave();
  CHECK(av::miniport_binding.miniport() == nullptr);
  entered.image().leave(); }
