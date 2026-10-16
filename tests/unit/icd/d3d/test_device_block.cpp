#include "harness/framework/test_registry.hpp"
#include "icd/d3d/device_block.hpp"
#include "icd/d3d/device_table.hpp"

#include <cstddef>
#include <cstdint>

using av::d3d::DeviceBlock;
using av::d3d::DeviceTable;

namespace {

struct FakeDevice {
    std::uint32_t serial;
};

constexpr std::uint8_t pattern = 0xA5u;
constexpr std::size_t  guard = 16u;

/* The runtime's block for a device, sized as pfnCalcPrivateDeviceSize
 * answered, with a band past its end the driver must never write. */
struct RuntimeBlock {
    alignas(8) std::uint8_t bytes[DeviceTable::private_bytes + guard];

    RuntimeBlock()
    {
        for (std::uint8_t &byte : bytes) byte = pattern;
    }

    bool untouched(std::size_t from, std::size_t to) const
    {
        for (std::size_t index = from; index < to; index++)
            if (bytes[index] != pattern) return false;
        return true;
    }
};

}  // namespace

TEST(device_block, names_the_device_it_was_bound_to)
{ RuntimeBlock block;
  FakeDevice   device{7u};
  DeviceBlock<FakeDevice>(block.bytes).bind(device);
  CHECK(&DeviceBlock<FakeDevice>(block.bytes).device() == &device);
  CHECK(DeviceBlock<FakeDevice>(block.bytes).device().serial == 7u); }

/* The cookie DeviceTable wrote ahead of the slot, which is how the table finds
 * the device again, and whatever lies past the block, are not the slot's. */
TEST(device_block, writes_only_the_extension_slot)
{ RuntimeBlock block;
  FakeDevice   device{1u};
  DeviceBlock<FakeDevice>(block.bytes).bind(device);
  CHECK(block.untouched(0u, DeviceTable::extension_offset));
  CHECK(!block.untouched(DeviceTable::extension_offset, DeviceTable::private_bytes));
  CHECK(block.untouched(DeviceTable::private_bytes, sizeof block.bytes)); }

/* A deferred context's block names a device of its own, and binding it again
 * moves it - the runtime reuses blocks. */
TEST(device_block, each_block_names_its_own_device_and_a_rebind_replaces_it)
{ RuntimeBlock first, second;
  FakeDevice   one{1u}, two{2u};
  DeviceBlock<FakeDevice>(first.bytes).bind(one);
  DeviceBlock<FakeDevice>(second.bytes).bind(two);
  CHECK(&DeviceBlock<FakeDevice>(first.bytes).device() == &one);
  CHECK(&DeviceBlock<FakeDevice>(second.bytes).device() == &two);
  DeviceBlock<FakeDevice>(first.bytes).bind(two);
  CHECK(&DeviceBlock<FakeDevice>(first.bytes).device() == &two); }
