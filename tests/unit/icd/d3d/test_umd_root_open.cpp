/* The two exports, driven as d3d11.dll and D3D12Core.dll drive them
 * (contracts/umd_interface.md). The fake checks the table the way the real
 * runtime does - D3D12Core zeroes all eight slots and refuses the driver
 * unless the last six come back non-null (research.md §12) - so a table with a
 * hole is a failing test rather than a silent pass. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/fake_d3d_runtime.hpp"
#include "harness/icd/umd_rig.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_binding.hpp"

#include <cstdint>
#include <cstring>

using av::FakeD3dRuntime;
using av::MockKmt;
using av::UmdRig;
using av::d3d::RuntimeKind;
using namespace av::d3d;

TEST(umd_root_open, the_d3d11_runtime_opens_the_adapter_and_gets_a_whole_table)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK(runtime.table_is_whole());
  CHECK(runtime.adapter() != nullptr);
  CHECK(rig.root().invariants_hold());
  /* One kernel adapter for the one open, and the identity learned off it. */
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(umd_root_open, the_d3d12_runtime_opens_the_adapter_and_gets_all_eight_slots)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK(runtime.table_is_whole());
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

/* Two opens in one process are two adapters over one kernel adapter: the D3D
 * DDI has no instance, so both must reach the same kernel object. */
