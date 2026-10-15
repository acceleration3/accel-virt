/* A running FNV-1a digest over bytes, 64 bits wide: what a program binary's
 * envelope checks its payload and its driver's description by. It catches a
 * binary damaged in an application's cache or cut short, which a host driver
 * would otherwise be handed - rusticl takes a binary with a byte flipped in
 * its middle and says nothing. It is not a seal against a guest that means
 * harm: nothing about it is secret, and a guest could as well hand the host's
 * compiler any source it likes. */
#ifndef AV_EXECUTOR_CL_BINARY_DIGEST_HPP
#define AV_EXECUTOR_CL_BINARY_DIGEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClBinaryDigest {
public:
    ClBinaryDigest();
    ClBinaryDigest(const ClBinaryDigest &) = delete;
    ClBinaryDigest &operator=(const ClBinaryDigest &) = delete;

    void     add(const void *bytes, size_t count);
    /* A string's bytes and then its terminator, so "ab" then "c" and "a" then
     * "bc" digest differently. */
    void     add_text(const char *text);
    uint64_t value() const;

private:
    static constexpr uint64_t offset_basis = 0xCBF29CE484222325ull;
    static constexpr uint64_t prime = 0x00000100000001B3ull;

    uint64_t value_;
};

}  // namespace av

#endif  // AV_EXECUTOR_CL_BINARY_DIGEST_HPP
