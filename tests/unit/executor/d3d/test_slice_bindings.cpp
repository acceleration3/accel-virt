#include "core/support/status.hpp"
#include "executor/d3d/slice_bindings.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/scripted_slice_importer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::MockPlatform;
using av::ScriptedSliceImporter;
using av::SliceBindings;
using av::SourceMemory;
using av::Status;

namespace {

std::uint8_t bytes[4096];

class Rig {
public:
    Rig()
        : platform_(), bindings_(platform_), first_(1u, 1u, 0u, 0u, av::ExportedMemory()),
          second_(1u, 2u, 0u, 0u, av::ExportedMemory()), importer_a_(), importer_b_()
    {
    }

    SliceBindings::Binding make(std::uint32_t resource, const SourceMemory *store, const av::SliceImporter *importer,
                                std::uint64_t size = 256u) const
    {
        return SliceBindings::Binding{resource, store, bytes + resource % 64u, size, importer};
    }

    MockPlatform           platform_;
    SliceBindings          bindings_;
    SourceMemory           first_;
    SourceMemory           second_;
    ScriptedSliceImporter  importer_a_;
    ScriptedSliceImporter  importer_b_;
};

}

TEST(slice_bindings, starts_empty)
{ Rig rig;
  CHECK_EQ(rig.bindings_.count(), 0u);
  SliceBindings::Binding was = {};
  CHECK(!rig.bindings_.find(1u, was));
  CHECK(!rig.bindings_.take(1u, was));
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_bind, remembers_a_resource_under_its_own_key)
{ Rig rig;
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_, 300u)), Status::ok);
  CHECK_EQ(rig.bindings_.count(), 1u);
  SliceBindings::Binding was = {};
  CHECK(rig.bindings_.find(7u, was));
  CHECK_EQ(was.resource_, 7u);
  CHECK(was.store_ == &rig.first_);
  CHECK(was.slice_ == bytes + 7u);
  CHECK_EQ(was.bytes_, 300u);
  CHECK(was.importer_ == &rig.importer_a_);
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_bind, rebinding_a_resource_replaces_it_without_growing)
{ Rig rig;
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_, 300u)), Status::ok);
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.second_, &rig.importer_b_, 900u)), Status::ok);
  CHECK_EQ(rig.bindings_.count(), 1u);
  SliceBindings::Binding was = {};
  CHECK(rig.bindings_.find(7u, was));
  CHECK(was.store_ == &rig.second_);
  CHECK_EQ(was.bytes_, 900u);
  CHECK(was.importer_ == &rig.importer_b_);
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_bind, a_binding_with_no_store_or_no_slice_is_refused_and_changes_nothing)
{ Rig rig;
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_)), Status::ok);
  SliceBindings::Binding bad = rig.make(7u, nullptr, &rig.importer_a_);
  CHECK_STATUS(rig.bindings_.bind(bad), Status::invalid_parameter);
  bad = rig.make(7u, &rig.second_, &rig.importer_a_);
  bad.slice_ = nullptr;
  CHECK_STATUS(rig.bindings_.bind(bad), Status::invalid_parameter);
  bad.resource_ = 8u;
  CHECK_STATUS(rig.bindings_.bind(bad), Status::invalid_parameter);
  CHECK_EQ(rig.bindings_.count(), 1u);
  SliceBindings::Binding was = {};
  CHECK(rig.bindings_.find(7u, was));
  CHECK(was.store_ == &rig.first_);
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_bind, the_extreme_resource_numbers_are_ordinary_keys)
{ Rig rig;
  const std::uint32_t keys[] = {0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu};
  for (const std::uint32_t key : keys) CHECK_STATUS(rig.bindings_.bind(rig.make(key, &rig.first_, &rig.importer_a_)), Status::ok);
  CHECK_EQ(rig.bindings_.count(), sizeof keys / sizeof keys[0]);
  for (const std::uint32_t key : keys) {
      SliceBindings::Binding was = {};
      CHECK(rig.bindings_.find(key, was));
      CHECK_EQ(was.resource_, key);
  }
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_bind, refuses_when_the_table_cannot_grow)
{ Rig rig;
  rig.platform_.fail_allocation_at(1u);
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_)), Status::no_memory);
  CHECK_EQ(rig.bindings_.count(), 0u);
  CHECK(rig.bindings_.invariants_hold());
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_)), Status::ok); }

TEST(slice_bindings_bind, many_resources_keep_their_own_bindings)
{ Rig rig;
  for (std::uint32_t resource = 1u; resource <= 3000u; resource++)
      CHECK_STATUS(rig.bindings_.bind(rig.make(resource, resource % 2u != 0u ? &rig.first_ : &rig.second_,
                                               &rig.importer_a_, resource)),
                   Status::ok);
  CHECK_EQ(rig.bindings_.count(), 3000u);
  CHECK(rig.bindings_.invariants_hold());
  for (std::uint32_t resource = 1u; resource <= 3000u; resource++) {
      SliceBindings::Binding was = {};
      CHECK(rig.bindings_.find(resource, was));
      CHECK_EQ(was.bytes_, resource);
      CHECK(was.store_ == (resource % 2u != 0u ? &rig.first_ : &rig.second_));
  } }

