/* The security every trace object is made with: SYSTEM and Administrators get
 * full access, so rvkinspect.exe - run by the guest agent as SYSTEM - opens
 * another user's section and events. Built with ntdll's own routines, since
 * advapi32 is a DLL the images may not import. */
#ifndef AV_UM_OBJECT_SECURITY_HPP
#define AV_UM_OBJECT_SECURITY_HPP

#include <stdint.h>

namespace av {

class ObjectSecurity {
public:
    ObjectSecurity();
    ObjectSecurity(const ObjectSecurity &) = delete;
    ObjectSecurity &operator=(const ObjectSecurity &) = delete;

    /* A SECURITY_ATTRIBUTES, or null when the descriptor could not be built -
     * the object then takes its default security. */
    void *attributes();

private:
    static constexpr uint32_t acl_bytes = 128u;
    static constexpr uint32_t descriptor_bytes = 64u;
    static constexpr uint32_t attributes_bytes = 32u;

    bool    built_;
    /* Laid out as the structures they hold, so each is aligned as one. */
    alignas(8) uint8_t acl_[acl_bytes];
    alignas(8) uint8_t descriptor_[descriptor_bytes];
    alignas(8) uint8_t attributes_[attributes_bytes];
};

}  // namespace av

#endif  // AV_UM_OBJECT_SECURITY_HPP
