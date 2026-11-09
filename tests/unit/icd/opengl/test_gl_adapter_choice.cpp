#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_icd_root.hpp"

#include <cstdint>

namespace av::glicd {

struct GlIcdRootProbe {
    static std::uint32_t adapter(GlIcdRoot &root) { return root.connection_.adapter(); }
};

}

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::GlIcdRig;
using av::PciIdentity;
using av::glicd::GlIcdRootProbe;

namespace {

constexpr std::uint64_t first_luid = 0x21u;
constexpr std::uint64_t second_luid = 0x22u;

void two_adapters(GlIcdRig &rig)
{
    rig.kmt().add_adapter(PciIdentity(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem),
                          AdapterScenario::wddm_3_2);
    rig.kmt().set_adapter_luid(0u, first_luid);
    rig.kmt().set_adapter_luid(1u, second_luid);
}

std::uint64_t luid_of_connection(GlIcdRig &rig)
{
    std::uint64_t luid = 0u;
    CHECK_STATUS(rig.kmt().adapter_luid(GlIcdRootProbe::adapter(rig.root()), luid), av::Status::ok);
    return luid;
}

}

TEST(gl_adapter_choice, the_first_context_made_current_runs_on_the_adapter_dxgi_lists_first)
{ GlIcdRig rig;
  two_adapters(rig);
  rig.order().set_first(second_luid);
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  CHECK_EQ(luid_of_connection(rig), second_luid); }

TEST(gl_adapter_choice, with_no_adapter_named_the_first_avgpu_adapter_is_used)
{ GlIcdRig rig;
  two_adapters(rig);
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  CHECK_EQ(luid_of_connection(rig), first_luid); }

TEST(gl_adapter_choice, the_adapter_is_chosen_once_and_a_later_context_follows_it)
{ GlIcdRig rig;
  two_adapters(rig);
  rig.order().set_first(second_luid);
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  const std::uint32_t asked = rig.order().asked();
  rig.order().set_first(first_luid);
  CHECK(rig.current_context() != nullptr);
  CHECK_EQ(rig.order().asked(), asked);
  CHECK_EQ(luid_of_connection(rig), second_luid); }

TEST(gl_adapter_choice, a_context_that_is_only_created_asks_nothing_of_the_system)
{ GlIcdRig rig;
  two_adapters(rig);
  rig.load_with_format();
  CHECK(rig.opengl32().create_context(rig.dc()) != nullptr);
  CHECK_EQ(rig.order().asked(), 0u); }