TEST(slice_bindings_take, removes_the_binding_and_hands_it_back_once)
{ Rig rig;
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_, 300u)), Status::ok);
  CHECK_STATUS(rig.bindings_.bind(rig.make(8u, &rig.first_, &rig.importer_a_, 400u)), Status::ok);
  SliceBindings::Binding was = {};
  CHECK(rig.bindings_.take(7u, was));
  CHECK_EQ(was.bytes_, 300u);
  CHECK_EQ(rig.bindings_.count(), 1u);
  CHECK(!rig.bindings_.take(7u, was));
  CHECK_EQ(was.bytes_, 300u);
  CHECK(!rig.bindings_.find(7u, was));
  CHECK(rig.bindings_.find(8u, was));
  CHECK_EQ(was.bytes_, 400u);
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_find, leaves_the_binding_in_place_and_the_answer_untouched_on_a_miss)
{ Rig rig;
  CHECK_STATUS(rig.bindings_.bind(rig.make(7u, &rig.first_, &rig.importer_a_, 300u)), Status::ok);
  SliceBindings::Binding was = rig.make(99u, &rig.second_, &rig.importer_b_, 5u);
  CHECK(!rig.bindings_.find(8u, was));
  CHECK_EQ(was.resource_, 99u);
  CHECK_EQ(was.bytes_, 5u);
  CHECK(rig.bindings_.find(7u, was));
  CHECK(rig.bindings_.find(7u, was));
  CHECK_EQ(rig.bindings_.count(), 1u); }

TEST(slice_bindings_take_all_bound_to, visits_and_removes_exactly_the_bindings_of_one_store_in_one_pass)
{ Rig rig;
  for (std::uint32_t resource = 1u; resource <= 10u; resource++)
      CHECK_STATUS(rig.bindings_.bind(rig.make(resource, resource <= 4u ? &rig.first_ : &rig.second_,
                                               &rig.importer_a_)),
                   Status::ok);
  std::uint32_t visited = 0u;
  std::uint32_t seen[10] = {};
  rig.bindings_.take_all_bound_to(rig.first_, [&](const SliceBindings::Binding &binding) {
      CHECK(binding.store_ == &rig.first_);
      seen[binding.resource_ - 1u]++;
      visited++;
  });
  CHECK_EQ(visited, 4u);
  for (std::uint32_t resource = 1u; resource <= 4u; resource++) CHECK_EQ(seen[resource - 1u], 1u);
  for (std::uint32_t resource = 5u; resource <= 10u; resource++) CHECK_EQ(seen[resource - 1u], 0u);
  CHECK_EQ(rig.bindings_.count(), 6u);
  SliceBindings::Binding was = {};
  for (std::uint32_t resource = 1u; resource <= 4u; resource++) CHECK(!rig.bindings_.find(resource, was));
  for (std::uint32_t resource = 5u; resource <= 10u; resource++) CHECK(rig.bindings_.find(resource, was));
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_take_all_bound_to, a_store_nothing_is_bound_to_visits_nothing)
{ Rig rig;
  CHECK_STATUS(rig.bindings_.bind(rig.make(1u, &rig.first_, &rig.importer_a_)), Status::ok);
  std::uint32_t visited = 0u;
  rig.bindings_.take_all_bound_to(rig.second_, [&](const SliceBindings::Binding &) { visited++; });
  CHECK_EQ(visited, 0u);
  CHECK_EQ(rig.bindings_.count(), 1u);
  SliceBindings empty(rig.platform_);
  empty.take_all_bound_to(rig.first_, [&](const SliceBindings::Binding &) { visited++; });
  CHECK_EQ(visited, 0u); }

TEST(slice_bindings_take_all_bound_to, every_binding_of_a_store_can_go_in_one_call)
{ Rig rig;
  for (std::uint32_t resource = 1u; resource <= 500u; resource++)
      CHECK_STATUS(rig.bindings_.bind(rig.make(resource, &rig.first_, &rig.importer_a_)), Status::ok);
  std::uint32_t visited = 0u;
  rig.bindings_.take_all_bound_to(rig.first_, [&](const SliceBindings::Binding &) { visited++; });
  CHECK_EQ(visited, 500u);
  CHECK_EQ(rig.bindings_.count(), 0u);
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_take_all_of, removes_the_bindings_one_importer_made_and_no_others)
{ Rig rig;
  for (std::uint32_t resource = 1u; resource <= 6u; resource++)
      CHECK_STATUS(rig.bindings_.bind(rig.make(resource, &rig.first_,
                                               resource % 3u == 0u ? static_cast<const av::SliceImporter *>(&rig.importer_b_)
                                                                    : static_cast<const av::SliceImporter *>(&rig.importer_a_))),
                   Status::ok);
  rig.bindings_.take_all_of(rig.importer_b_);
  CHECK_EQ(rig.bindings_.count(), 4u);
  SliceBindings::Binding was = {};
  CHECK(!rig.bindings_.find(3u, was));
  CHECK(!rig.bindings_.find(6u, was));
  CHECK(rig.bindings_.find(1u, was));
  rig.bindings_.take_all_of(rig.importer_b_);
  CHECK_EQ(rig.bindings_.count(), 4u);
  CHECK(rig.bindings_.invariants_hold()); }

TEST(slice_bindings_clear, forgets_everything_and_the_table_is_usable_again)
{ Rig rig;
  for (std::uint32_t resource = 1u; resource <= 20u; resource++)
      CHECK_STATUS(rig.bindings_.bind(rig.make(resource, &rig.first_, &rig.importer_a_)), Status::ok);
  rig.bindings_.clear();
  CHECK_EQ(rig.bindings_.count(), 0u);
  SliceBindings::Binding was = {};
  CHECK(!rig.bindings_.find(5u, was));
  CHECK(rig.bindings_.invariants_hold());
  CHECK_STATUS(rig.bindings_.bind(rig.make(5u, &rig.second_, &rig.importer_b_)), Status::ok);
  CHECK(rig.bindings_.find(5u, was));
  CHECK(was.store_ == &rig.second_);
  rig.bindings_.clear();
  rig.bindings_.clear();
  CHECK_EQ(rig.bindings_.count(), 0u); }
