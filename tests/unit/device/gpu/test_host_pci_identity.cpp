#include "device/abi/av_device.h"
#include "device/gpu/host_pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/gpu_identity.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::GpuIdentity;
using av::HostPciIdentity;
using av::MockHost;

namespace {

constexpr std::uint8_t discrete[GpuIdentity::uuid_bytes] = {3};
constexpr std::uint8_t integrated[GpuIdentity::uuid_bytes] = {0x7b};
constexpr char         vendor_path[] = "/sys/bus/pci/devices/0000:03:00.0/vendor";
constexpr char         device_path[] = "/sys/bus/pci/devices/0000:03:00.0/device";
constexpr char         revision_path[] = "/sys/bus/pci/devices/0000:03:00.0/revision";

void add_text(MockHost &host, const char *path, const char *text)
{
    host.add_file(path, text, std::strlen(text));
}

void add_radeon(MockHost &host)
{
    host.add_gpu(discrete);
    host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 0x03u, 0u, 0u});
    add_text(host, vendor_path, "0x1002\n");
    add_text(host, device_path, "0x7550\n");
    add_text(host, revision_path, "0xc0\n");
}

bool presents_vendor(const char *vendor, std::uint16_t &presented)
{
    MockHost host;
    host.add_gpu(discrete);
    host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 0x03u, 0u, 0u});
    add_text(host, vendor_path, vendor);
    add_text(host, device_path, "0x7550\n");
    add_text(host, revision_path, "0xc0\n");
    HostPciIdentity identity(host);
    identity.read(0u);
    AvPciIdentity answer = {};
    const bool present = identity.presented(answer);
    presented = answer.vendor_id;
    return present;
}

}

TEST(host_pci_identity, presents_the_bound_gpus_vendor_device_and_revision_and_says_so)
{ MockHost host;
  add_radeon(host);
  HostPciIdentity identity(host);
  AvPciIdentity answer = {0xAAAAu, 0xBBBBu, 0xCCu};
  CHECK(!identity.presented(answer));
  CHECK_EQ(answer.vendor_id, 0xAAAAu);
  identity.read(0u);
  CHECK(identity.presented(answer));
  CHECK_EQ(answer.vendor_id, 0x1002u);
  CHECK_EQ(answer.device_id, 0x7550u);
  CHECK_EQ(answer.revision, 0xC0u);
  CHECK_EQ(host.logged(), 1u);
  CHECK(std::strcmp(host.last_log(), "virtio-gpu-av: pci identity 1002:7550 rev c0, the host's 0000:03:00.0") == 0); }

TEST(host_pci_identity, a_gpu_with_no_pci_function_presents_nothing)
{ MockHost host;
  add_radeon(host);
  host.add_gpu(GpuIdentity(0x10005u, 0u, GpuIdentity::cpu, integrated, 0u, 1u << 30, "llvmpipe (LLVM 19)"));
  HostPciIdentity identity(host);
  identity.read(0u);
  identity.read(1u);
  AvPciIdentity answer = {0xAAAAu, 0xBBBBu, 0xCCu};
  CHECK(!identity.presented(answer));
  CHECK_EQ(answer.vendor_id, 0xAAAAu);
  CHECK_EQ(answer.device_id, 0xBBBBu);
  CHECK_EQ(answer.revision, 0xCCu);
  CHECK_EQ(host.logged(), 1u); }

TEST(host_pci_identity, a_function_sysfs_does_not_fully_describe_presents_nothing)
{ for (const char *missing : {vendor_path, device_path, revision_path}) {
    MockHost host;
    host.add_gpu(discrete);
    host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 0x03u, 0u, 0u});
    if (std::strcmp(missing, vendor_path) != 0) add_text(host, vendor_path, "0x1002\n");
    if (std::strcmp(missing, device_path) != 0) add_text(host, device_path, "0x7550\n");
    if (std::strcmp(missing, revision_path) != 0) add_text(host, revision_path, "0xc0\n");
    HostPciIdentity identity(host);
    identity.read(0u);
    AvPciIdentity answer = {};
    CHECK(!identity.presented(answer));
    CHECK_EQ(host.logged(), 0u); } }

TEST(host_pci_identity, reads_sysfs_hex_in_either_case_with_trailing_whitespace)
{ std::uint16_t vendor = 0u;
  CHECK(presents_vendor("0x10de\n", vendor));
  CHECK_EQ(vendor, 0x10DEu);
  CHECK(presents_vendor("0X10DE \t\n", vendor));
  CHECK_EQ(vendor, 0x10DEu);
  CHECK(presents_vendor("0x8086", vendor));
  CHECK_EQ(vendor, 0x8086u);
  CHECK(presents_vendor("0xffff\n", vendor));
  CHECK_EQ(vendor, 0xFFFFu); }

TEST(host_pci_identity, refuses_text_that_is_not_one_hex_number_in_range)
{ std::uint16_t vendor = 0u;
  for (const char *hostile : {"1002\n", "0x\n", "0x", "x1002", "01002", "0y1002", "0x10000\n", "0x1002z\n",
                              "0x10 02\n", "0x-1\n", "0xg\n", ""})
      CHECK(!presents_vendor(hostile, vendor));
  MockHost host;
  host.add_gpu(discrete);
  host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 0x03u, 0u, 0u});
  add_text(host, vendor_path, "0x1002\n");
  add_text(host, device_path, "0x7550\n");
  add_text(host, revision_path, "0x100\n");
  HostPciIdentity identity(host);
  identity.read(0u);
  AvPciIdentity answer = {};
  CHECK(!identity.presented(answer)); }

TEST(host_pci_identity, a_longer_file_is_judged_on_what_fits)
{ std::uint16_t vendor = 0u;
  CHECK(presents_vendor("0x1002         \nXXXXXXXX", vendor));
  CHECK_EQ(vendor, 0x1002u);
  CHECK(!presents_vendor("0x1002         garbage", vendor)); }
