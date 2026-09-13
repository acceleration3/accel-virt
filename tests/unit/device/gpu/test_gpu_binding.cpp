#include "core/support/status.hpp"
#include "device/gpu/gpu_binding.hpp"
#include "core/support/text_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>
#include <cstring>

using av::GpuBinding;
using av::HostViolation;
using av::MockHost;
using av::Status;
using av::TextBuffer;

namespace {

constexpr std::uint8_t intel[16] = {0x86, 0x80, 0xa6, 0x46, 0x0c, 0, 0, 0, 0, 0x02, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t nvidia[16] = {0x7b, 0x7a, 0xd2, 0xc5, 0x42, 0x19, 0x8c, 0x80,
                                     0xc4, 0xfd, 0xa6, 0x4b, 0x87, 0x75, 0xae, 0xdb};

class Fixture {
public:
    Fixture() : message_{}, text_(message_, sizeof message_), binding_(host_)
    {
        host_.add_gpu(intel);
        host_.add_gpu(nvidia);
    }
    MockHost   &host() { return host_; }
    TextBuffer &text() { return text_; }
    GpuBinding &binding() { return binding_; }
    const char *message() const { return message_; }

private:
    MockHost   host_;
    char       message_[128];
    TextBuffer text_;
    GpuBinding binding_;
};

}  // namespace

TEST(gpu_binding_bind, a_uuid_binds_the_device_that_has_it)
{ Fixture fixture;
  CHECK(!fixture.binding().bound());
  CHECK_STATUS(fixture.binding().bind("7b7ad2c5-4219-8c80-c4fd-a64b8775aedb", fixture.text()), Status::ok);
  CHECK(fixture.binding().bound());
  CHECK_EQ(fixture.binding().index(), 1u);
  CHECK_EQ(fixture.text().length(), 0u);
  CHECK_EQ(fixture.host().logged(), 0u); }

TEST(gpu_binding_bind, auto_or_an_unset_property_binds_the_first_device)
{ Fixture fixture;
  CHECK_STATUS(fixture.binding().bind("auto", fixture.text()), Status::ok);
  CHECK_EQ(fixture.binding().index(), 0u);
  Fixture unset;
  CHECK_STATUS(unset.binding().bind(nullptr, unset.text()), Status::ok);
  CHECK_EQ(unset.binding().index(), 0u); }

TEST(gpu_binding_bind, each_failure_is_named_and_logged)
{ Fixture malformed;
  CHECK_STATUS(malformed.binding().bind("gpu0", malformed.text()), Status::invalid_parameter);
  CHECK(std::strcmp(malformed.message(), "virtio-gpu-rvk: host-gpu 'gpu0' is neither auto nor a device UUID") == 0);
  CHECK(!malformed.binding().bound());
  CHECK_EQ(malformed.host().logged(), 1u);
  Fixture absent;
  CHECK_STATUS(absent.binding().bind("00000000-0000-0000-0000-000000000001", absent.text()), Status::not_found);
  CHECK(std::strcmp(absent.message(),
                    "virtio-gpu-rvk: no host Vulkan device has UUID 00000000-0000-0000-0000-000000000001") == 0);
  MockHost bare;
  char bytes[80];
  TextBuffer text(bytes, sizeof bytes);
  GpuBinding none(bare);
  CHECK_STATUS(none.bind("auto", text), Status::not_found);
  CHECK(std::strcmp(bytes, "virtio-gpu-rvk: the host has no Vulkan device") == 0);
  CHECK_STATUS(none.bind("7b7ad2c5-4219-8c80-c4fd-a64b8775aedb", text), Status::not_found); }

/* The glue cannot allocate on a physical device nobody named to it, so binding
 * tells it which one this instance runs on - and a host that cannot take the
 * GPU leaves the device unbound rather than silently on another one. */
TEST(gpu_binding_bind, tells_the_host_which_gpu_this_device_runs_on)
{ Fixture fixture;
  CHECK_STATUS(fixture.binding().bind("7b7ad2c5-4219-8c80-c4fd-a64b8775aedb", fixture.text()), Status::ok);
  CHECK_EQ(fixture.host().bound_gpu(), 1u);
  Fixture first;
  CHECK_STATUS(first.binding().bind("auto", first.text()), Status::ok);
  CHECK_EQ(first.host().bound_gpu(), 0u); }

TEST(gpu_binding_bind, a_host_that_refuses_the_gpu_leaves_the_device_unbound)
{ Fixture fixture;
  fixture.host().fail_bind_gpu_at(1u);
  CHECK_STATUS(fixture.binding().bind("auto", fixture.text()), Status::not_supported);
  CHECK(!fixture.binding().bound());
  CHECK(std::strstr(fixture.message(), "host GPU 0") != nullptr);
  CHECK_EQ(fixture.host().bound_gpu(), MockHost::no_gpu); }

TEST(gpu_binding, binds_on_the_main_loop)
{ Fixture fixture;
  struct Call {
      Fixture *fixture;
      Status   status;
  } call = {&fixture, Status::ok};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       c->status = c->fixture->binding().bind("auto", c->fixture->text());
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  /* Binding names the main loop, and so does telling the host which GPU it took. */
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }
