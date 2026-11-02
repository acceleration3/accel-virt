#ifndef AV_SERVICE_VALUE_HPP
#define AV_SERVICE_VALUE_HPP

#include <cstdint>

namespace av {

class ServiceValue {
public:
    ServiceValue(const char *name, std::uint32_t number);

    std::uint32_t read(const char *name, std::uint32_t fallback) const;

private:
    const char   *name_;
    std::uint32_t number_;
};

}

#endif
