#include "harness/fullstack/registered_driver.hpp"

#include "core/miniport/ddi_slot.hpp"

namespace av {

RegisteredDriver::RegisteredDriver()
    : driver_object_(0), registry_path_(0), image_(), entered_(image_.enter(&driver_object_, &registry_path_, ImageRange(0u, 0u), nullptr)),
      dxgkrnl_(), registered_(entered_ >= 0 ? dxgkrnl_.initialize(&table()) : Status::invalid_state) {}

/* A DriverEntry that failed has already left the image. */
RegisteredDriver::~RegisteredDriver()
{
    if (entered_ < 0) return;
    reinterpret_cast<void (*)()>(table().routine(DdiSlot::DxgkDdiUnload))();
    image_.leave();
}

std::int32_t    RegisteredDriver::entered() const { return entered_; }
Status          RegisteredDriver::registered() const { return registered_; }
MockPlatform   &RegisteredDriver::platform() { return image_.platform(); }
const DdiTable &RegisteredDriver::table() { return *image_.platform().registered_table(); }

}  // namespace av
