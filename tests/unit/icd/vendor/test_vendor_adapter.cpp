#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/vendor/vendor_adapter.hpp"

#include <cstdint>
#include <cstring>

using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::icd::AdapterFacts;
using av::vendor::VendorAdapter;

TEST(vendor_adapter, carries_the_facts_it_was_made_with_and_whether_it_is_primary)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u), 3200u);
  kmt.describe_adapter(0u, PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", 0x0000002300050000ull);
  kmt.set_adapter_luid(0u, 0x51u);
  std::uint32_t handles[4] = {};
  std::uint32_t found = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(handles, 4u, found), av::Status::ok);
  AdapterFacts facts;
  facts.learn(kmt, handles[0]);
  CHECK(facts.knows(AdapterFacts::luid_fact | AdapterFacts::name_fact));
  const VendorAdapter adapter(facts, true);
  CHECK_EQ(adapter.facts().luid(), facts.luid());
  CHECK_EQ(adapter.facts().identity().revision(), 0xC9u);
  CHECK(std::strcmp(adapter.facts().name(), facts.name()) == 0);
  CHECK(adapter.primary());
  const VendorAdapter empty;
  CHECK_EQ(empty.facts().luid(), 0u);
  CHECK(!empty.facts().knows(AdapterFacts::luid_fact));
  CHECK(!empty.primary()); }
