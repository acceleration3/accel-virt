#include "icd/vulkan/handle_bits.hpp"
#include "icd/vulkan/non_dispatchable_handle.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::icd::HandleBits;

namespace {
using Surface = av::NonDispatchableHandle<struct SurfaceTag>;

struct Slot {
    int value;
};

}

TEST(handle_bits, a_pointer_becomes_its_address_and_comes_back)
{ Slot slot = {7};
  const HandleBits bits(&slot);
  CHECK_EQ(bits.value(), static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&slot)));
  CHECK(bits.as<Slot *>() == &slot);
  CHECK(HandleBits(static_cast<Slot *>(nullptr)).value() == 0u); }

TEST(handle_bits, a_64_bit_value_becomes_a_handle_of_either_shape_and_back)
{ const std::uint64_t host = 0xFEDCBA9876543210ull;
  const Surface surface = HandleBits(host).as<Surface>();
  CHECK_EQ(static_cast<std::uint64_t>(surface), host);
  CHECK_EQ(HandleBits(surface).value(), host);
  CHECK_EQ(HandleBits(host).value(), host);
  const HandleBits none(Surface{});
  CHECK_EQ(none.value(), 0u); }

TEST(handle_bits, a_narrower_value_widens_without_its_sign_and_narrows_back)
{ const std::uint32_t address = 0x80001000u;
  const HandleBits bits(address);
  CHECK_EQ(bits.value(), 0x80001000u);
  CHECK_EQ(bits.as<std::uint32_t>(), 0x80001000u);
  CHECK_EQ(HandleBits(std::uint64_t{0x1234567880001000ull}).as<std::uint32_t>(), 0x80001000u); }
