#include "harness/framework/test_registry.hpp"
#include "icd/common/root_binding.hpp"

using av::icd::RootBinding;

namespace {
struct Root {
    int marker;
};

}

TEST(root_binding, nothing_is_bound_until_a_root_binds_itself)
{ RootBinding<Root> binding;
  CHECK(!binding.bound()); }

TEST(root_binding, the_bound_root_is_the_one_answered_until_it_unbinds)
{ RootBinding<Root> binding;
  Root first{1};
  Root second{2};
  binding.bind(first);
  CHECK(binding.bound());
  CHECK(&binding.root() == &first);
  binding.bind(second);
  CHECK_EQ(binding.root().marker, 2);
  binding.unbind();
  CHECK(!binding.bound()); }

constinit RootBinding<Root> process_binding;

TEST(root_binding, a_constinit_binding_starts_unbound)
{ CHECK(!process_binding.bound()); }
