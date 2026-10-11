/* atiadlxx.dll's answers (docs/vendor-libraries.md): the avgpu adapters that
 * present an AMD GPU, as ADL describes a GPU - and exactly what DaVinci
 * Resolve's GPU detection reads: the vendor, the bus, device and function, the
 * PnP string it parses for the hardware ID, the ASIC family it tells an
 * integrated GPU by, and the primary adapter. */
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vendor/adl_root.hpp"
#include "icd/vendor/ddi/adl_adapter_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_x2_record.hpp"
#include "proto/escape/umd_adapter_info.hpp"

#include <cstdint>
#include <cstring>

using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::UmdAdapterInfo;
using av::vendor::AdlAdapterInfoRecord;
using av::vendor::AdlRoot;
using av::vendor::AdlVersionsInfoRecord;
using av::vendor::AdlVersionsInfoX2Record;

namespace {

const PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u);
const PciIdentity on_radeon(0x1002u, 0x7550u, 0x1AF4u, 0x5256u, 0xC0u);
const PciIdentity on_geforce(0x10DEu, 0x2684u, 0x1AF4u, 0x5256u, 0xA1u);
constexpr std::uint64_t version = 0x0000002300050000ull;

/* A machine with an integrated AMD GPU the primary display is on, a discrete
 * one beside it, and an NVIDIA card ADL has nothing to say of. */
class Machine {
public:
    Machine() : kmt_(), trace_(), lock_(trace_, 'A'), root_(kmt_, lock_)
    {
        kmt_.add_adapter(on_raphael, 3200u);
        kmt_.add_adapter(on_geforce, 3200u);
        kmt_.add_adapter(on_radeon, 3200u);
        kmt_.describe_adapter(0u, PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", version);
        kmt_.describe_adapter(1u, PciLocation(0u, 18u, 0u), "NVIDIA GeForce RTX 4090", version);
        kmt_.describe_adapter(2u, PciLocation(5u, 0u, 0u), "AMD Radeon RX 9070 XT", version);
        kmt_.set_adapter_features(0u, UmdAdapterInfo::feature_integrated_gpu);
        kmt_.set_adapter_luid(0u, 0x41u);
        kmt_.set_primary_display_luid(0x41u);
    }

    MockKmt &kmt() { return kmt_; }
    AdlRoot &root() { return root_; }
    bool     unlocked() const { return !lock_.held(); }

private:
    MockKmt                 kmt_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    AdlRoot                 root_;
};

}  // namespace

TEST(adl_root, describes_each_amd_adapter_as_resolve_reads_an_adl_gpu)
{ Machine machine;
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  int count = 0;
  CHECK_EQ(machine.root().number_of_adapters(&count), AdlRoot::ok);
  CHECK_EQ(count, 2);
  AdlAdapterInfoRecord infos[2] = {};
  CHECK_EQ(machine.root().adapter_info(infos, static_cast<int>(sizeof infos)), AdlRoot::ok);
  CHECK_EQ(infos[0].iSize, static_cast<int>(sizeof(AdlAdapterInfoRecord)));
  CHECK_EQ(infos[0].iAdapterIndex, 0);
  CHECK_EQ(infos[0].iVendorID, 0x1002);
  CHECK_EQ(infos[0].iBusNumber, 0);
  CHECK_EQ(infos[0].iDeviceNumber, 17);
  CHECK_EQ(infos[0].iFunctionNumber, 0);
  CHECK_EQ(infos[0].iPresent, 1);
  CHECK_EQ(infos[0].iExist, 1);
  CHECK(std::strcmp(infos[0].strAdapterName, "AMD Radeon(TM) Graphics") == 0);
  CHECK(std::strcmp(infos[0].strPNPString, "PCI\\VEN_1002&DEV_13C0&SUBSYS_52561AF4&REV_C9") == 0);
  CHECK(std::strcmp(infos[0].strUDID, "PCI_VEN_1002&DEV_13C0&SUBSYS_52561AF4&REV_C9") == 0);
  /* No display path and no registry key is named rather than guessed. */
  CHECK_EQ(infos[0].strDisplayName[0], '\0');
  CHECK_EQ(infos[0].strDriverPath[0], '\0');
  CHECK_EQ(infos[0].iOSDisplayIndex, -1);
  CHECK_EQ(infos[1].iAdapterIndex, 1);
  CHECK_EQ(infos[1].iBusNumber, 5);
  CHECK(std::strcmp(infos[1].strPNPString, "PCI\\VEN_1002&DEV_7550&SUBSYS_52561AF4&REV_C0") == 0);
  CHECK(machine.unlocked()); }

