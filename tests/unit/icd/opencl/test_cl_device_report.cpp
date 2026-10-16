/* The device queries this ICD answers itself rather than forwarding.
 *
 * Four of them cannot be forwarded. The vendor ID and the LUID name an
 * adapter, and the host's name the host's own GPU, which the guest has no
 * handle to and cannot share memory with. The versions say what an
 * application may call, and the host's say what the host implements rather
 * than what this ICD carries. The SVM capabilities say what a kernel may
 * dereference, and reporting a grain this feature does not implement is an
 * application dereferencing a host pointer in the guest.
 *
 * The host's answers arrive as bytes with a length, from a device this ICD
 * does not control, so what happens to a version cut short or shaped like
 * nothing the specification fixes is as much of the point as the capping. */
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/adapter_facts.hpp"
#include "icd/opencl/cl_device_report.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

using av::Status;
using av::clicd::ClDeviceReport;

namespace av::clicd {

/* What the class never builds, and what its invariants are there to catch:
 * state that no observation of a host could have produced. */
struct ClDeviceReportProbe {
    static void report_version(ClDeviceReport &report, uint32_t major, uint32_t minor)
    {
        report.major_ = major;
        report.minor_ = minor;
    }

    static void report_c_version(ClDeviceReport &report, uint32_t major, uint32_t minor)
    {
        report.c_major_ = major;
        report.c_minor_ = minor;
    }

    static void run_version_past_its_room(ClDeviceReport &report)
    {
        for (size_t index = 0u; index < sizeof report.version_; ++index) report.version_[index] = 'x';
    }

    static void empty_the_c_version(ClDeviceReport &report) { report.c_version_[0] = '\0'; }

    static void report_svm(ClDeviceReport &report, cl_device_svm_capabilities svm)
    {
        report.svm_ = svm;
    }
};

}  // namespace av::clicd

namespace {

/* The LUID of an adapter that exists: low word and high word both set, so a
 * byte order that is wrong shows up rather than reading the same either way. */
constexpr uint64_t adapter_luid = 0x0000000100000ABCull;

/* What a caller does: ask for the size, then for the bytes. */
bool reads_back(const ClDeviceReport &report, cl_device_info name, char *into, std::size_t room)
{
    std::size_t bytes = 0u;
    if (report.info(name, 0u, nullptr, &bytes) != CL_SUCCESS) return false;
    if (bytes == 0u || bytes > room) return false;
    if (report.info(name, bytes, into, nullptr) != CL_SUCCESS) return false;
    return std::strlen(into) + 1u == bytes;
}

}  // namespace

