#include "harness/platform/service_value.hpp"

#include <cstring>

namespace av {

ServiceValue::ServiceValue(const char *name, std::uint32_t number) : name_(name), number_(number) {}

std::uint32_t ServiceValue::read(const char *name, std::uint32_t fallback) const
{
    return std::strcmp(name, name_) == 0 ? number_ : fallback;
}

}
