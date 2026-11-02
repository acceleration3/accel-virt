/* nvapi64.dll's answers (docs/vendor-libraries.md): the avgpu adapters that
 * present an NVIDIA GPU, as NVAPI describes a GPU - the calls DaVinci
 * Resolve's GPU detection makes, each answered from what Windows reports. */
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vendor/ddi/nv_display_driver_memory_info_record.hpp"
#include "icd/vendor/nvapi_root.hpp"
#include "proto/escape/umd_adapter_info.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::UmdAdapterInfo;
using av::vendor::NvapiRoot;
using av::vendor::NvDisplayDriverMemoryInfoRecord;

namespace {

const PciIdentity on_geforce(0x10DEu, 0x2684u, 0x1AF4u, 0x5256u, 0xA1u);
const PciIdentity on_laptop(0x10DEu, 0x28E0u, 0x1AF4u, 0x5256u, 0xA1u);
const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u);
constexpr std::uint64_t version = 0x0000002300050000ull;
constexpr std::uint32_t memory_v1 = 20u | (1u << 16u);
constexpr std::uint32_t memory_v2 = 24u | (2u << 16u);
constexpr std::uint32_t memory_v3 = 32u | (3u << 16u);

/* A discrete NVIDIA card the primary display is on, an AMD iGPU NVAPI has
 * nothing to say of, and an integrated NVIDIA GPU with no display. */
class Machine {
public:
    Machine() : kmt_(), trace_(), lock_(trace_, 'N'), root_(kmt_, lock_)
    {
        kmt_.add_adapter(on_geforce, 3200u);
        kmt_.add_adapter(on_raphael, 3200u);
        kmt_.add_adapter(on_laptop, 3200u);
        kmt_.describe_adapter(0u, PciLocation(3u, 0u, 0u), "NVIDIA GeForce RTX 4090", version);
        kmt_.describe_adapter(1u, PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", version);
        kmt_.describe_adapter(2u, PciLocation(0u, 18u, 0u), "NVIDIA GeForce RTX 4060 Laptop GPU", version);
        kmt_.set_adapter_features(2u, UmdAdapterInfo::feature_integrated_gpu);
        kmt_.set_segment_sizes(24ull << 30u, 0u, 16ull << 30u);
        kmt_.set_adapter_luid(0u, 0x0000000500001234ull);
        kmt_.set_primary_display_luid(0x0000000500001234ull);
    }

    MockKmt   &kmt() { return kmt_; }
    NvapiRoot &root() { return root_; }
    /* The handle of the `index`th GPU NVAPI enumerates. */
    void *gpu(std::uint32_t index)
    {
        void         *gpus[NvapiRoot::max_physical_gpus] = {};
        std::uint32_t count = 0u;
        CHECK_EQ(root_.enum_physical_gpus(gpus, &count), NvapiRoot::ok);
        return index < count ? gpus[index] : nullptr;
    }
    bool unlocked() const { return !lock_.held(); }

private:
    MockKmt                 kmt_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    NvapiRoot               root_;
};

}  // namespace

