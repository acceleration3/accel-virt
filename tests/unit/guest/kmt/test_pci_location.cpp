#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"

TEST(pci_location, carries_the_bus_device_and_function_it_was_made_with)
{ const av::PciLocation location(3u, 17u, 1u);
  CHECK_EQ(location.bus(), 3u);
  CHECK_EQ(location.device(), 17u);
  CHECK_EQ(location.function(), 1u);
  const av::PciLocation none;
  CHECK_EQ(none.bus(), 0u);
  CHECK_EQ(none.device(), 0u);
  CHECK_EQ(none.function(), 0u); }
