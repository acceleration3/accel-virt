/* What Windows says of one avgpu adapter (AdapterFacts): every fact as the
 * kernel gave it, a fact refused unknown and read as nothing - never a guess,
 * never what an earlier learn left - and the others still known. */
#include "core/support/text_buffer.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/adapter_facts.hpp"
#include "proto/escape/umd_adapter_info.hpp"

#include <cstdint>
#include <cstring>

using av::AvgpuAdapters;
using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::Status;
using av::TextBuffer;
using av::UmdAdapterInfo;
using av::icd::AdapterFacts;

namespace {

const PciIdentity       on_raphael(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u);
constexpr std::uint64_t some_luid = 0x0000000200001234ull;
constexpr std::uint64_t version = 0x0000002300050000ull;
constexpr std::uint32_t all_facts = AdapterFacts::luid_fact | AdapterFacts::identity_fact |
                                    AdapterFacts::location_fact | AdapterFacts::name_fact |
                                    AdapterFacts::driver_version_fact | AdapterFacts::segments_fact |
                                    AdapterFacts::features_fact;

/* One adapter Windows describes in full. */
void describe(MockKmt &kmt)
{
    kmt.add_adapter(on_raphael, 3200u);
    kmt.describe_adapter(0u, PciLocation(4u, 17u, 2u), "AMD Radeon(TM) Graphics", version);
    kmt.set_adapter_luid(0u, some_luid);
    kmt.set_segment_sizes(512ull << 20u, 1ull << 20u, 4ull << 30u);
    kmt.set_adapter_features(0u, UmdAdapterInfo::feature_integrated_gpu);
}

/* The adapter handle MockKmt hands out for its first adapter. */
std::uint32_t first_adapter(MockKmt &kmt)
{
    std::uint32_t handles[4] = {};
    std::uint32_t found = 0u;
    if (kmt.enumerate_adapters(handles, 4u, found) != Status::ok || found == 0u) return 0u;
    return handles[0];
}

}  // namespace

TEST(adapter_facts, knows_nothing_before_a_learn)
{ const AdapterFacts facts;
  CHECK(!facts.knows(AdapterFacts::luid_fact));
  CHECK(!facts.knows(AdapterFacts::name_fact));
  CHECK(facts.knows(0u));
  CHECK_EQ(facts.luid(), 0u);
  CHECK_EQ(facts.name()[0], '\0');
  CHECK(!facts.integrated());
  CHECK(facts.invariants_hold()); }

TEST(adapter_facts, learns_every_fact_windows_gives)
{ MockKmt kmt;
  describe(kmt);
  AdapterFacts facts;
  facts.learn(kmt, first_adapter(kmt));
  CHECK(facts.knows(all_facts));
  CHECK_EQ(facts.luid(), some_luid);
  CHECK(facts.identity().same_as(on_raphael));
  CHECK_EQ(facts.identity().revision(), 0xC9u);
  CHECK_EQ(facts.location().bus(), 4u);
  CHECK_EQ(facts.location().device(), 17u);
  CHECK_EQ(facts.location().function(), 2u);
  CHECK(std::strcmp(facts.name(), "AMD Radeon(TM) Graphics") == 0);
  CHECK_EQ(facts.driver_version(), version);
  CHECK_EQ(facts.dedicated_video_bytes(), 512ull << 20u);
  CHECK_EQ(facts.dedicated_system_bytes(), 1ull << 20u);
  CHECK_EQ(facts.shared_system_bytes(), 4ull << 30u);
  CHECK(facts.integrated());
  CHECK(facts.invariants_hold()); }

/* Each refusal leaves that fact, and only that one, unknown and read as
 * nothing; what Windows did give is still there. */
