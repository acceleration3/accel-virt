#include "um/trace/object_security.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

extern "C" LONG NTAPI RtlCreateSecurityDescriptor(PSECURITY_DESCRIPTOR descriptor, ULONG revision);
extern "C" LONG NTAPI RtlCreateAcl(PACL acl, ULONG length, ULONG revision);
extern "C" LONG NTAPI RtlAddAccessAllowedAce(PACL acl, ULONG revision, ACCESS_MASK mask, PSID sid);
extern "C" LONG NTAPI RtlSetDaclSecurityDescriptor(PSECURITY_DESCRIPTOR descriptor, BOOLEAN present, PACL acl,
                                                   BOOLEAN defaulted);

namespace av {

namespace {
constexpr BYTE local_system[] = {1, 1, 0, 0, 0, 0, 0, 5, 18, 0, 0, 0};
constexpr BYTE administrators[] = {1, 2, 0, 0, 0, 0, 0, 5, 32, 0, 0, 0, 0x20, 0x02, 0, 0};

}

ObjectSecurity::ObjectSecurity() : built_(false), acl_{}, descriptor_{}, attributes_{}
{
    AV_SCOPE("ObjectSecurity::ObjectSecurity");
    static_assert(sizeof(SECURITY_ATTRIBUTES) <= attributes_bytes && SECURITY_DESCRIPTOR_MIN_LENGTH <= descriptor_bytes);
    BYTE system_sid[sizeof local_system] = {};
    BYTE admins_sid[sizeof administrators] = {};
    for (size_t byte = 0u; byte < sizeof system_sid; byte++) system_sid[byte] = local_system[byte];
    for (size_t byte = 0u; byte < sizeof admins_sid; byte++) admins_sid[byte] = administrators[byte];
    const auto acl = reinterpret_cast<PACL>(acl_);
    const auto descriptor = static_cast<PSECURITY_DESCRIPTOR>(descriptor_);
    built_ = RtlCreateAcl(acl, acl_bytes, ACL_REVISION) >= 0 &&
             RtlAddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, system_sid) >= 0 &&
             RtlAddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, admins_sid) >= 0 &&
             RtlCreateSecurityDescriptor(descriptor, SECURITY_DESCRIPTOR_REVISION) >= 0 &&
             RtlSetDaclSecurityDescriptor(descriptor, TRUE, acl, FALSE) >= 0;
    auto *const attributes = reinterpret_cast<SECURITY_ATTRIBUTES *>(attributes_);
    attributes->nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes->lpSecurityDescriptor = descriptor;
    attributes->bInheritHandle = FALSE;
}

void *ObjectSecurity::attributes()
{
    AV_SCOPE("ObjectSecurity::attributes");
    return built_ ? attributes_ : nullptr;
}

}
