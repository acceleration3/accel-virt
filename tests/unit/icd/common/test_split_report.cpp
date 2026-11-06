#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/split_report.hpp"
#include "proto/submission/channel_counter.hpp"

using av::ChannelCounter;
using av::MockTransport;
using av::icd::SplitReport;

TEST(split_report, a_transfer_counts_as_split_at_its_second_piece_and_every_piece_after_counts)
{ MockTransport wire;
  SplitReport   report(wire);
  report.piece();
  CHECK_EQ(wire.noted(ChannelCounter::split_transfers), 0u);
  CHECK_EQ(wire.noted(ChannelCounter::split_pieces), 0u);
  report.piece();
  CHECK_EQ(wire.noted(ChannelCounter::split_transfers), 1u);
  CHECK_EQ(wire.noted(ChannelCounter::split_pieces), 1u);
  report.piece();
  report.piece();
  CHECK_EQ(wire.noted(ChannelCounter::split_transfers), 1u);
  CHECK_EQ(wire.noted(ChannelCounter::split_pieces), 3u); }

TEST(split_report, each_report_is_one_transfer)
{ MockTransport wire;
  SplitReport   first(wire);
  SplitReport   second(wire);
  first.piece();
  second.piece();
  CHECK_EQ(wire.noted(ChannelCounter::split_transfers), 0u);
  first.piece();
  second.piece();
  CHECK_EQ(wire.noted(ChannelCounter::split_transfers), 2u);
  CHECK_EQ(wire.noted(ChannelCounter::split_pieces), 2u); }

TEST(split_report, a_report_without_a_wire_can_be_fed_pieces_and_a_wired_one_still_counts_its_own)
{ SplitReport loose;
  for (int piece = 0; piece < 3; piece++) loose.piece();
  MockTransport wire;
  SplitReport   wired(wire);
  wired.piece();
  wired.piece();
  CHECK_EQ(wire.noted(ChannelCounter::split_transfers), 1u);
  CHECK_EQ(wire.noted(ChannelCounter::split_pieces), 1u); }