TEST(umd_root_open, two_runtimes_in_one_process_share_one_kernel_adapter)
{ UmdRig rig;
  FakeD3dRuntime eleven(rig.root(), RuntimeKind::d3d10_11);
  FakeD3dRuntime twelve(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(eleven.open(), hr_ok);
  const std::uint32_t after_one = rig.kmt().open_objects();
  CHECK_EQ(twelve.open(), hr_ok);
  CHECK_EQ(rig.kmt().open_objects(), after_one);
  CHECK(eleven.adapter() != twelve.adapter());
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(eleven.close_adapter(), hr_ok);
  /* The kernel adapter is still held: the other open still needs it. */
  CHECK_EQ(rig.kmt().open_objects(), after_one);
  CHECK_EQ(twelve.close_adapter(), hr_ok);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(umd_root_open, a_null_argument_or_a_null_table_is_refused_before_anything_is_opened)
{ UmdRig rig;
  CHECK_EQ(rig.root().open_adapter10_2(nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().open_adapter12(nullptr), hr_invalid_arg);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  runtime.offer_no_table();
  CHECK_EQ(runtime.open(), hr_invalid_arg);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(umd_root_open, a_runtime_that_offers_no_callbacks_is_refused)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  runtime.offer_no_callbacks();
  CHECK_EQ(runtime.open(), hr_invalid_arg);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

/* A runtime asking for a DDI revision this driver has not implemented gets
 * E_NOINTERFACE and no adapter: answering it would be claiming a version
 * (constitution VI). */
TEST(umd_root_open, an_interface_this_driver_does_not_serve_is_refused)
{ const std::uint32_t neighbours[] = {0u, FakeD3dRuntime::d3d11_interface - 1u,
                                      FakeD3dRuntime::d3d11_interface + 1u, 0xFFFFFFFFu};
  for (const std::uint32_t asked : neighbours) {
      UmdRig rig;
      FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
      runtime.ask_for_interface(asked);
      CHECK_EQ(runtime.open(), hr_no_interface);
      CHECK(runtime.adapter() == nullptr);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.log().holding("open refused, interface"), 1u);
      CHECK(rig.root().invariants_hold());
  } }

TEST(umd_root_open, no_rvkgpu_adapter_is_a_failure_and_a_log_line)
{ UmdRig rig;
  rig.kmt().claim_adapters(0u);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_fail);
  CHECK_EQ(rig.log().holding("no rvkgpu adapter"), 1u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

/* An adapter whose LUID the kernel refuses is an adapter this driver cannot
 * report, so the open fails and gives the kernel adapter back. */
TEST(umd_root_open, an_adapter_whose_identity_is_refused_is_given_back)
{ UmdRig rig;
  rig.kmt().fail_at(MockKmt::Call::adapter_luid, 1u, MockKmt::invalid_parameter);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_fail);
  CHECK(runtime.adapter() == nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

/* Every allocation the open makes, failed in turn: the documented code, no
 * kernel object left open, no memory leaked, and one line saying why. */
TEST(umd_root_open, an_open_with_no_memory_answers_out_of_memory_and_leaks_nothing)
{ UmdRig rig;
  rig.platform().fail_allocation_at(1u);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_out_of_memory);
  CHECK(runtime.adapter() == nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u);
  CHECK_EQ(rig.log().holding("no memory for the adapter"), 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(umd_root_open, the_table_slots_refuse_an_adapter_handle_this_driver_never_gave)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  int                 forged = 0;
  std::uint32_t       entries = 0u;
  CHECK_EQ(rig.root().supported_versions(&forged, &entries, nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().caps(&forged, nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().private_device_bytes(&forged), 0u);
  CHECK_EQ(rig.root().create_device(&forged, nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().create_device12(&forged, nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().close_adapter(&forged), hr_invalid_arg);
  CHECK_EQ(rig.root().optional_ddi_tables(&forged, &entries, nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().fill_ddi_table(&forged, 0u), hr_invalid_arg);
  /* A null handle is the same answer and is never dereferenced. */
  CHECK_EQ(rig.root().close_adapter(nullptr), hr_invalid_arg);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(umd_root_open, the_supported_versions_slot_answers_the_one_word_this_driver_declares)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  std::uint32_t entries = 0u;
  CHECK_EQ(runtime.supported_versions(&entries, nullptr), hr_ok);
  CHECK_EQ(entries, 1u);
  std::uint64_t words[1] = {0ull};
  CHECK_EQ(runtime.supported_versions(&entries, words), hr_ok);
  CHECK_EQ(words[0] >> 32, FakeD3dRuntime::d3d11_interface);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

/* No D3D capability is implemented here, so none is claimed. D3D12Core's two
 * pipeline-level types are named because they are the first it asks and the
 * ones 057 owes (research.md §12). */
TEST(umd_root_open, every_cap_is_refused_and_logged_with_the_type_asked_for)
{ const std::uint32_t types[] = {128u, 129u, 130u, 1007u, 1074u, 0u, 0xFFFFFFFFu};
  UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  std::uint64_t answer = 0xA5A5A5A5A5A5A5A5ull;
  for (const std::uint32_t type : types) {
      CHECK_EQ(runtime.caps(type, &answer, sizeof answer), hr_invalid_arg);
      /* Refused means nothing written, not zeroed. */
      CHECK_EQ(answer, 0xA5A5A5A5A5A5A5A5ull);
  }
  CHECK_EQ(rig.log().holding("cap refused, type"), 7u);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(umd_root_open, d3d12_answers_no_optional_tables_and_fills_none)
{ const std::uint32_t needed[] = {0u, 1u, 2u, 3u};
  UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  std::uint32_t entries = 7u;
  CHECK_EQ(runtime.optional_ddi_tables(&entries, nullptr), hr_ok);
  CHECK_EQ(entries, 0u);
  for (const std::uint32_t table : needed) CHECK_EQ(runtime.fill_ddi_table(table), hr_not_impl);
  CHECK_EQ(rig.log().holding("ddi table refused, type"), 4u);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(umd_root_open, a_state_dump_names_the_adapters_and_the_connection)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  char bytes[1024] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.root().report(writer);
  CHECK(std::strstr(writer.dump(), "umd/0 adapters=1 devices=0") != nullptr);
  CHECK(std::strstr(writer.dump(), "opens=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "luid=0x0000000200001234") != nullptr);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

/* A state dump must never wait on the thread that is inside the driver. */
TEST(umd_root_open, a_state_dump_says_busy_rather_than_waiting)
{ UmdRig rig;
  rig.lock().held_elsewhere(true);
  char bytes[256] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  rig.root().report(writer);
  CHECK(std::strstr(writer.dump(), "busy=true") != nullptr);
  rig.lock().held_elsewhere(false); }

TEST(umd_root_open, the_root_is_bound_while_it_lives_and_unbound_after)
{ { UmdRig rig;
    CHECK(umd_binding.bound());
    CHECK(&umd_binding.root() == &rig.root()); }
  CHECK(!umd_binding.bound()); }
