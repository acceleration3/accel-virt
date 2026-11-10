#include "core/support/extension_words.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_extension_policy.hpp"

#include <cstring>

using av::clicd::ClExtensionPolicy;

namespace {
constexpr ClExtensionPolicy::Row rows[] = {
    {"cl_khr_fp64", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
    {"cl_khr_int64_base_atomics", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
    {"cl_khr_3d_image_writes", 1u, "cl_extension_policy.a_row_the_host_does_not_offer_is_not_reported"},
};
constexpr uint32_t row_count = 3u;

class Host {
public:
    Host() : platform_(), words_(platform_, av::Pool::paged) {}
    explicit Host(const char *text) : platform_(), words_(platform_, av::Pool::paged)
    { CHECK_STATUS(words_.hold(text, std::strlen(text) + 1u), av::Status::ok); }
    Host(const char *text, std::size_t bytes) : platform_(), words_(platform_, av::Pool::paged)
    { CHECK_STATUS(words_.hold(text, bytes), av::Status::ok); }

    const av::ExtensionWords &words() const { return words_; }

private:
    av::MockPlatform   platform_;
    av::ExtensionWords words_;
};

}

TEST(cl_extension_policy, an_empty_policy_reports_nothing_whatever_the_host_offers)
{ ClExtensionPolicy policy(nullptr, 0u);
  const Host host("cl_khr_fp64 cl_khr_icd cl_khr_gl_sharing");
  char into[128] = {'z'};
  const std::size_t needed = policy.report(host.words(), into, sizeof into);
  CHECK_EQ(needed, 1u);
  CHECK_EQ(into[0], '\0');
  CHECK_EQ(policy.rows(), 0u);
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, a_row_the_host_offers_is_reported)
{ ClExtensionPolicy policy(rows, row_count);
  const Host host("cl_khr_fp64 cl_khr_int64_base_atomics cl_khr_icd");
  char into[128] = {};
  const std::size_t needed = policy.report(host.words(), into, sizeof into);
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64 cl_khr_int64_base_atomics"), 0);
  CHECK_EQ(needed, std::strlen(into) + 1u);
  CHECK(policy.reports(host.words(), "cl_khr_fp64"));
  CHECK(!policy.reports(host.words(), "cl_khr_3d_image_writes"));
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, a_row_the_host_does_not_offer_is_not_reported)
{ ClExtensionPolicy policy(rows, row_count);
  const Host host("cl_khr_icd cl_khr_byte_addressable_store");
  char into[128] = {'z'};
  CHECK_EQ(policy.report(host.words(), into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0'); }

TEST(cl_extension_policy, an_extension_the_host_offers_and_no_row_names_is_not_reported)
{ ClExtensionPolicy policy(rows, row_count);
  const Host host("cl_khr_gl_sharing cl_khr_fp16 cl_intel_unified_shared_memory");
  char into[128] = {'z'};
  CHECK_EQ(policy.report(host.words(), into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0');
  CHECK(!policy.reportable("cl_khr_gl_sharing"));
  CHECK(!policy.reportable("cl_khr_fp16"));
  CHECK(!policy.reports(host.words(), "cl_khr_fp16")); }

TEST(cl_extension_policy, a_row_answered_here_is_reported_whatever_the_host_offers)
{ const ClExtensionPolicy::Row own[] = {
      {"cl_khr_fp64", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
      {"cl_khr_d3d10_sharing", 1u, "cl_extension_policy.a_row_answered_here_is_reported_whatever_the_host_offers",
       ClExtensionPolicy::answered_here},
  };
  ClExtensionPolicy policy(own, 2u);
  char into[128] = {'z'};
  const Host nothing;
  CHECK_EQ(policy.report(nothing.words(), into, sizeof into), std::strlen("cl_khr_d3d10_sharing") + 1u);
  CHECK_EQ(std::strcmp(into, "cl_khr_d3d10_sharing"), 0);
  const Host host("cl_khr_icd");
  static_cast<void>(policy.report(host.words(), into, sizeof into));
  CHECK_EQ(std::strcmp(into, "cl_khr_d3d10_sharing"), 0);
  const ClExtensionPolicy::Row dashed[] = {
      {"cl_khr_d3d10_sharing", 1u, "cl_extension_policy.a_row_answered_here_is_reported_whatever_the_host_offers",
       "-x"}};
  ClExtensionPolicy borrowed(dashed, 1u);
  CHECK_EQ(borrowed.report(host.words(), into, sizeof into), 1u);
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, a_name_is_matched_whole_and_never_as_a_prefix)
{ ClExtensionPolicy policy(rows, row_count);
  CHECK(!policy.reports(Host("cl_khr_fp64_extra").words(), "cl_khr_fp64"));
  CHECK(!policy.reports(Host("cl_khr_fp6").words(), "cl_khr_fp64"));
  CHECK(!policy.reports(Host("xcl_khr_fp64").words(), "cl_khr_fp64"));
  CHECK(policy.reports(Host("cl_khr_fp64").words(), "cl_khr_fp64"));
  CHECK(policy.reports(Host("cl_khr_icd cl_khr_fp64").words(), "cl_khr_fp64")); }

TEST(cl_extension_policy, a_host_string_padded_with_spaces_is_read_as_a_list)
{ ClExtensionPolicy policy(rows, row_count);
  const Host host("   cl_khr_fp64     cl_khr_3d_image_writes   ");
  char into[128] = {};
  static_cast<void>(policy.report(host.words(), into, sizeof into));
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64 cl_khr_3d_image_writes"), 0); }

TEST(cl_extension_policy, a_host_string_with_no_terminator_is_read_only_as_far_as_it_says)
{ ClExtensionPolicy policy(rows, row_count);
  const char host[] = {'c', 'l', '_', 'k', 'h', 'r', '_', 'f', 'p', '6', '4'};
  char into[128] = {};
  const Host whole(host, sizeof host);
  static_cast<void>(policy.report(whole.words(), into, sizeof into));
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64"), 0);
  const Host cut(host, 10u);
  CHECK(!policy.reports(cut.words(), "cl_khr_fp64")); }

TEST(cl_extension_policy, an_empty_host_string_offers_nothing)
{ ClExtensionPolicy policy(rows, row_count);
  char into[128] = {'z'};
  const Host empty("");
  CHECK_EQ(policy.report(empty.words(), into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0');
  const Host fp64("cl_khr_fp64");
  CHECK(!policy.reports(empty.words(), "cl_khr_fp64"));
  CHECK(!policy.reports(fp64.words(), nullptr)); }

TEST(cl_extension_policy, an_answer_that_does_not_fit_writes_nothing_and_says_how_much_it_needs)
{ ClExtensionPolicy policy(rows, row_count);
  const Host host("cl_khr_fp64 cl_khr_int64_base_atomics");
  char into[8] = {'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z'};
  const std::size_t needed = policy.report(host.words(), into, sizeof into);
  CHECK(needed > sizeof into);
  CHECK_EQ(into[0], 'z');
  char room[64] = {};
  CHECK_EQ(policy.report(host.words(), room, needed), needed);
  CHECK_EQ(std::strlen(room) + 1u, needed); }

TEST(cl_extension_policy, the_size_may_be_asked_for_with_no_buffer)
{ ClExtensionPolicy policy(rows, row_count);
  const Host host("cl_khr_fp64");
  const std::size_t needed = policy.report(host.words(), nullptr, 0u);
  CHECK_EQ(needed, std::strlen("cl_khr_fp64") + 1u); }

TEST(cl_extension_policy, reportable_is_what_the_icd_asks_before_it_acts_on_an_extension)
{ ClExtensionPolicy policy(rows, row_count);
  CHECK(policy.reportable("cl_khr_fp64"));
  CHECK(!policy.reportable("cl_khr_fp64_extra"));
  CHECK(!policy.reportable(""));
  CHECK(!policy.reportable(nullptr)); }

TEST(cl_extension_policy, a_policy_given_no_rows_but_a_count_holds_none)
{ ClExtensionPolicy policy(nullptr, 7u);
  CHECK_EQ(policy.rows(), 0u);
  CHECK(policy.invariants_hold());
  CHECK(!policy.reportable("cl_khr_fp64")); }

TEST(cl_extension_policy, a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one)
{ const ClExtensionPolicy::Row gated[] = {
      {"cl_khr_pci_bus_info", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-*"},
      {"cl_amd_device_attribute_query", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-1002"},
      {"cl_nv_device_attribute_query", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-10DE"},
      {"cl_khr_3d_image_writes", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-10de"},
      {"cl_khr_depth_images", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-#"},
      {"cl_khr_device_uuid", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-10:E"},
      {"cl_khr_byte_addressable_store", 1u, "cl_extension_policy.a_vendor_s_row_is_reported_on_its_adapter_and_a_located_row_on_any_described_one", "-*1"},
  };
  ClExtensionPolicy policy(gated, 7u);
  char into[160] = {};
  const Host nothing;
  CHECK_EQ(policy.report(nothing.words(), into, sizeof into), 1u);
  CHECK_EQ(into[0], '\0');
  policy.observe_vendor(0x1002u);
  static_cast<void>(policy.report(nothing.words(), into, sizeof into));
  CHECK_EQ(std::strcmp(into, "cl_khr_pci_bus_info cl_amd_device_attribute_query"), 0);
  policy.observe_vendor(0x10DEu);
  static_cast<void>(policy.report(nothing.words(), into, sizeof into));
  CHECK_EQ(std::strcmp(into, "cl_khr_pci_bus_info cl_nv_device_attribute_query"), 0);
  policy.observe_vendor(0x8086u);
  const Host host("cl_amd_device_attribute_query");
  static_cast<void>(policy.report(host.words(), into, sizeof into));
  CHECK_EQ(std::strcmp(into, "cl_khr_pci_bus_info"), 0);
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, the_shipped_table_is_a_valid_policy_that_reports_what_the_icd_answers_itself)
{ ClExtensionPolicy policy(av::clicd::reported_extensions, av::clicd::reported_extension_count);
  CHECK_EQ(policy.rows(), av::clicd::reported_extension_count);
  CHECK(policy.invariants_hold());
  CHECK(policy.reportable("cl_khr_external_memory"));
  CHECK(policy.reportable("cl_khr_icd"));
  policy.observe_vendor(0x1002u);
  char into[1024] = {};
  const Host nothing;
  static_cast<void>(policy.report(nothing.words(), into, sizeof into));
  CHECK(std::strstr(into, "cl_khr_d3d10_sharing") != nullptr);
  CHECK(std::strstr(into, "cl_khr_d3d11_sharing") != nullptr);
  CHECK(std::strstr(into, "cl_amd_device_attribute_query") != nullptr);
  CHECK(std::strstr(into, "cl_nv_device_attribute_query") == nullptr);
  CHECK(std::strstr(into, "cl_khr_fp64") == nullptr); }

TEST(cl_extension_policy, more_rows_than_it_holds_are_clamped_to_what_it_holds)
{ static char                names[40][16];
  static ClExtensionPolicy::Row many[40];
  for (unsigned index = 0u; index < 40u; ++index) {
      std::strcpy(names[index], "cl_av_many_00");
      names[index][11] = static_cast<char>('0' + index / 10u);
      names[index][12] = static_cast<char>('0' + index % 10u);
      many[index] = {names[index], 1u, "cl_extension_policy.more_rows_than_it_holds_are_clamped_to_what_it_holds"};
  }
  ClExtensionPolicy policy(many, 40u);
  CHECK(40u > ClExtensionPolicy::max_rows);
  CHECK_EQ(policy.rows(), ClExtensionPolicy::max_rows);
  CHECK(policy.invariants_hold()); }

TEST(cl_extension_policy, the_invariant_catches_a_row_nothing_could_report)
{ ClExtensionPolicy shipped(rows, row_count);
  CHECK(shipped.invariants_hold());

  const auto refuses = [](const ClExtensionPolicy::Row *table, unsigned int count) {
      ClExtensionPolicy policy(table, count);
      return policy.invariants_hold();
  };
  const ClExtensionPolicy::Row no_name[] = {
      {nullptr, 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  const ClExtensionPolicy::Row no_test[] = {{"cl_khr_fp64", 1u, nullptr}};
  const ClExtensionPolicy::Row empty_test[] = {{"cl_khr_fp64", 1u, ""}};
  const ClExtensionPolicy::Row no_version[] = {
      {"cl_khr_fp64", 0u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  const ClExtensionPolicy::Row empty_name[] = {
      {"", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  const ClExtensionPolicy::Row only_the_prefix[] = {
      {"cl_", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  const ClExtensionPolicy::Row wrong_prefix[] = {
      {"VK_KHR_swapchain", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  const ClExtensionPolicy::Row empty_host[] = {
      {"cl_khr_fp64", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report", ""}};
  const ClExtensionPolicy::Row named_twice[] = {
      {"cl_khr_fp64", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"},
      {"cl_khr_fp64", 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  CHECK(!refuses(no_name, 1u));
  CHECK(!refuses(no_test, 1u));
  CHECK(!refuses(empty_test, 1u));
  CHECK(!refuses(no_version, 1u));
  CHECK(!refuses(empty_name, 1u));
  CHECK(!refuses(only_the_prefix, 1u));
  CHECK(!refuses(wrong_prefix, 1u));
  CHECK(!refuses(empty_host, 1u));
  CHECK(!refuses(named_twice, 2u));

  static char over_long[av::icd::ExtensionRows::name_bytes + 2u];
  for (char &byte : over_long) byte = 'x';
  over_long[0] = 'c';
  over_long[1] = 'l';
  over_long[2] = '_';
  over_long[sizeof over_long - 1u] = '\0';
  const ClExtensionPolicy::Row runs_on[] = {
      {over_long, 1u, "cl_extension_policy.the_invariant_catches_a_row_nothing_could_report"}};
  CHECK(!refuses(runs_on, 1u));
  CHECK(!ClExtensionPolicy(runs_on, 1u).reportable(over_long)); }

TEST(cl_extension_policy, report_versions_names_what_report_names_with_each_row_s_version)
{ const ClExtensionPolicy::Row versioned[] = {
      {"cl_khr_fp64", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
      {"cl_khr_d3d10_sharing", 2u, "cl_extension_policy.a_row_answered_here_is_reported_whatever_the_host_offers",
       ClExtensionPolicy::answered_here},
      {"cl_khr_3d_image_writes", 1u, "cl_extension_policy.a_row_the_host_does_not_offer_is_not_reported"},
  };
  ClExtensionPolicy policy(versioned, 3u);
  const Host        host("cl_khr_icd cl_khr_fp64");
  CHECK_EQ(policy.report_versions(host.words(), nullptr, 0u), 2u);
  cl_name_version listed[3] = {};
  std::memset(listed, 'z', sizeof listed);
  CHECK_EQ(policy.report_versions(host.words(), listed, 1u), 2u);
  CHECK_EQ(listed[0].name[0], 'z');
  CHECK_EQ(policy.report_versions(host.words(), listed, 3u), 2u);
  CHECK_EQ(std::strcmp(listed[0].name, "cl_khr_fp64"), 0);
  CHECK_EQ(listed[0].version, CL_MAKE_VERSION(1u, 0u, 0u));
  CHECK_EQ(std::strcmp(listed[1].name, "cl_khr_d3d10_sharing"), 0);
  CHECK_EQ(listed[1].version, CL_MAKE_VERSION(2u, 0u, 0u));
  CHECK_EQ(listed[1].name[CL_NAME_VERSION_MAX_NAME_SIZE - 1u], '\0');
  CHECK_EQ(listed[2].name[0], 'z'); }

TEST(cl_extension_policy, a_row_the_wire_does_not_carry_is_held_but_never_reported_whatever_the_host_offers)
{ const ClExtensionPolicy::Row mixed[] = {
      {"cl_av_not_carried", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
      {"cl_khr_fp64", 1u, "cl_extension_policy.a_row_the_host_offers_is_reported"},
  };
  ClExtensionPolicy policy(mixed, 2u);
  const Host        host("cl_av_not_carried cl_khr_fp64");
  char              into[128] = {};
  CHECK_EQ(policy.report(host.words(), into, sizeof into), std::strlen("cl_khr_fp64") + 1u);
  CHECK_EQ(std::strcmp(into, "cl_khr_fp64"), 0);
  CHECK(!policy.reports(host.words(), "cl_av_not_carried"));
  CHECK(!policy.reportable("cl_av_not_carried"));
  cl_name_version listed[2] = {};
  CHECK_EQ(policy.report_versions(host.words(), listed, 2u), 1u);
  CHECK_EQ(std::strcmp(listed[0].name, "cl_khr_fp64"), 0);
  CHECK(policy.invariants_hold()); }
