/* One DWORD under the driver's service key, as the kernel's registry would
 * answer KernelPlatform::read_service_u32 for it. DriverImage::enter builds its
 * platform itself, so a value DriverEntry reads is handed to the platform's
 * constructor rather than set on it afterwards. */
#ifndef AV_SERVICE_VALUE_HPP
#define AV_SERVICE_VALUE_HPP

#include <cstdint>

namespace av {

class ServiceValue {
public:
    ServiceValue(const char *name, std::uint32_t number);

    /* `number` when `name` is this value's, `fallback` otherwise. */
    std::uint32_t read(const char *name, std::uint32_t fallback) const;

private:
    const char   *name_;
    std::uint32_t number_;
};

}  // namespace av

#endif  // AV_SERVICE_VALUE_HPP
