/* One chunk slot of an rvk context (feature 045): free until it takes an
 * allocation and the resource it became, free again when cleared. */
#include "core/rvk/host_visible_binding.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

struct HostVisibleBindingProbe {
    static void set_resource(HostVisibleBinding &binding, std::uint32_t resource) { binding.resource_id_ = resource; }
};

}  // namespace av

using av::HostVisibleBinding;
using av::HostVisibleBindingProbe;

namespace {

/* Never dereferenced: a slot only names the allocation. */
int                         storage = 0;
const av::Allocation *const somewhere = static_cast<const av::Allocation *>(static_cast<void *>(&storage));

}  // namespace

TEST(host_visible_binding, holds_what_it_took_until_cleared)
{ HostVisibleBinding slot;
  CHECK(!slot.bound());
  CHECK(slot.allocation() == nullptr);
  CHECK_EQ(slot.resource_id(), 0u);
  CHECK(slot.invariants_hold());
  slot.take(*somewhere, 0x00100003u);
  CHECK(slot.bound());
  CHECK(slot.allocation() == somewhere);
  CHECK_EQ(slot.resource_id(), 0x00100003u);
  CHECK(slot.invariants_hold());
  slot.clear();
  CHECK(!slot.bound());
  CHECK_EQ(slot.resource_id(), 0u);
  CHECK(slot.invariants_hold()); }

TEST(host_visible_binding_invariants, a_resource_without_an_allocation_or_the_reverse_breaks_them)
{ HostVisibleBinding slot;
  HostVisibleBindingProbe::set_resource(slot, 7u);
  CHECK(!slot.invariants_hold());
  slot.take(*somewhere, 7u);
  HostVisibleBindingProbe::set_resource(slot, 0u);
  CHECK(!slot.invariants_hold()); }
