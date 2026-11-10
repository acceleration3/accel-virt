#include "harness/framework/test_registry.hpp"
#include "icd/d3d/query_kind.hpp"

using av::d3d::QueryKind;

TEST(query_kind, the_queries_of_direct3d_10_keep_their_numbers_and_sizes)
{ CHECK_EQ(QueryKind(0u).api_type(), 0u);
  CHECK_EQ(QueryKind(1u).data_bytes(), 8u);
  CHECK_EQ(QueryKind(3u).data_bytes(), 16u);
  CHECK_EQ(QueryKind(5u).api_type(), 5u);
  CHECK_EQ(QueryKind(6u).data_bytes(), 16u);
  CHECK_EQ(QueryKind(7u).data_bytes(), 4u); }

TEST(query_kind, the_pipeline_statistics_of_direct3d_11_are_the_fourth_query_of_88_bytes)
{ const QueryKind statistics(8u);
  CHECK(statistics.known());
  CHECK_EQ(statistics.api_type(), 4u);
  CHECK_EQ(statistics.data_bytes(), 88u); }

TEST(query_kind, the_stream_queries_of_direct3d_11_follow_the_numbers_of_the_api)
{ CHECK_EQ(QueryKind(9u).api_type(), 8u);
  CHECK_EQ(QueryKind(12u).api_type(), 11u);
  CHECK_EQ(QueryKind(12u).data_bytes(), 16u);
  CHECK_EQ(QueryKind(13u).api_type(), 12u);
  CHECK_EQ(QueryKind(16u).api_type(), 15u);
  CHECK_EQ(QueryKind(16u).data_bytes(), 4u); }

TEST(query_kind, a_query_past_the_last_is_unknown)
{ CHECK(!QueryKind(17u).known());
  CHECK(!QueryKind(0x1000u).known());
  CHECK_EQ(QueryKind(17u).data_bytes(), 0u); }