TEST(cl_device_report, the_vendor_id_is_the_avgpu_adapters_rather_than_the_host_gpus)
{ ClDeviceReport report(adapter_luid);
  cl_uint vendor = 0u;
  std::size_t bytes = 0u;
  CHECK_EQ(report.info(CL_DEVICE_VENDOR_ID, sizeof vendor, &vendor, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof(cl_uint));
  /* Red Hat's, which is the PCI vendor of the virtio function avgpu drives;
   * the host GPU's would name a device the guest has no handle to. */
  CHECK_EQ(vendor, 0x1AF4u);
  CHECK(report.decides(CL_DEVICE_VENDOR_ID));
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, the_luid_is_the_adapters_own_bytes_the_way_windows_reads_them)
{ ClDeviceReport report(adapter_luid);
  cl_uchar luid[CL_LUID_SIZE_KHR] = {};
  std::size_t bytes = 0u;
  CHECK_EQ(report.info(CL_DEVICE_LUID_KHR, sizeof luid, luid, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, CL_LUID_SIZE_KHR);
  /* LowPart first, then HighPart: the bytes of the 64-bit value little-endian,
   * which is what an application memcmps against a DXGI adapter's LUID. */
  CHECK_EQ(luid[0], 0xBCu);
  CHECK_EQ(luid[1], 0x0Au);
  CHECK_EQ(luid[2], 0x00u);
  CHECK_EQ(luid[3], 0x00u);
  CHECK_EQ(luid[4], 0x01u);
  CHECK_EQ(luid[7], 0x00u);
  /* An adapter that was found has a LUID worth matching against. */
  cl_bool valid = CL_FALSE;
  CHECK_EQ(report.info(CL_DEVICE_LUID_VALID_KHR, sizeof valid, &valid, nullptr), CL_SUCCESS);
  CHECK_EQ(valid, CL_TRUE); }

TEST(cl_device_report, a_luid_of_zero_is_reported_as_one_that_means_nothing)
{ /* Kmt::adapter_luid leaves zero when it found no adapter, and zero is no
   * adapter's LUID; an application told it was valid would match this device
   * against the first zero it meets. */
  ClDeviceReport report(0u);
  cl_bool valid = CL_TRUE;
  CHECK_EQ(report.info(CL_DEVICE_LUID_VALID_KHR, sizeof valid, &valid, nullptr), CL_SUCCESS);
  CHECK_EQ(valid, CL_FALSE);
  /* The bytes are still answered, and they are zeros: refusing the query would
   * be a failure where the answer is simply "no LUID". */
  cl_uchar luid[CL_LUID_SIZE_KHR] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  CHECK_EQ(report.info(CL_DEVICE_LUID_KHR, sizeof luid, luid, nullptr), CL_SUCCESS);
  CHECK_EQ(luid[0], 0u);
  CHECK_EQ(luid[7], 0u);
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, it_names_the_adapter_it_is_on_and_no_other)
{ ClDeviceReport report(adapter_luid);
  CHECK(report.names_adapter(adapter_luid));
  CHECK(!report.names_adapter(adapter_luid + 1u));
  CHECK(!report.names_adapter(0u));
  /* A report with no adapter names none - not even an application's zero,
   * which would otherwise be the one LUID every adapterless report matched. */
  ClDeviceReport unplaced(0u);
  CHECK(!unplaced.names_adapter(0u));
  unplaced.observe_luid(adapter_luid);
  CHECK(unplaced.names_adapter(adapter_luid)); }

TEST(cl_device_report, until_a_host_is_asked_the_report_is_this_icds_own_version)
{ ClDeviceReport report(adapter_luid);
  char text[64] = {};
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0 ", 11u), 0);
  CHECK(reads_back(report, CL_DEVICE_OPENCL_C_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL C 3.0 ", 13u), 0);
  /* And nothing is claimed about SVM before a host has said anything: a
   * capability reported is a capability a kernel will use. */
  cl_device_svm_capabilities svm = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
  CHECK_EQ(report.info(CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, nullptr), CL_SUCCESS);
  CHECK_EQ(svm, 0); }

TEST(cl_device_report, a_host_newer_than_this_icd_is_capped_at_what_this_icd_implements)
{ ClDeviceReport report(adapter_luid);
  char text[64] = {};
  /* The host is an OpenCL 3.5 runtime; what is reported is what is tested. */
  CHECK_STATUS(report.observe_version("OpenCL 3.5 CUDA 12.4", sizeof "OpenCL 3.5 CUDA 12.4"), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0 ", 11u), 0);
  cl_version numeric = 0u;
  CHECK_EQ(report.info(CL_DEVICE_NUMERIC_VERSION, sizeof numeric, &numeric, nullptr), CL_SUCCESS);
  CHECK_EQ(CL_VERSION_MAJOR(numeric), 3u);
  CHECK_EQ(CL_VERSION_MINOR(numeric), 0u);
  /* A major above the cap is capped whatever its minor says. */
  CHECK_STATUS(report.observe_version("OpenCL 9.9 ", sizeof "OpenCL 9.9 "), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0 ", 11u), 0);
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, a_host_older_than_this_icd_is_reported_as_it_is)
{ ClDeviceReport report(adapter_luid);
  char text[64] = {};
  /* An OpenCL 1.2 host: reporting 3.0 over it would promise an application
   * calls the host would refuse. */
  CHECK_STATUS(report.observe_version("OpenCL 1.2 pocl", sizeof "OpenCL 1.2 pocl"), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 1.2 ", 11u), 0);
  cl_version numeric = 0u;
  CHECK_EQ(report.info(CL_DEVICE_NUMERIC_VERSION, sizeof numeric, &numeric, nullptr), CL_SUCCESS);
  CHECK_EQ(CL_VERSION_MAJOR(numeric), 1u);
  CHECK_EQ(CL_VERSION_MINOR(numeric), 2u);
  /* The version at the cap itself is neither raised nor lowered. */
  CHECK_STATUS(report.observe_version("OpenCL 3.0 Mesa", sizeof "OpenCL 3.0 Mesa"), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0 ", 11u), 0);
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, the_language_version_is_the_hosts_own_and_not_the_devices)
{ ClDeviceReport report(adapter_luid);
  char text[64] = {};
  /* An OpenCL 3.0 device whose compiler is OpenCL C 1.2 is the common case,
   * and it is the compiler's answer that says what a kernel may be written
   * in. */
  CHECK_STATUS(report.observe_version("OpenCL 3.0 Mesa", sizeof "OpenCL 3.0 Mesa"), Status::ok);
  CHECK_STATUS(report.observe_c_version("OpenCL C 1.2 Mesa", sizeof "OpenCL C 1.2 Mesa"), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_OPENCL_C_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL C 1.2 ", 13u), 0);
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0 ", 11u), 0);
  /* And it is capped by the same rule the device version is. */
  CHECK_STATUS(report.observe_c_version("OpenCL C 3.5 CUDA", sizeof "OpenCL C 3.5 CUDA"), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_OPENCL_C_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL C 3.0 ", 13u), 0);
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, a_host_version_cut_short_is_refused_rather_than_read_as_an_answer)
{ ClDeviceReport report(adapter_luid);
  char text[64] = {};
  CHECK_STATUS(report.observe_version("OpenCL 1.1 pocl", sizeof "OpenCL 1.1 pocl"), Status::ok);
  /* The host's string with no terminator inside the bytes the host gave: the
   * digits that follow are somebody else's memory. */
  const char cut[] = "OpenCL 2.0 whatever";
  CHECK_STATUS(report.observe_version(cut, 10u), Status::malformed);
  CHECK_STATUS(report.observe_c_version("OpenCL C 2.0 whatever", 12u), Status::malformed);
  /* And the answer that was good before is the answer still. */
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 1.1 ", 11u), 0);
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, a_host_version_shaped_like_nothing_the_specification_fixes_is_refused)
{ ClDeviceReport report(adapter_luid);
  /* Every one of these is a string a host could return and none of them is a
   * version this could cap, so each leaves the report as it was rather than
   * guessing a number out of it. */
  CHECK_STATUS(report.observe_version(nullptr, 16u), Status::malformed);
  CHECK_STATUS(report.observe_version("", 1u), Status::malformed);
  CHECK_STATUS(report.observe_version("CUDA 11.4", sizeof "CUDA 11.4"), Status::malformed);
  CHECK_STATUS(report.observe_version("OpenCL", sizeof "OpenCL"), Status::malformed);
  CHECK_STATUS(report.observe_version("OpenCL 3.", sizeof "OpenCL 3."), Status::malformed);
  CHECK_STATUS(report.observe_version("OpenCL x.y z", sizeof "OpenCL x.y z"), Status::malformed);
  CHECK_STATUS(report.observe_version("OpenCL 3-0 Mesa", sizeof "OpenCL 3-0 Mesa"), Status::malformed);
  CHECK_STATUS(report.observe_version("OpenCL 3.x Mesa", sizeof "OpenCL 3.x Mesa"), Status::malformed);
  /* Two digits either side is no OpenCL release, and reading the first of
   * them would turn 12.0 into 1.0 and 3.25 into 3.2. */
  CHECK_STATUS(report.observe_version("OpenCL 12.0 Mesa", sizeof "OpenCL 12.0 Mesa"), Status::malformed);
  CHECK_STATUS(report.observe_version("OpenCL 3.25 Mesa", sizeof "OpenCL 3.25 Mesa"), Status::malformed);
  /* The language version is read by the same rule, prefix and all. */
  CHECK_STATUS(report.observe_c_version("OpenCL 1.2 Mesa", sizeof "OpenCL 1.2 Mesa"), Status::malformed);
  char text[64] = {};
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 3.0 ", 11u), 0);
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, a_version_with_nothing_after_the_number_is_read_all_the_same)
{ ClDeviceReport report(adapter_luid);
  char text[64] = {};
  /* The vendor's half of the string may be empty, and a host that leaves it
   * out is still reporting a version. */
  CHECK_STATUS(report.observe_version("OpenCL 1.2", sizeof "OpenCL 1.2"), Status::ok);
  CHECK(reads_back(report, CL_DEVICE_VERSION, text, sizeof text));
  CHECK_EQ(std::strncmp(text, "OpenCL 1.2 ", 11u), 0);
  /* And what this ICD reports names itself, so a log says which driver
   * answered. */
  CHECK(std::strstr(text, "avgpu") != nullptr); }

TEST(cl_device_report, svm_is_coarse_grained_or_nothing_whatever_the_host_offers)
{ ClDeviceReport report(adapter_luid);
  cl_device_svm_capabilities svm = 0u;
  /* A host with everything: feature 052 implements the coarse-grained buffer
   * and nothing else, and a fine-grained report would have a kernel
   * dereference a host address the guest never mirrored. */
  report.observe_svm(CL_DEVICE_SVM_COARSE_GRAIN_BUFFER | CL_DEVICE_SVM_FINE_GRAIN_BUFFER |
                     CL_DEVICE_SVM_FINE_GRAIN_SYSTEM | CL_DEVICE_SVM_ATOMICS);
  CHECK_EQ(report.info(CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, nullptr), CL_SUCCESS);
  CHECK_EQ(svm, CL_DEVICE_SVM_COARSE_GRAIN_BUFFER);
  CHECK(report.invariants_hold());
  /* A host with fine-grained SVM and no coarse-grained buffer has nothing
   * this can carry, so nothing is reported. */
  report.observe_svm(CL_DEVICE_SVM_FINE_GRAIN_SYSTEM);
  std::size_t bytes = 0u;
  CHECK_EQ(report.info(CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, &bytes), CL_SUCCESS);
  CHECK_EQ(svm, 0);
  CHECK_EQ(bytes, sizeof(cl_device_svm_capabilities));
  CHECK(report.invariants_hold()); }

TEST(cl_device_report, the_applications_memory_is_never_the_devices_whatever_the_gpu)
{ ClDeviceReport report(adapter_luid);
  /* An integrated GPU's host says yes, and an application told yes reads a
   * kernel's results out of its own memory without a map - memory the guest
   * holds and no host GPU writes. So it is told no, and maps or reads. */
  cl_bool unified = CL_TRUE;
  std::size_t bytes = 0u;
  CHECK(report.decides(CL_DEVICE_HOST_UNIFIED_MEMORY));
  CHECK_EQ(report.info(CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof unified, &unified, &bytes), CL_SUCCESS);
  CHECK_EQ(unified, static_cast<cl_bool>(CL_FALSE));
  CHECK_EQ(bytes, sizeof(cl_bool)); }

TEST(cl_device_report, every_other_device_query_is_left_to_the_host)
{ ClDeviceReport report(adapter_luid);
  /* The host's own answers describe the device that will run the work, and
   * this has nothing truer to say about any of them. */
  CHECK(!report.decides(CL_DEVICE_NAME));
  CHECK(!report.decides(CL_DEVICE_VENDOR));
  CHECK(!report.decides(CL_DEVICE_MAX_COMPUTE_UNITS));
  CHECK(!report.decides(CL_DRIVER_VERSION));
  /* Including the extensions, which are ClExtensionPolicy's answer rather
   * than either this report's or the host's. */
  CHECK(!report.decides(CL_DEVICE_EXTENSIONS));
  std::size_t bytes = 1u;
  CHECK_EQ(report.info(CL_DEVICE_NAME, 0u, nullptr, &bytes), CL_INVALID_VALUE);
  /* And nothing is reported about a value it did not answer. */
  CHECK_EQ(bytes, 1u); }

TEST(cl_device_report, what_it_decides_is_exactly_what_it_answers)
{ ClDeviceReport report(adapter_luid);
  const cl_device_info decided[] = {CL_DEVICE_VENDOR_ID,        CL_DEVICE_LUID_KHR,
                                    CL_DEVICE_LUID_VALID_KHR,   CL_DEVICE_VERSION,
                                    CL_DEVICE_OPENCL_C_VERSION, CL_DEVICE_NUMERIC_VERSION,
                                    CL_DEVICE_SVM_CAPABILITIES, CL_DEVICE_HOST_UNIFIED_MEMORY};
  for (const cl_device_info name : decided) {
    CHECK(report.decides(name));
    std::size_t bytes = 0u;
    /* A caller that trusts decides() must get an answer out of info(), or the
     * query falls between the two and the application is told nothing. */
    CHECK_EQ(report.info(name, 0u, nullptr, &bytes), CL_SUCCESS);
    CHECK(bytes > 0u);
  } }

TEST(cl_device_report, a_buffer_too_small_is_refused_rather_than_filled_as_far_as_it_goes)
{ ClDeviceReport report(adapter_luid);
  char text[4] = {'z', 'z', 'z', 'z'};
  CHECK_EQ(report.info(CL_DEVICE_VERSION, sizeof text, text, nullptr), CL_INVALID_VALUE);
  /* Nothing was written: a caller that shrank its buffer is told, not handed
   * a string cut where it happened to fit. */
  CHECK_EQ(text[0], 'z');
  /* The fixed-width answers are refused by the same rule, and a LUID cut to
   * four bytes would match some other adapter. */
  cl_uchar luid[CL_LUID_SIZE_KHR] = {9u, 9u, 9u, 9u, 9u, 9u, 9u, 9u};
  CHECK_EQ(report.info(CL_DEVICE_LUID_KHR, 4u, luid, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(luid[0], 9u);
  cl_uint vendor = 7u;
  CHECK_EQ(report.info(CL_DEVICE_VENDOR_ID, 1u, &vendor, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(vendor, 7u); }

TEST(cl_device_report, a_size_may_be_asked_for_with_no_buffer_at_all)
{ ClDeviceReport report(adapter_luid);
  std::size_t bytes = 0u;
  CHECK_EQ(report.info(CL_DEVICE_VERSION, 0u, nullptr, &bytes), CL_SUCCESS);
  CHECK(bytes > 1u);
  /* A buffer with no size ret is the other half of the same call. */
  char text[64] = {};
  CHECK_EQ(report.info(CL_DEVICE_VERSION, sizeof text, text, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strlen(text) + 1u, bytes); }

TEST(cl_device_report, a_report_naming_a_version_it_does_not_implement_breaks_its_invariant)
{ ClDeviceReport report(adapter_luid);
  CHECK(report.invariants_hold());
  av::clicd::ClDeviceReportProbe::report_version(report, 3u, 1u);
  CHECK(!report.invariants_hold());
  av::clicd::ClDeviceReportProbe::report_version(report, 4u, 0u);
  CHECK(!report.invariants_hold());
  av::clicd::ClDeviceReportProbe::report_version(report, 1u, 2u);
  CHECK(report.invariants_hold());
  /* The language version is held to the same ceiling: OpenCL C 3.5 is a
   * language nothing here compiles. */
  av::clicd::ClDeviceReportProbe::report_c_version(report, 3u, 5u);
  CHECK(!report.invariants_hold()); }

TEST(cl_device_report, a_report_whose_version_string_lost_its_terminator_breaks_its_invariant)
{ ClDeviceReport report(adapter_luid);
  av::clicd::ClDeviceReportProbe::run_version_past_its_room(report);
  /* A string with no terminator in its room is one info() would read past. */
  CHECK(!report.invariants_hold()); }

TEST(cl_device_report, a_report_with_no_language_version_at_all_breaks_its_invariant)
{ ClDeviceReport report(adapter_luid);
  av::clicd::ClDeviceReportProbe::empty_the_c_version(report);
  /* An empty answer is not one an application can parse a version out of. */
  CHECK(!report.invariants_hold()); }

TEST(cl_device_report, a_report_offering_a_grain_of_svm_this_feature_lacks_breaks_its_invariant)
{ ClDeviceReport report(adapter_luid);
  av::clicd::ClDeviceReportProbe::report_svm(report, CL_DEVICE_SVM_FINE_GRAIN_BUFFER);
  CHECK(!report.invariants_hold());
  av::clicd::ClDeviceReportProbe::report_svm(report, CL_DEVICE_SVM_COARSE_GRAIN_BUFFER);
  CHECK(report.invariants_hold()); }

/* The version a host answers is read only if it is shaped like one: a prefix,
 * a digit, a dot, a digit. Anything else is a string this driver does not
 * believe, and believing it would have the ICD report a version it cannot
 * serve. */
TEST(cl_device_report, a_version_whose_digits_are_not_digits_is_not_believed)
{ const char *const wrong[] = {
      "OpenCL x.0 host",     /* a letter where the major belongs */
      "OpenCL 3:0 host",     /* the separator is not a dot */
      "OpenCL 3.x host",     /* a letter where the minor belongs */
      "OpenCL -.0 host",     /* one below '0', which is the lower half of the
                              * range check */
      "OpenCL 3.: host",     /* one above '9', which is the upper half */
  };
  for (const char *const text : wrong) {
      ClDeviceReport report(adapter_luid);
      CHECK_STATUS(report.observe_version(text, std::strlen(text) + 1u), Status::malformed);
      /* Unchanged: what this ICD implements is what it goes on reporting. */
      cl_version numeric = 0u;
      CHECK_EQ(report.info(CL_DEVICE_NUMERIC_VERSION, sizeof numeric, &numeric, nullptr),
               CL_SUCCESS);
      CHECK_EQ(CL_VERSION_MAJOR(numeric), 3u);
      CHECK(report.invariants_hold());
  } }

namespace {

const av::PciIdentity on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u);
const av::PciIdentity on_geforce(0x10DEu, 0x2684u, 0x1AF4u, 0x5256u, 0xA1u);
constexpr const char *radeon = "AMD Radeon";

/* What Windows says of an adapter presenting `identity` at `location`, named
 * `name` - no name at all when null. */
av::icd::AdapterFacts adapter(const av::PciIdentity &identity, const av::PciLocation &location, const char *name)
{
    av::MockKmt kmt;
    kmt.add_adapter(identity, 3200u);
    kmt.describe_adapter(0u, location, name == nullptr ? "" : name, 0u);
    std::uint32_t handles[4] = {};
    std::uint32_t found = 0u;
    static_cast<void>(kmt.enumerate_adapters(handles, 4u, found));
    if (name == nullptr) kmt.fail_at(av::MockKmt::Call::query_adapter_name, 1u, av::MockKmt::invalid_parameter);
    av::icd::AdapterFacts facts;
    facts.learn(kmt, handles[0]);
    return facts;
}

}  // namespace

/* Described by Windows, the device is the guest's adapter: its vendor (the
 * host GPU's, which avgpu presents), the name Device Manager shows, and where
 * it sits on the guest's PCI bus - not the host's (cl_khr_pci_bus_info). Before
 * that, the name is the host's to answer and no location is said. */
TEST(cl_device_report, the_pci_location_and_name_are_the_guest_adapter_s)
{ ClDeviceReport report(adapter_luid);
  CHECK(!report.decides(CL_DEVICE_NAME));
  CHECK(!report.decides(CL_DEVICE_PCI_BUS_INFO_KHR));
  report.observe_adapter(adapter(on_raphael, av::PciLocation(0u, 17u, 0u), radeon));
  cl_uint vendor = 0u;
  CHECK_EQ(report.info(CL_DEVICE_VENDOR_ID, sizeof vendor, &vendor, nullptr), CL_SUCCESS);
  CHECK_EQ(vendor, 0x1002u);
  char        name[64] = {};
  std::size_t bytes = 0u;
  CHECK_EQ(report.info(CL_DEVICE_NAME, sizeof name, name, &bytes), CL_SUCCESS);
  CHECK_EQ(std::strcmp(name, "AMD Radeon"), 0);
  CHECK_EQ(bytes, sizeof "AMD Radeon");
  char small[4] = {'x'};
  CHECK_EQ(report.info(CL_DEVICE_NAME, sizeof small, small, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(small[0], 'x');
  cl_device_pci_bus_info_khr where = {9u, 9u, 9u, 9u};
  CHECK_EQ(report.info(CL_DEVICE_PCI_BUS_INFO_KHR, sizeof where, &where, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof where);
  CHECK_EQ(where.pci_domain, 0u);
  CHECK_EQ(where.pci_bus, 0u);
  CHECK_EQ(where.pci_device, 17u);
  CHECK_EQ(where.pci_function, 0u);
  /* An adapter with no name leaves the name to the host. */
  ClDeviceReport unnamed(adapter_luid);
  unnamed.observe_adapter(adapter(on_raphael, av::PciLocation(0u, 17u, 0u), nullptr));
  CHECK(!unnamed.decides(CL_DEVICE_NAME));
  CHECK(report.invariants_hold()); }

/* AMD's attribute query on an AMD adapter, as AMD's own driver answers it:
 * the PCIe topology (type 1, then bus, device and function at bytes 21 to 23)
 * and the board's name. Not on another vendor's adapter. */
TEST(cl_device_report, an_amd_adapter_says_where_it_sits_as_amd_s_driver_does)
{ ClDeviceReport report(adapter_luid);
  CHECK(!report.decides(CL_DEVICE_TOPOLOGY_AMD));
  report.observe_adapter(adapter(on_raphael, av::PciLocation(3u, 17u, 2u), radeon));
  std::uint8_t topology[24] = {};
  std::size_t  bytes = 0u;
  CHECK_EQ(report.info(CL_DEVICE_TOPOLOGY_AMD, sizeof topology, topology, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof topology);
  CHECK_EQ(topology[0], 1u);
  CHECK_EQ(topology[1], 0u);
  CHECK_EQ(topology[21], 3u);
  CHECK_EQ(topology[22], 17u);
  CHECK_EQ(topology[23], 2u);
  char board[64] = {};
  CHECK_EQ(report.info(CL_DEVICE_BOARD_NAME_AMD, sizeof board, board, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strcmp(board, "AMD Radeon"), 0);
  ClDeviceReport nvidia(adapter_luid);
  nvidia.observe_adapter(adapter(on_geforce, av::PciLocation(3u, 0u, 0u), radeon));
  CHECK(!nvidia.decides(CL_DEVICE_TOPOLOGY_AMD));
  CHECK(!nvidia.decides(CL_DEVICE_BOARD_NAME_AMD)); }

/* NVIDIA's attribute query on an NVIDIA adapter: the bus, the slot - the PCI
 * device number - and the domain, which the guest has one of. Not on another
 * vendor's adapter. */
TEST(cl_device_report, an_nvidia_adapter_says_where_it_sits_as_nvidia_s_driver_does)
{ constexpr cl_device_info bus_nv = 0x4008u;
  constexpr cl_device_info slot_nv = 0x4009u;
  constexpr cl_device_info domain_nv = 0x400Au;
  ClDeviceReport report(adapter_luid);
  CHECK(!report.decides(bus_nv));
  report.observe_adapter(adapter(on_geforce, av::PciLocation(4u, 18u, 0u), radeon));
  cl_uint value = 99u;
  CHECK_EQ(report.info(bus_nv, sizeof value, &value, nullptr), CL_SUCCESS);
  CHECK_EQ(value, 4u);
  CHECK_EQ(report.info(slot_nv, sizeof value, &value, nullptr), CL_SUCCESS);
  CHECK_EQ(value, 18u);
  CHECK_EQ(report.info(domain_nv, sizeof value, &value, nullptr), CL_SUCCESS);
  CHECK_EQ(value, 0u);
  ClDeviceReport amd(adapter_luid);
  amd.observe_adapter(adapter(on_raphael, av::PciLocation(4u, 18u, 0u), radeon));
  CHECK(!amd.decides(bus_nv));
  CHECK(!amd.decides(slot_nv));
  /* Which vendor the adapter presents, which AMD's free memory is asked by. */
  CHECK(amd.on_vendor(av::PciIdentity::amd_vendor));
  CHECK(!amd.on_vendor(av::PciIdentity::nvidia_vendor));
  CHECK(report.on_vendor(av::PciIdentity::nvidia_vendor)); }