TEST(nvapi_root, enumerates_each_nvidia_gpu_with_what_windows_says_of_it)
{ Machine machine;
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  void         *gpus[NvapiRoot::max_physical_gpus] = {};
  std::uint32_t count = 0u;
  CHECK_EQ(machine.root().enum_physical_gpus(gpus, &count), NvapiRoot::ok);
  CHECK_EQ(count, 2u);
  char name[NvapiRoot::short_string] = {};
  CHECK_EQ(machine.root().full_name(gpus[0], name), NvapiRoot::ok);
  CHECK(std::strcmp(name, "NVIDIA GeForce RTX 4090") == 0);
  std::uint32_t device = 0u, subsystem = 0u, revision = 0u, external = 0u;
  CHECK_EQ(machine.root().pci_identifiers(gpus[0], &device, &subsystem, &revision, &external), NvapiRoot::ok);
  CHECK_EQ(device, 0x268410DEu);
  CHECK_EQ(subsystem, 0x52561AF4u);
  CHECK_EQ(revision, 0xA1u);
  CHECK_EQ(external, 0x268410DEu);
  std::uint32_t bus = 99u, slot = 99u;
  CHECK_EQ(machine.root().bus_id(gpus[0], &bus), NvapiRoot::ok);
  CHECK_EQ(machine.root().bus_slot_id(gpus[0], &slot), NvapiRoot::ok);
  CHECK_EQ(bus, 3u);
  CHECK_EQ(slot, 0u);
  CHECK_EQ(machine.root().bus_slot_id(gpus[1], &slot), NvapiRoot::ok);
  CHECK_EQ(slot, 18u);
  int type = 0;
  CHECK_EQ(machine.root().gpu_type(gpus[0], &type), NvapiRoot::ok);
  CHECK_EQ(type, NvapiRoot::gpu_discrete);
  CHECK_EQ(machine.root().gpu_type(gpus[1], &type), NvapiRoot::ok);
  CHECK_EQ(type, NvapiRoot::gpu_integrated);
  std::uint8_t luid[8] = {};
  CHECK_EQ(machine.root().adapter_id(gpus[0], luid), NvapiRoot::ok);
  const std::uint8_t expected[8] = {0x34u, 0x12u, 0u, 0u, 0x05u, 0u, 0u, 0u};
  CHECK(std::memcmp(luid, expected, sizeof luid) == 0);
  CHECK(machine.unlocked()); }

/* The memory record at each version NVAPI has had, in KiB; a version it never
 * had is refused before anything is written. */
TEST(nvapi_root, memory_info_answers_each_version_and_refuses_another)
{ Machine machine;
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  void *const gpu = machine.gpu(0u);
  NvDisplayDriverMemoryInfoRecord memory = {};
  memory.version = memory_v3;
  CHECK_EQ(machine.root().memory_info(gpu, &memory), NvapiRoot::ok);
  CHECK_EQ(memory.dedicatedVideoMemory, 24u << 20u);
  CHECK_EQ(memory.availableDedicatedVideoMemory, 24u << 20u);
  CHECK_EQ(memory.systemVideoMemory, 0u);
  CHECK_EQ(memory.sharedSystemMemory, 16u << 20u);
  CHECK_EQ(memory.curAvailableDedicatedVideoMemory, 24u << 20u);
  CHECK_EQ(memory.dedicatedVideoMemoryEvictionCount, 0u);
  /* A smaller version's record is no bigger than its fields. */
  NvDisplayDriverMemoryInfoRecord small;
  std::memset(&small, 0xEE, sizeof small);
  small.version = memory_v1;
  CHECK_EQ(machine.root().memory_info(gpu, &small), NvapiRoot::ok);
  CHECK_EQ(small.dedicatedVideoMemory, 24u << 20u);
  CHECK_EQ(small.curAvailableDedicatedVideoMemory, 0xEEEEEEEEu);
  small.version = memory_v2;
  CHECK_EQ(machine.root().memory_info(gpu, &small), NvapiRoot::ok);
  CHECK_EQ(small.curAvailableDedicatedVideoMemory, 24u << 20u);
  CHECK_EQ(small.dedicatedVideoMemoryEvictionsSize, 0xEEEEEEEEu);
  NvDisplayDriverMemoryInfoRecord future = {};
  future.version = 36u | (4u << 16u);
  CHECK_EQ(machine.root().memory_info(gpu, &future), NvapiRoot::incompatible_struct_version);
  CHECK_EQ(future.dedicatedVideoMemory, 0u);
  CHECK_EQ(machine.root().memory_info(gpu, nullptr), NvapiRoot::invalid_argument); }

/* The user-mode driver's version as NVAPI writes a driver's, and the primary
 * display: the one display NVIDIA GPUs drive here, and the GPU driving it. */
