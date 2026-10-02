/* A context's resource ids, as the driver names them and the device reads them
 * back: three transport resources, then 64 chunk slots, and nothing else. */
#include "miniport/av_context/av_context_binding.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/context_resources.hpp"

#include <cstdint>

using av::ContextResources;

/* The driver names ids with these numbers and the device reads them back with
 * the same: one definition, which the driver's binding takes. */
TEST(context_resources, the_chunks_follow_the_three_transport_resources_and_end_where_the_next_context_begins)
{ const ContextResources resources(0x00100000u + 67u * 5u);
  CHECK_EQ(ContextResources::per_context, 67u);
  CHECK_EQ(av::AvContextBinding::resources_per_context, ContextResources::per_context);
  CHECK_EQ(av::AvContextBinding::chunk_slots, ContextResources::chunk_slots);
  CHECK_EQ(resources.chunk(0u), 0x00100000u + 338u);
  CHECK_EQ(resources.chunk(63u), 0x00100000u + 401u);
  CHECK_EQ(0x00100000u + 67u * 6u, resources.chunk(63u) + 1u); }

TEST(context_resources_chunk_slot, every_chunk_id_is_its_slot_and_its_neighbours_are_none)
{ const ContextResources resources(200u);
  for (std::uint32_t slot = 0u; slot < ContextResources::chunk_slots; slot++)
      CHECK_EQ(resources.chunk_slot(resources.chunk(slot)), slot);
  const std::uint32_t none[] = {0u, 199u, 200u, 201u, 202u, 267u, 268u, 0xFFFFFFFFu};
  for (const std::uint32_t id : none) CHECK_EQ(resources.chunk_slot(id), ContextResources::chunk_slots); }

TEST(context_resources_chunk_slot, a_first_id_near_the_top_does_not_wrap_onto_low_ids)
{ const ContextResources resources(0xFFFFFFF0u);
  CHECK_EQ(resources.chunk_slot(0xFFFFFFF3u), 0u);
  CHECK_EQ(resources.chunk_slot(0xFFFFFFFFu), 12u);
  CHECK_EQ(resources.chunk_slot(0u), ContextResources::chunk_slots);
  CHECK_EQ(resources.chunk_slot(50u), ContextResources::chunk_slots); }
