#ifndef AV_UM_OBJECT_SECURITY_HPP
#define AV_UM_OBJECT_SECURITY_HPP

#include <stdint.h>

namespace av {

class ObjectSecurity {
public:
    ObjectSecurity();
    ObjectSecurity(const ObjectSecurity &) = delete;
    ObjectSecurity &operator=(const ObjectSecurity &) = delete;

    void *attributes();

private:
    static constexpr uint32_t acl_bytes = 128u;
    static constexpr uint32_t descriptor_bytes = 64u;
    static constexpr uint32_t attributes_bytes = 32u;

    bool    built_;
    alignas(8) uint8_t acl_[acl_bytes];
    alignas(8) uint8_t descriptor_[descriptor_bytes];
    alignas(8) uint8_t attributes_[attributes_bytes];
};

}

#endif
