#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/scripted_adapter_order.hpp"
#include "icd/opengl/preferred_adapter.hpp"

#include <cstdint>

using av::MockKmt;
using av::PciIdentity;
using av::ScriptedAdapterOrder;
using av::glicd::PreferredAdapter;

namespace {

const PciIdentity on_radeon(0x1002u, 0x7550u, 0x1AF4u, 0x5256u);
const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u);
const PciIdentity real_radeon(0x1002u, 0x7550u, 0x1002u, 0x0E3Bu);

constexpr std::uint64_t display_luid = 0x22u;
constexpr std::uint64_t render_luid = 0x33u;

void two_adapters(MockKmt &kmt)
{
    kmt.add_adapter(on_radeon, 3200u);
    kmt.add_adapter(on_raphael, 3200u);
    kmt.set_adapter_luid(0u, display_luid);
    kmt.set_adapter_luid(1u, render_luid);
}

}

TEST(preferred_adapter, takes_the_adapter_dxgi_lists_first_when_it_is_an_avgpu_adapter)
{ MockKmt kmt; two_adapters(kmt);
  ScriptedAdapterOrder order;
  order.set_first(render_luid);
  PreferredAdapter preferred(kmt, order);
  CHECK_EQ(preferred.choose(nullptr), render_luid);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(preferred_adapter, falls_back_to_the_adapter_the_window_is_drawn_on)
{ MockKmt kmt; two_adapters(kmt);
  ScriptedAdapterOrder order;
  order.set_first(0x99u);
  order.set_drawn(display_luid);
  PreferredAdapter preferred(kmt, order);
  CHECK_EQ(preferred.choose(nullptr), display_luid); }

TEST(preferred_adapter, a_first_adapter_that_is_no_avgpu_adapter_is_passed_over)
{ MockKmt kmt; two_adapters(kmt);
  kmt.add_adapter(real_radeon, 3200u);
  kmt.set_adapter_luid(2u, 0x44u);
  ScriptedAdapterOrder order;
  order.set_first(0x44u);
  order.set_drawn(render_luid);
  PreferredAdapter preferred(kmt, order);
  CHECK_EQ(preferred.choose(nullptr), render_luid); }

TEST(preferred_adapter, answers_zero_when_neither_names_an_avgpu_adapter)
{ MockKmt kmt; two_adapters(kmt);
  ScriptedAdapterOrder order;
  order.set_first(0x99u);
  order.set_drawn(0x98u);
  PreferredAdapter preferred(kmt, order);
  CHECK_EQ(preferred.choose(nullptr), 0u); }

TEST(preferred_adapter, answers_zero_when_the_system_names_nothing)
{ MockKmt kmt; two_adapters(kmt);
  ScriptedAdapterOrder order;
  PreferredAdapter preferred(kmt, order);
  CHECK_EQ(preferred.choose(nullptr), 0u);
  CHECK_EQ(order.asked(), 2u); }