TEST(adapter_facts, a_refused_fact_is_unknown_and_reads_as_nothing_while_the_rest_are_known)
{ struct Refusal {
      MockKmt::Call call;
      std::uint32_t fact;
  };
  const Refusal refusals[] = {
      {MockKmt::Call::adapter_luid, AdapterFacts::luid_fact},
      {MockKmt::Call::query_identity, AdapterFacts::identity_fact},
      {MockKmt::Call::query_adapter_location, AdapterFacts::location_fact},
      {MockKmt::Call::query_adapter_name, AdapterFacts::name_fact},
      {MockKmt::Call::query_umd_driver_version, AdapterFacts::driver_version_fact},
      {MockKmt::Call::query_segment_sizes, AdapterFacts::segments_fact},
      {MockKmt::Call::query_adapter_features, AdapterFacts::features_fact},
  };
  for (const Refusal &refusal : refusals) {
      MockKmt kmt;
      describe(kmt);
      const std::uint32_t adapter = first_adapter(kmt);
      kmt.fail_at(refusal.call, 1u, MockKmt::invalid_parameter);
      AdapterFacts facts;
      facts.learn(kmt, adapter);
      CHECK(!facts.knows(refusal.fact));
      CHECK(facts.knows(all_facts & ~refusal.fact));
      CHECK(!facts.knows(all_facts));
      CHECK_EQ(facts.luid(), refusal.fact == AdapterFacts::luid_fact ? 0u : some_luid);
      CHECK_EQ(facts.identity().vendor(), refusal.fact == AdapterFacts::identity_fact ? 0u : 0x1002u);
      CHECK_EQ(facts.location().device(), refusal.fact == AdapterFacts::location_fact ? 0u : 17u);
      CHECK_EQ(facts.name()[0] == '\0', refusal.fact == AdapterFacts::name_fact);
      CHECK_EQ(facts.driver_version(), refusal.fact == AdapterFacts::driver_version_fact ? 0u : version);
      CHECK_EQ(facts.shared_system_bytes(), refusal.fact == AdapterFacts::segments_fact ? 0u : 4ull << 30u);
      /* A miniport from before the bit reads as discrete. */
      CHECK_EQ(facts.integrated(), refusal.fact != AdapterFacts::features_fact);
      CHECK(facts.invariants_hold());
  } }

/* A LUID of zero names no adapter: it is not a fact. */
TEST(adapter_facts, a_zero_luid_is_unknown)
{ MockKmt kmt;
  describe(kmt);
  kmt.set_adapter_luid(0u, 0u);
  AdapterFacts facts;
  facts.learn(kmt, first_adapter(kmt));
  CHECK(!facts.knows(AdapterFacts::luid_fact));
  CHECK(facts.knows(AdapterFacts::name_fact));
  CHECK(facts.invariants_hold()); }

/* Learning again forgets: a fact the second learn is refused is not the
 * first one's answer. */
TEST(adapter_facts, a_second_learn_forgets_what_the_first_was_told)
{ MockKmt kmt;
  describe(kmt);
  const std::uint32_t adapter = first_adapter(kmt);
  AdapterFacts facts;
  facts.learn(kmt, adapter);
  CHECK(facts.knows(all_facts));
  kmt.fail_at(MockKmt::Call::query_adapter_name, 1u, MockKmt::invalid_parameter);
  kmt.fail_at(MockKmt::Call::query_adapter_features, 1u, MockKmt::invalid_parameter);
  facts.learn(kmt, adapter);
  CHECK(!facts.knows(AdapterFacts::name_fact));
  CHECK_EQ(facts.name()[0], '\0');
  CHECK(!facts.integrated());
  CHECK(facts.knows(AdapterFacts::luid_fact));
  CHECK(facts.invariants_hold());
  facts.forget();
  CHECK(!facts.knows(AdapterFacts::luid_fact));
  CHECK_EQ(facts.luid(), 0u);
  CHECK(facts.invariants_hold()); }

/* The driver package's version as Windows writes it: four 16-bit parts, the
 * most significant first, each in full. */
TEST(adapter_facts, writes_the_driver_version_as_windows_does)
{ MockKmt kmt;
  describe(kmt);
  const std::uint32_t adapter = first_adapter(kmt);
  AdapterFacts facts;
  facts.learn(kmt, adapter);
  char       bytes[32] = {};
  TextBuffer text(bytes, sizeof bytes);
  facts.append_driver_version(text);
  CHECK(std::strcmp(bytes, "0.35.5.0") == 0);
  kmt.describe_adapter(0u, PciLocation(), "", 0x0001FFFF00020003ull);
  facts.learn(kmt, adapter);
  char       more[32] = {};
  TextBuffer again(more, sizeof more);
  facts.append_driver_version(again);
  CHECK(std::strcmp(more, "1.65535.2.3") == 0); }