TEST(nvapi_root, the_driver_version_and_the_primary_display_with_its_gpu)
{ Machine machine;
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  std::uint32_t driver = 0u;
  char          branch[NvapiRoot::short_string] = {};
  CHECK_EQ(machine.root().driver_and_branch_version(&driver, branch), NvapiRoot::ok);
  CHECK_EQ(driver, 50000u);
  CHECK(std::strcmp(branch, "avgpu 0.35.5.0") == 0);
  void *display = nullptr;
  CHECK_EQ(machine.root().enum_display(0u, &display), NvapiRoot::ok);
  CHECK(display != nullptr);
  void *more = nullptr;
  CHECK_EQ(machine.root().enum_display(1u, &more), NvapiRoot::end_enumeration);
  void         *gpus[NvapiRoot::max_physical_gpus] = {};
  std::uint32_t count = 0u;
  CHECK_EQ(machine.root().gpus_from_display(display, gpus, &count), NvapiRoot::ok);
  CHECK_EQ(count, 1u);
  CHECK(gpus[0] == machine.gpu(0u));
  CHECK_EQ(machine.root().gpus_from_display(machine.gpu(0u), gpus, &count), NvapiRoot::expected_display_handle);
  CHECK_EQ(machine.root().enum_display(0u, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().gpus_from_display(display, nullptr, &count), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().gpus_from_display(display, gpus, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().driver_and_branch_version(nullptr, branch), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().driver_and_branch_version(&driver, nullptr), NvapiRoot::invalid_argument);
  /* A token for the display of the GPU beside it, which drives none - what
   * a stale or forged handle is - names no display. */
  void *const beside = static_cast<char *>(display) + 1;
  count = 7u;
  CHECK_EQ(machine.root().gpus_from_display(beside, gpus, &count), NvapiRoot::expected_display_handle);
  CHECK_EQ(count, 7u); }

/* A handle that is no GPU of this library's - a display's, garbage, null - is
 * refused as NVAPI refuses one, and nothing is written. */
TEST(nvapi_root, a_handle_that_is_no_gpu_is_refused)
{ Machine machine;
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  int           stranger = 0;
  char          name[NvapiRoot::short_string] = {'x', '\0'};
  std::uint32_t value = 7u, other = 7u, third = 7u, fourth = 7u;
  int           type = 7;
  std::uint8_t  luid[8] = {};
  NvDisplayDriverMemoryInfoRecord memory = {};
  memory.version = memory_v3;
  for (void *const gpu : {static_cast<void *>(&stranger), static_cast<void *>(nullptr)}) {
      CHECK_EQ(machine.root().full_name(gpu, name), NvapiRoot::expected_physical_gpu_handle);
      CHECK_EQ(machine.root().pci_identifiers(gpu, &value, &other, &third, &fourth),
               NvapiRoot::expected_physical_gpu_handle);
      CHECK_EQ(machine.root().bus_id(gpu, &value), NvapiRoot::expected_physical_gpu_handle);
      CHECK_EQ(machine.root().bus_slot_id(gpu, &value), NvapiRoot::expected_physical_gpu_handle);
      CHECK_EQ(machine.root().gpu_type(gpu, &type), NvapiRoot::expected_physical_gpu_handle);
      CHECK_EQ(machine.root().memory_info(gpu, &memory), NvapiRoot::expected_physical_gpu_handle);
      CHECK_EQ(machine.root().adapter_id(gpu, luid), NvapiRoot::expected_physical_gpu_handle);
  }
  CHECK(std::strcmp(name, "x") == 0);
  CHECK_EQ(value, 7u);
  CHECK_EQ(type, 7);
  void *const gpu = machine.gpu(0u);
  CHECK_EQ(machine.root().full_name(gpu, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().pci_identifiers(gpu, nullptr, &other, &third, &fourth), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().bus_id(gpu, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().bus_slot_id(gpu, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().gpu_type(gpu, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().adapter_id(gpu, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().enum_physical_gpus(nullptr, &value), NvapiRoot::invalid_argument);
  void *gpus[NvapiRoot::max_physical_gpus] = {};
  CHECK_EQ(machine.root().enum_physical_gpus(gpus, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().pci_identifiers(gpu, &value, nullptr, &third, &fourth), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().pci_identifiers(gpu, &value, &other, nullptr, &fourth), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().pci_identifiers(gpu, &value, &other, &third, nullptr), NvapiRoot::invalid_argument);
  CHECK_EQ(machine.root().memory_info(gpu, nullptr), NvapiRoot::invalid_argument);
  CHECK(machine.unlocked()); }

/* Nothing is answered before NvAPI_Initialize or after the unload that ends
 * it; each initialise pairs with one unload. Where no NVIDIA GPU is presented
 * NVAPI says so, as on any machine without one. */
TEST(nvapi_root, every_call_waits_for_initialisation_and_no_nvidia_gpu_is_not_found)
{ Machine machine;
  void         *gpus[NvapiRoot::max_physical_gpus] = {};
  std::uint32_t count = 0u;
  char          text[NvapiRoot::short_string] = {};
  std::uint32_t value = 0u;
  void         *display = nullptr;
  CHECK_EQ(machine.root().enum_physical_gpus(gpus, &count), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().full_name(&value, text), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().driver_and_branch_version(&value, text), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().enum_display(0u, &display), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().gpus_from_display(&value, gpus, &count), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().unload(), NvapiRoot::not_initialized);
  /* A handle from before an unload is no GPU until the next initialise. */
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  void *const gpu = machine.gpu(0u);
  CHECK_EQ(machine.root().unload(), NvapiRoot::ok);
  std::uint32_t other = 0u, third = 0u, fourth = 0u;
  int           type = 0;
  std::uint8_t  luid[8] = {};
  NvDisplayDriverMemoryInfoRecord memory = {};
  memory.version = memory_v3;
  CHECK_EQ(machine.root().pci_identifiers(gpu, &value, &other, &third, &fourth), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().bus_id(gpu, &value), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().bus_slot_id(gpu, &value), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().gpu_type(gpu, &type), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().memory_info(gpu, &memory), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().adapter_id(gpu, luid), NvapiRoot::not_initialized);
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  CHECK_EQ(machine.root().unload(), NvapiRoot::ok);
  CHECK_EQ(machine.root().enum_physical_gpus(gpus, &count), NvapiRoot::ok);
  CHECK_EQ(machine.root().unload(), NvapiRoot::ok);
  CHECK_EQ(machine.root().enum_physical_gpus(gpus, &count), NvapiRoot::not_initialized);
  MockKmt                 kmt;
  av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'N');
  NvapiRoot               root(kmt, lock);
  kmt.add_adapter(on_raphael, 3200u);
  CHECK_EQ(root.initialize(), NvapiRoot::device_not_found);
  CHECK_EQ(root.enum_physical_gpus(gpus, &count), NvapiRoot::not_initialized);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(root.initialize(), NvapiRoot::error); }

/* With no NVIDIA GPU the primary display is on, there is no display to
 * enumerate. */
TEST(nvapi_root, a_primary_display_on_no_nvidia_gpu_is_no_display)
{ Machine machine;
  machine.kmt().set_primary_display_luid(0x77u);
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  void *display = nullptr;
  CHECK_EQ(machine.root().enum_display(0u, &display), NvapiRoot::end_enumeration);
  CHECK(display == nullptr); }

/* An initialise that cannot see the GPUs any more - the kernel will not
 * enumerate - fails, and leaves the first open over no GPU: each call then
 * answers as on a machine without one, rather than describing what it can no
 * longer see. */
TEST(nvapi_root, a_second_initialise_that_finds_no_gpu_leaves_none_to_describe)
{ Machine machine;
  CHECK_EQ(machine.root().initialize(), NvapiRoot::ok);
  machine.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(machine.root().initialize(), NvapiRoot::error);
  std::uint32_t driver = 7u;
  char          branch[NvapiRoot::short_string] = {'x', '\0'};
  CHECK_EQ(machine.root().driver_and_branch_version(&driver, branch), NvapiRoot::device_not_found);
  CHECK_EQ(driver, 7u);
  CHECK(std::strcmp(branch, "x") == 0);
  void         *gpus[NvapiRoot::max_physical_gpus] = {};
  std::uint32_t count = 7u;
  CHECK_EQ(machine.root().enum_physical_gpus(gpus, &count), NvapiRoot::ok);
  CHECK_EQ(count, 0u);
  void *display = nullptr;
  CHECK_EQ(machine.root().enum_display(0u, &display), NvapiRoot::end_enumeration);
  CHECK(machine.unlocked()); }
