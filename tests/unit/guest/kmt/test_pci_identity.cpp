#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"

using av::PciIdentity;

TEST(pci_identity, is_the_same_only_when_every_field_matches)
{ const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
  CHECK(rvk.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u)));
  CHECK(!rvk.same_as(PciIdentity(0x10DEu, 0x1050u, 0x1AF4u, 0x5256u)));
  CHECK(!rvk.same_as(PciIdentity(0x1AF4u, 0x1051u, 0x1AF4u, 0x5256u)));
  CHECK(!rvk.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1B36u, 0x5256u)));
  CHECK(!rvk.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u)));   /* stock virtio-gpu-pci */
  CHECK(!rvk.same_as(PciIdentity())); }
