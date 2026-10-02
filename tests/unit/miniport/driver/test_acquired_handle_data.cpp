#include "miniport/driver/acquired_handle_data.hpp"

#include "miniport/driver/dxgkrnl_interface.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::AcquiredHandleData;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeHandleTable;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace av {

/* A loan whose data went away but whose release handle did not - the state the
 * destructor must never be left to hand back. */
struct AcquiredHandleDataProbe {
    static void forget_data(AcquiredHandleData &acquired) { acquired.data_ = nullptr; }
    static void clear_release(AcquiredHandleData &acquired) { acquired.release_handle_ = nullptr; }
};

}  // namespace av

namespace {

int driver_data = 0;

/* A DxgkrnlInterface over the fake adapter's callbacks, and the handle table
 * they answer from. */
class Lender {
public:
    Lender() : platform_(), function_(), dxgkrnl_(platform_, function_), interface_(platform_), handles_()
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "fixture");
    }
    MockPlatform     &platform() { return platform_; }
    DxgkrnlInterface &interface() { return interface_; }
    FakeHandleTable  &handles() { return handles_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    FakeHandleTable       handles_;
};

}  // namespace

TEST(acquired_handle_data_acquire, lends_the_data_and_hands_the_loan_back)
{ Lender lender;
  const std::uint32_t handle = lender.handles().insert(&driver_data);
  {
      AcquiredHandleData acquired(lender.interface(), handle);
      CHECK(acquired.data() == nullptr);
      CHECK(acquired.invariants_hold());
      CHECK_STATUS(acquired.acquire(), Status::ok);
      CHECK(acquired.data() == &driver_data);
      CHECK(acquired.invariants_hold());
      CHECK_EQ(lender.handles().outstanding(), 1u);
  }
  /* The destructor released it: the table would fail the test otherwise. */
  CHECK_EQ(lender.handles().outstanding(), 0u); }

/* dxgkrnl knowing no such handle is invalid_parameter, never not_found, and
 * leaves nothing to hand back. */
TEST(acquired_handle_data_acquire, an_unknown_handle_lends_nothing)
{ Lender lender;
  {
      AcquiredHandleData acquired(lender.interface(), 0x40000999u);
      CHECK_STATUS(acquired.acquire(), Status::invalid_parameter);
      CHECK(acquired.data() == nullptr);
      CHECK(acquired.invariants_hold());
      CHECK_EQ(lender.handles().outstanding(), 0u);
  }
  CHECK_EQ(lender.handles().outstanding(), 0u); }

TEST(acquired_handle_data_acquire, refuses_a_second_loan_on_the_same_object)
{ Lender lender;
  const std::uint32_t handle = lender.handles().insert(&driver_data);
  AcquiredHandleData acquired(lender.interface(), handle);
  CHECK_STATUS(acquired.acquire(), Status::ok);
  CHECK_STATUS(acquired.acquire(), Status::invalid_state);
  CHECK(acquired.data() == &driver_data);
  CHECK(acquired.invariants_hold()); }

TEST(acquired_handle_data_acquire, is_apc_at_most)
{ Lender lender;
  const std::uint32_t handle = lender.handles().insert(&driver_data);
  {
      AcquiredHandleData acquired(lender.interface(), handle);
      lender.platform().set_irql(Irql::dispatch);
      static_cast<void>(acquired.acquire());
      lender.platform().set_irql(Irql::passive);
      CHECK(lender.platform().claim(Violation::irql_ceiling));
  }
  while (lender.platform().claim(Violation::irql_ceiling)) {} }

TEST(acquired_handle_data_invariants, a_loan_with_no_data_to_read_is_detected)
{ Lender lender;
  const std::uint32_t handle = lender.handles().insert(&driver_data);
  AcquiredHandleData acquired(lender.interface(), handle);
  CHECK_STATUS(acquired.acquire(), Status::ok);
  av::AcquiredHandleDataProbe::forget_data(acquired);
  CHECK(!acquired.invariants_hold());
  /* Hand the loan back through the table so the fixture stays honest. */
  av::AcquiredHandleDataProbe::clear_release(acquired);
  lender.interface().release_handle_data(&driver_data);
  CHECK(acquired.invariants_hold()); }