TEST(adl_root, the_asic_family_says_integrated_or_discrete_and_the_primary_is_the_display_s)
{ Machine machine;
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  int types = 0;
  int valids = 0;
  CHECK_EQ(machine.root().asic_family_type(0, &types, &valids), AdlRoot::ok);
  CHECK_EQ(types, AdlRoot::asic_integrated);
  CHECK_EQ(valids, AdlRoot::asic_discrete | AdlRoot::asic_integrated);
  CHECK_EQ(machine.root().asic_family_type(1, &types, &valids), AdlRoot::ok);
  CHECK_EQ(types, AdlRoot::asic_discrete);
  CHECK_EQ(machine.root().asic_family_type(2, &types, &valids), AdlRoot::invalid_adapter_index);
  CHECK_EQ(machine.root().asic_family_type(-1, &types, &valids), AdlRoot::invalid_adapter_index);
  CHECK_EQ(machine.root().asic_family_type(0, nullptr, &valids), AdlRoot::null_pointer);
  int primary = -1;
  CHECK_EQ(machine.root().primary(&primary), AdlRoot::ok);
  CHECK_EQ(primary, 0);
  CHECK_EQ(machine.root().primary(nullptr), AdlRoot::null_pointer);
  int active = 0;
  CHECK_EQ(machine.root().active(1, &active), AdlRoot::ok);
  CHECK_EQ(active, 1);
  CHECK_EQ(machine.root().active(7, &active), AdlRoot::invalid_adapter_index);
  CHECK_EQ(machine.root().active(0, nullptr), AdlRoot::null_pointer); }

TEST(adl_root, the_primary_display_on_no_amd_adapter_is_no_primary)
{ Machine machine;
  machine.kmt().set_primary_display_luid(0x99u);
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  int primary = -1;
  CHECK_EQ(machine.root().primary(&primary), AdlRoot::error);
  CHECK_EQ(primary, -1); }

/* The driver package's version, as Windows writes it; the Catalyst and
 * Crimson versions are empty, because this is neither driver. */
TEST(adl_root, the_versions_are_the_driver_package_s_and_no_catalyst_s)
{ Machine machine;
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  AdlVersionsInfoRecord versions;
  std::memset(&versions, 'x', sizeof versions);
  CHECK_EQ(machine.root().versions(&versions), AdlRoot::ok);
  CHECK(std::strcmp(versions.strDriverVer, "0.35.5.0") == 0);
  CHECK_EQ(versions.strCatalystVersion[0], '\0');
  CHECK_EQ(versions.strCatalystWebLink[0], '\0');
  AdlVersionsInfoX2Record versions_x2;
  std::memset(&versions_x2, 'x', sizeof versions_x2);
  CHECK_EQ(machine.root().versions_x2(&versions_x2), AdlRoot::ok);
  CHECK(std::strcmp(versions_x2.strDriverVer, "0.35.5.0") == 0);
  CHECK_EQ(versions_x2.strCrimsonVersion[0], '\0');
  CHECK_EQ(machine.root().versions(nullptr), AdlRoot::null_pointer);
  CHECK_EQ(machine.root().versions_x2(nullptr), AdlRoot::null_pointer);
  /* A refresh that cannot enumerate leaves no adapter, and so no driver to
   * give a version of. */
  machine.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(machine.root().refresh(), AdlRoot::error);
  std::memset(&versions, 'x', sizeof versions);
  CHECK_EQ(machine.root().versions(&versions), AdlRoot::ok);
  CHECK_EQ(versions.strDriverVer[0], '\0'); }

