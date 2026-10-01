#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"

using av::PciIdentity;

TEST(pci_identity, is_the_same_only_when_every_field_matches)
{ const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
  CHECK(av_gpu.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x10DEu, 0x1050u, 0x1AF4u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x1AF4u, 0x1051u, 0x1AF4u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1B36u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u)));   /* stock virtio-gpu-pci */
  CHECK(!av_gpu.same_as(PciIdentity())); }

TEST(pci_identity, the_same_subsystem_whatever_vendor_and_device_it_presents)
{ const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
  CHECK(av_gpu.same_subsystem_as(PciIdentity(0x1002u, 0x7550u, 0x1AF4u, 0x5256u)));   /* on the host's RX 9070 XT */
  CHECK(av_gpu.same_subsystem_as(PciIdentity(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u)));   /* on its Raphael iGPU */
  CHECK(av_gpu.same_subsystem_as(av_gpu));
  CHECK(!av_gpu.same_subsystem_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u)));   /* stock virtio-gpu-pci */
  CHECK(!av_gpu.same_subsystem_as(PciIdentity(0x1002u, 0x7550u, 0x1002u, 0x5256u)));   /* a real card's own subsystem */
  CHECK(!av_gpu.same_subsystem_as(PciIdentity())); }
