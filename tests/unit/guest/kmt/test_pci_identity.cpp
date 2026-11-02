#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "core/support/text_buffer.hpp"

#include <cstring>

using av::PciIdentity;

TEST(pci_identity, is_the_same_only_when_every_field_matches)
{ const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
  CHECK(av_gpu.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x10DEu, 0x1050u, 0x1AF4u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x1AF4u, 0x1051u, 0x1AF4u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1B36u, 0x5256u)));
  CHECK(!av_gpu.same_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u)));
  CHECK(!av_gpu.same_as(PciIdentity())); }

TEST(pci_identity, the_same_subsystem_whatever_vendor_and_device_it_presents)
{ const PciIdentity av_gpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
  CHECK(av_gpu.same_subsystem_as(PciIdentity(0x1002u, 0x7550u, 0x1AF4u, 0x5256u)));
  CHECK(av_gpu.same_subsystem_as(PciIdentity(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u)));
  CHECK(av_gpu.same_subsystem_as(av_gpu));
  CHECK(!av_gpu.same_subsystem_as(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u)));
  CHECK(!av_gpu.same_subsystem_as(PciIdentity(0x1002u, 0x7550u, 0x1002u, 0x5256u)));
  CHECK(!av_gpu.same_subsystem_as(PciIdentity())); }

TEST(pci_identity, writes_the_hardware_id_windows_gives_the_device)
{ const av::PciIdentity raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u);
  char           bytes[64] = {};
  av::TextBuffer text(bytes, sizeof bytes);
  raphael.append_hardware_id(text);
  CHECK(std::strcmp(bytes, "VEN_1002&DEV_13C0&SUBSYS_52561AF4&REV_C9") == 0);
  CHECK_EQ(raphael.subsystem_vendor(), 0x1AF4u);
  CHECK_EQ(raphael.subsystem(), 0x5256u);
  CHECK_EQ(raphael.revision(), 0xC9u);
  CHECK(raphael.same_as(av::PciIdentity(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u)));
  CHECK_EQ(av::PciIdentity().revision(), 0u); }