/* An AdapterInfo array too small for every adapter is refused whole, and a
 * missing one is a null pointer. */
TEST(adl_root, adapter_info_into_too_little_room_is_refused)
{ Machine machine;
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  AdlAdapterInfoRecord one = {};
  CHECK_EQ(machine.root().adapter_info(&one, static_cast<int>(sizeof one)), AdlRoot::invalid_parameter_size);
  CHECK_EQ(machine.root().adapter_info(&one, -1), AdlRoot::invalid_parameter_size);
  CHECK_EQ(one.iSize, 0);
  CHECK_EQ(machine.root().adapter_info(nullptr, 4096), AdlRoot::null_pointer);
  CHECK_EQ(machine.root().number_of_adapters(nullptr), AdlRoot::null_pointer);
  int types = 0;
  int valids = 0;
  CHECK_EQ(machine.root().asic_family_type(0, nullptr, &valids), AdlRoot::null_pointer);
  CHECK_EQ(machine.root().asic_family_type(0, &types, nullptr), AdlRoot::null_pointer); }

/* Nothing is answered before a create or after its destroy; creates and
 * destroys pair, ADL's and ADL2's alike, so one caller's destroy does not
 * end another's. */
TEST(adl_root, every_call_waits_for_a_create_and_each_destroy_undoes_one)
{ Machine machine;
  int count = 0;
  int types = 0;
  int valids = 0;
  int primary = 0;
  int active = 0;
  AdlAdapterInfoRecord  info = {};
  AdlVersionsInfoRecord versions = {};
  AdlVersionsInfoX2Record versions_x2 = {};
  CHECK_EQ(machine.root().number_of_adapters(&count), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().adapter_info(&info, static_cast<int>(sizeof info)), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().asic_family_type(0, &types, &valids), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().primary(&primary), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().active(0, &active), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().versions(&versions), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().versions_x2(&versions_x2), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().refresh(), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().create(false), AdlRoot::invalid_callback);
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  CHECK_EQ(machine.root().destroy(), AdlRoot::ok);
  CHECK_EQ(machine.root().number_of_adapters(&count), AdlRoot::ok);
  CHECK_EQ(machine.root().refresh(), AdlRoot::ok);
  CHECK_EQ(machine.root().destroy(), AdlRoot::ok);
  CHECK_EQ(machine.root().number_of_adapters(&count), AdlRoot::not_initialized);
  CHECK_EQ(machine.root().destroy(), AdlRoot::ok);
  CHECK(machine.root().holds(machine.root().context()));
  CHECK(!machine.root().holds(&count));
  CHECK(machine.root().holds(nullptr));   /* ADL1's context */
  CHECK(machine.unlocked()); }

/* Where no AMD GPU is presented ADL will not start, as on any machine with no
 * AMD driver; where the kernel will not enumerate, neither. A refresh that
 * cannot enumerate is an error. */
TEST(adl_root, no_amd_adapter_or_no_enumeration_is_no_adl)
{ MockKmt               kmt;
  av::LockTrace         trace;
  av::RecordingThreadLock lock(trace, 'A');
  AdlRoot               root(kmt, lock);
  kmt.add_adapter(on_geforce, 3200u);
  CHECK_EQ(root.create(true), AdlRoot::error);
  int count = 0;
  CHECK_EQ(root.number_of_adapters(&count), AdlRoot::not_initialized);
  Machine machine;
  machine.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(machine.root().create(true), AdlRoot::error);
  CHECK_EQ(machine.root().create(true), AdlRoot::ok);
  machine.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(machine.root().refresh(), AdlRoot::error); }
