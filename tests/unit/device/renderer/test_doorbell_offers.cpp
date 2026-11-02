#include "core/support/status.hpp"
#include "device/renderer/doorbell_offers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::DoorbellOffers;
using av::Status;

namespace av {

struct DoorbellOffersProbe {
    static void set(DoorbellOffers &offers, std::uint32_t index, int descriptor)
    {
        offers.descriptors_[index] = descriptor;
    }
};

}

TEST(doorbell_offers_offer, a_page_holds_one_descriptor_until_it_is_taken)
{ DoorbellOffers offers;
  CHECK(offers.invariants_hold());
  CHECK_STATUS(offers.offer(0u, 10), Status::ok);
  CHECK_STATUS(offers.offer(DoorbellOffers::pages - 1u, 0), Status::ok);
  CHECK(offers.invariants_hold());
  CHECK_STATUS(offers.offer(0u, 11), Status::conflict);
  int descriptor = -1;
  CHECK_STATUS(offers.take(0u, descriptor), Status::ok);
  CHECK_EQ(descriptor, 10);
  CHECK_STATUS(offers.take(0u, descriptor), Status::not_found);
  CHECK_STATUS(offers.take(DoorbellOffers::pages - 1u, descriptor), Status::ok);
  CHECK_EQ(descriptor, 0);
  CHECK_STATUS(offers.offer(0u, 11), Status::ok);
  CHECK(offers.invariants_hold()); }

TEST(doorbell_offers_offer, a_page_past_the_region_a_negative_or_a_doubly_offered_descriptor_is_refused)
{ DoorbellOffers offers;
  CHECK_STATUS(offers.offer(DoorbellOffers::pages, 10), Status::invalid_parameter);
  CHECK_STATUS(offers.offer(0xFFFFFFFFu, 10), Status::invalid_parameter);
  CHECK_STATUS(offers.offer(1u, -1), Status::invalid_parameter);
  CHECK_STATUS(offers.offer(1u, -7), Status::invalid_parameter);
  CHECK_STATUS(offers.offer(1u, 10), Status::ok);
  CHECK_STATUS(offers.offer(2u, 10), Status::conflict);
  int descriptor = -1;
  CHECK_STATUS(offers.take(2u, descriptor), Status::not_found);
  CHECK_EQ(descriptor, -1);
  CHECK(offers.invariants_hold()); }

TEST(doorbell_offers_take, a_page_with_no_offer_or_past_the_region_is_not_found)
{ DoorbellOffers offers;
  int descriptor = 5;
  CHECK_STATUS(offers.take(0u, descriptor), Status::not_found);
  CHECK_STATUS(offers.take(DoorbellOffers::pages, descriptor), Status::not_found);
  CHECK_STATUS(offers.take(0xFFFFFFFFu, descriptor), Status::not_found);
  CHECK_EQ(descriptor, 5); }

TEST(doorbell_offers_withdraw, drains_every_offer_still_standing_once)
{ DoorbellOffers offers;
  int descriptor = -1;
  CHECK_STATUS(offers.withdraw(descriptor), Status::not_found);
  CHECK_STATUS(offers.offer(3u, 30), Status::ok);
  CHECK_STATUS(offers.offer(9u, 90), Status::ok);
  CHECK_STATUS(offers.offer(DoorbellOffers::pages - 1u, 70), Status::ok);
  CHECK_STATUS(offers.take(9u, descriptor), Status::ok);
  int sum = 0;
  int drained = 0;
  while (offers.withdraw(descriptor) == Status::ok) {
      sum += descriptor;
      drained++;
  }
  CHECK_EQ(drained, 2);
  CHECK_EQ(sum, 100);
  CHECK_STATUS(offers.take(3u, descriptor), Status::not_found);
  CHECK(offers.invariants_hold()); }

TEST(doorbell_offers_invariants, one_descriptor_at_two_pages_or_a_negative_one_is_detected)
{ DoorbellOffers twice;
  CHECK_STATUS(twice.offer(1u, 10), Status::ok);
  CHECK_STATUS(twice.offer(2u, 20), Status::ok);
  av::DoorbellOffersProbe::set(twice, 5u, 10);
  CHECK(!twice.invariants_hold());
  DoorbellOffers negative;
  av::DoorbellOffersProbe::set(negative, 0u, -3);
  CHECK(!negative.invariants_hold()); }
