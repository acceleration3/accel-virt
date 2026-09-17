/* Many contexts on many threads (US3, T039): contexts on two threads share the
 * process's one rvkgpu device while each owns its rvk context; a context
 * current on one thread is refused on another; deleting contexts in every
 * order releases each host context once and the last one closes the device;
 * and a Vulkan connection beside them keeps its own rvk context. Everything is
 * driven as opengl32 drives it, through FakeOpengl32, with invariants checked
 * after every step. */
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"

#include "gl_protocol_ids.h"
#include "gl_types.h"

#include <cstdint>

using av::GlIcdRig;
using av::MockKmt;
using av::Status;
using Root = av::glicd::GlIcdRoot;

namespace {

using ClearFn = void (APIENTRY *)(GLbitfield);

constexpr std::uint32_t slot_clear = 203u;
constexpr GLbitfield    color_buffer_bit = 0x4000u;

/* Every thread of a scenario draws on its own window. */
struct Scenario {
    GlIcdRig rig;

    Scenario()
    {
        rig.load_with_format();
        CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 1));
    }

    void *dc_of(std::uint32_t thread) { return thread == 1u ? rig.dc() : rig.other_dc(); }

    /* A context made current on `thread`, the host answering its make and its
     * window; null when either failed. */
    void *made_on(std::uint32_t thread)
    {
        rig.threads().switch_to(thread);
        void *const context = rig.opengl32().create_context(dc_of(thread));
        if (context == nullptr) return nullptr;
        rig.host().gl_make("host gpu", "4.6.0 host");
        rig.host().gl_window(1u, 1u);
        return rig.opengl32().make_current(dc_of(thread), context) ? context : nullptr;
    }

    /* Host contexts created and not yet destroyed. */
    std::uint32_t live_host_contexts()
    {
        return rig.kmt().calls(MockKmt::Call::create_context_virtual) - rig.kmt().calls(MockKmt::Call::destroy_context);
    }

    /* A glClear from the calling thread's table: one command on its ring. */
    std::uint32_t clear_sends()
    {
        const std::uint32_t before = rig.kmt().commands();
        rig.opengl32().gl<ClearFn>(slot_clear)(color_buffer_bit);
        return rig.kmt().commands() - before;
    }
};

}  // namespace

TEST(gl_contexts, three_contexts_on_two_threads_share_one_device_and_own_three_rvk_contexts)
{ Scenario scenario;
  GlIcdRig &rig = scenario.rig;
  const void *const first = scenario.made_on(1u);
  CHECK(first != nullptr);
  CHECK(rig.root().invariants_hold());
  const void *const second = scenario.made_on(2u);
  CHECK(second != nullptr);
  CHECK(rig.root().invariants_hold());
  /* A second context on thread 2: opengl32 releases the first there. */
  const void *const third = scenario.made_on(2u);
  CHECK(third != nullptr);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 3u);
  CHECK_EQ(scenario.live_host_contexts(), 3u);
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::opengl);
  /* Each thread draws on its own current context. */
  CHECK_EQ(scenario.clear_sends(), 1u);
  rig.threads().switch_to(1u);
  CHECK_EQ(scenario.clear_sends(), 1u);
  CHECK(rig.root().current() != nullptr);
  rig.threads().switch_to(2u);
  CHECK(rig.root().current() != nullptr);
  CHECK(rig.root().invariants_hold());
  static_cast<void>(first);
  static_cast<void>(second);
  static_cast<void>(third); }

/* US3.2: a context current on thread A is refused on thread B with
 * ERROR_BUSY, and A keeps drawing on it as before. */
TEST(gl_contexts, a_context_current_on_one_thread_is_refused_on_another_and_left_as_it_was)
{ Scenario scenario;
  GlIcdRig &rig = scenario.rig;
  void *const held = scenario.made_on(1u);
  CHECK(held != nullptr);
  const auto *const current = rig.root().current();
  rig.threads().switch_to(2u);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK(!rig.opengl32().make_current(rig.other_dc(), held));
  CHECK_EQ(rig.threads().last_error(), Root::error_busy);
  CHECK(rig.root().current() == nullptr);
  CHECK(rig.opengl32().slot(slot_clear) == nullptr);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK(!rig.opengl32().delete_context(held));
  CHECK_EQ(rig.threads().last_error(), Root::error_busy);
  CHECK(rig.root().invariants_hold());
  rig.threads().switch_to(1u);
  CHECK(rig.root().current() == current);
  CHECK_EQ(scenario.clear_sends(), 1u);
  CHECK(rig.root().invariants_hold()); }

