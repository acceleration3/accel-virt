#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_devices.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::ClIcdRig;
using av::FakeClLoader;
using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::Status;
using av::clicd::ClDevices;
using av::clicd::ClIcdRoot;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t integrated_luid = 0x0000000300005678ull;

const PciIdentity on_radeon(0x1002u, 0x7550u, PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem, 0xC0u);
const PciIdentity on_raphael(0x1002u, 0x13C0u, PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem, 0xC9u);

void two_gpus(ClIcdRig &rig)
{
    MockKmt &kmt = rig.kmt();
    kmt.set_identity(0u, on_radeon);
    kmt.describe_adapter(0u, PciLocation(3u, 0u, 0u), "AMD Radeon RX 9070 XT", 0x0023000000000000ull);
    kmt.add_adapter(on_raphael, AdapterScenario::wddm_3_2);
    kmt.set_adapter_luid(1u, integrated_luid);
    kmt.describe_adapter(1u, PciLocation(0x7Bu, 0u, 0u), "AMD Radeon(TM) Graphics", 0x0023000000000000ull);
}

void device_opens(ClIcdRig &rig)
{
    rig.host().blob_import(0u, 0x77u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
}

std::uint64_t luid_of(ClIcdRig &rig, cl_device_id device)
{
    std::uint8_t luid[CL_LUID_SIZE_KHR] = {};
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_LUID_KHR, sizeof luid, luid, nullptr), CL_SUCCESS);
    std::uint64_t value = 0u;
    std::memcpy(&value, luid, sizeof value);
    return value;
}

void name_of(ClIcdRig &rig, cl_device_id device, char (&name)[64])
{
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_NAME, sizeof name, name, nullptr), CL_SUCCESS);
}

void both_devices(ClIcdRig &rig, cl_device_id (&devices)[2])
{
    CHECK(rig.loader().load() == FakeClLoader::Load::kept);
    device_opens(rig);
    device_opens(rig);
    cl_uint count = 0u;
    CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 2u, devices, &count), CL_SUCCESS);
    CHECK_EQ(count, 2u);
}

cl_context context_on(ClIcdRig &rig, cl_device_id device)
{
    rig.host().blob_import(0u, 0x78u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(host_context, CL_SUCCESS);
    cl_int           error = CL_INVALID_VALUE;
    const cl_context made = rig.loader().create_context(device, &error);
    CHECK_EQ(error, CL_SUCCESS);
    return made;
}

}

TEST(cl_devices, a_device_for_each_adapter_the_primary_displays_first)
{ ClIcdRig rig;
  two_gpus(rig);
  rig.kmt().set_primary_display_luid(integrated_luid);
  cl_device_id devices[2] = {};
  both_devices(rig, devices);
  CHECK(devices[0] != nullptr && devices[1] != nullptr && devices[0] != devices[1]);
  CHECK_EQ(luid_of(rig, devices[0]), integrated_luid);
  CHECK_EQ(luid_of(rig, devices[1]), ClIcdRig::adapter_luid);
  char first[64] = {};
  char second[64] = {};
  name_of(rig, devices[0], first);
  name_of(rig, devices[1], second);
  CHECK_EQ(std::strcmp(first, "AMD Radeon(TM) Graphics"), 0);
  CHECK_EQ(std::strcmp(second, "AMD Radeon RX 9070 XT"), 0);
  cl_uint count = 0u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_ALL, 0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 2u);
  cl_device_id one = nullptr;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &one, &count), CL_SUCCESS);
  CHECK(one == devices[0]);
  CHECK_EQ(count, 2u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_devices, the_platform_is_named_after_its_gpu_only_when_it_has_one)
{ { ClIcdRig rig;
    two_gpus(rig);
    CHECK(rig.loader().load() == FakeClLoader::Load::kept);
    char name[96] = {};
    CHECK_EQ(rig.root().platform_info(nullptr, CL_PLATFORM_NAME, sizeof name, name, nullptr), CL_SUCCESS);
    CHECK_EQ(std::strcmp(name, "avgpu OpenCL"), 0); }
  { ClIcdRig rig;
    rig.kmt().describe_adapter(0u, PciLocation(3u, 0u, 0u), "AMD Radeon RX 9070 XT", 0u);
    CHECK(rig.loader().load() == FakeClLoader::Load::kept);
    char name[96] = {};
    CHECK_EQ(rig.root().platform_info(nullptr, CL_PLATFORM_NAME, sizeof name, name, nullptr), CL_SUCCESS);
    CHECK_EQ(std::strcmp(name, "avgpu OpenCL on AMD Radeon RX 9070 XT"), 0); } }