namespace {

enum class Kind : std::uint8_t { current, released, unmade };

/* Every order of the three kinds, as indices into {current, released, unmade}. */
constexpr std::uint8_t orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};

}  // namespace

/* A context current on thread 1, one made on thread 2 then released there,
 * and one never made, deleted in each of the six orders: the two host
 * contexts are destroyed exactly once each, the unmade one costs nothing, and
 * the device closes with the last made context. */
TEST(gl_contexts, deleting_current_released_and_unmade_contexts_in_every_order_releases_each_once)
{ for (const auto &order : orders) {
    Scenario scenario;
    GlIcdRig &rig = scenario.rig;
    void *contexts[3] = {};
    contexts[0] = scenario.made_on(1u);
    contexts[1] = scenario.made_on(2u);
    CHECK(rig.opengl32().make_current(rig.other_dc(), nullptr));
    rig.threads().switch_to(2u);
    contexts[2] = rig.opengl32().create_context(rig.other_dc());
    CHECK(contexts[0] != nullptr && contexts[1] != nullptr && contexts[2] != nullptr);
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 2u);
    CHECK(rig.root().invariants_hold());
    std::uint32_t made_left = 2u;
    for (const std::uint8_t which : order) {
      /* The current context is deleted from its own thread, as opengl32 allows. */
      rig.threads().switch_to(static_cast<Kind>(which) == Kind::current ? 1u : 2u);
      CHECK(rig.opengl32().delete_context(contexts[which]));
      CHECK(rig.root().invariants_hold());
      if (static_cast<Kind>(which) != Kind::unmade) made_left--;
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_context), 2u - made_left);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), made_left == 0u ? 1u : 0u);
    }
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 2u);
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::close_adapter), 1u);
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    CHECK_EQ(rig.kmt().misuses(), 0u);
    rig.threads().switch_to(1u);
    CHECK(rig.root().current() == nullptr);
  } }

/* The process's last context closes the device and its adapter; until then
 * the device stays open for the contexts left. */
TEST(gl_contexts, the_last_context_deleted_closes_the_device)
{ Scenario scenario;
  GlIcdRig &rig = scenario.rig;
  void *const first = scenario.made_on(1u);
  void *const second = scenario.made_on(2u);
  CHECK(rig.opengl32().delete_context(second));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), 0u);
  CHECK(rig.kmt().open_objects() > 0u);
  rig.threads().switch_to(1u);
  CHECK_EQ(scenario.clear_sends(), 1u);
  CHECK(rig.opengl32().delete_context(first));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::close_adapter), 1u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold());
  /* A context made after that opens the device again. */
  CHECK(scenario.made_on(1u) != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 2u); }

/* 045's Vulkan ICD opens its own device and transport in the same process: an
 * OpenGL context beside it gets a device and an rvk context of its own, and
 * either one closing leaves the other working. */
TEST(gl_contexts, a_vulkan_connection_and_an_opengl_context_each_keep_their_own_rvk_context)
{ Scenario scenario;
  GlIcdRig &rig = scenario.rig;
  const av::PciIdentity rvkgpu(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                               av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem);
  av::AdapterSearch     search(rig.kmt());
  CHECK_STATUS(search.find(rvkgpu), Status::ok);
  av::GuestConnection vulkan_device(rig.kmt());
  CHECK_STATUS(vulkan_device.open(search.adapter()), Status::ok);
  std::uint8_t     staging[4096] = {};
  av::KmtTransport vulkan(vulkan_device, av::SubApi::vulkan, 12u, 4096u, staging, sizeof staging);
  CHECK_STATUS(vulkan.initialize(), Status::ok);
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::vulkan);

  void *const context = scenario.made_on(1u);
  CHECK(context != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 2u);
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::opengl);
  CHECK(vulkan.established());

  vulkan.teardown();
  vulkan_device.close();
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(scenario.clear_sends(), 1u);
  CHECK(rig.root().invariants_hold());
  CHECK(rig.opengl32().delete_context(context));
  CHECK_EQ(rig.kmt().open_objects(), 0u); }