TEST(cl_devices, each_direct3d_adapter_finds_its_own_device)
{ ClIcdRig rig;
  two_gpus(rig);
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = rig.loader().platform(0u);
  int            integrated = 0;
  rig.d3d_adapters().name_adapter(&integrated, integrated_luid);
  device_opens(rig);
  cl_device_id found = nullptr;
  cl_uint      count = 0u;
  CHECK_EQ(rig.root().device_ids_from_d3d11(platform, ClIcdRoot::d3d11_adapter_source, &integrated,
                                             ClIcdRoot::d3d11_preferred_devices, 1u, &found, &count),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(luid_of(rig, found), integrated_luid);
  int discrete = 0;
  rig.d3d_adapters().name_adapter(&discrete, ClIcdRig::adapter_luid);
  device_opens(rig);
  cl_device_id other = nullptr;
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, ClIcdRoot::d3d10_adapter_source, &discrete,
                                             ClIcdRoot::d3d10_all_devices, 1u, &other, &count),
           CL_SUCCESS);
  CHECK(other != nullptr && other != found);
  CHECK_EQ(luid_of(rig, other), ClIcdRig::adapter_luid);
  int stranger = 0;
  rig.d3d_adapters().name_adapter(&stranger, 0x99u);
  CHECK_EQ(rig.root().device_ids_from_d3d11(platform, ClIcdRoot::d3d11_adapter_source, &stranger,
                                             ClIcdRoot::d3d11_preferred_devices, 1u, &other, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u); }

TEST(cl_devices, a_context_and_its_queues_are_its_devices_alone)
{ ClIcdRig rig;
  two_gpus(rig);
  cl_device_id devices[2] = {};
  both_devices(rig, devices);
  const cl_context context = context_on(rig, devices[1]);
  CHECK(context != nullptr);
  cl_device_id named = nullptr;
  CHECK_EQ(rig.root().context_info(context, CL_CONTEXT_DEVICES, sizeof named, &named, nullptr), CL_SUCCESS);
  CHECK(named == devices[1]);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_queue(context, devices[0], &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  const cl_command_queue queue = rig.loader().create_queue(context, devices[1], &error);
  CHECK(queue != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  cl_device_id of_queue = nullptr;
  CHECK_EQ(rig.root().queue_info(queue, CL_QUEUE_DEVICE, sizeof of_queue, &of_queue, nullptr), CL_SUCCESS);
  CHECK(of_queue == devices[1]);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_devices, learns_a_device_for_each_adapter_with_a_luid_once)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, AdapterScenario::wddm_3_2);
  kmt.add_adapter(on_raphael, AdapterScenario::wddm_3_2);
  kmt.set_adapter_luid(0u, 0x22u);
  kmt.set_adapter_luid(1u, 0x33u);
  kmt.set_primary_display_luid(0x33u);
  ClDevices devices(kmt);
  CHECK(devices.invariants_hold());
  { AvgpuAdapters adapters(kmt);
    CHECK_STATUS(adapters.learn(), Status::ok);
    devices.learn(adapters); }
  CHECK(devices.learnt());
  CHECK_EQ(devices.count(), 2u);
  CHECK_EQ(devices.left_out(), 0u);
  CHECK_EQ(devices.at(0u).luid(), 0x33u);
  CHECK_EQ(devices.at(1u).luid(), 0x22u);
  CHECK(devices.on(0x22u) == &devices.at(1u));
  CHECK(devices.on(0x44u) == nullptr);
  CHECK(devices.on(0u) == nullptr);
  CHECK(devices.invariants_hold());
  kmt.add_adapter(on_radeon, AdapterScenario::wddm_3_2);
  { AvgpuAdapters adapters(kmt);
    CHECK_STATUS(adapters.learn(), Status::ok);
    devices.learn(adapters); }
  CHECK_EQ(devices.count(), 2u); }

TEST(cl_devices, an_adapter_without_a_luid_is_left_out)
{ MockKmt kmt;
  kmt.add_adapter(on_radeon, AdapterScenario::wddm_3_2);
  kmt.add_adapter(on_raphael, AdapterScenario::wddm_3_2);
  kmt.add_adapter(on_radeon, AdapterScenario::wddm_3_2);
  kmt.set_adapter_luid(2u, 0x44u);
  kmt.fail_at(MockKmt::Call::adapter_luid, 1u, 0xC0000001u);
  ClDevices devices(kmt);
  { AvgpuAdapters adapters(kmt);
    CHECK_STATUS(adapters.learn(), Status::ok);
    devices.learn(adapters); }
  CHECK_EQ(devices.count(), 2u);
  CHECK_EQ(devices.left_out(), 1u);
  CHECK_EQ(devices.at(1u).luid(), 0x44u);
  CHECK(devices.invariants_hold()); }
