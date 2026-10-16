/* A running 64-bit FNV-1a digest: bytes folded one at a time, or a word
 * folded whole as one step. What an OpenCL program binary's envelope checks
 * its payload and its driver's description by - it catches a binary damaged
 * in an application's cache or cut short, which rusticl would take with a
 * byte flipped and say nothing - and what names a guest page list by its
 * frames. Not a seal: nothing about it is secret. */
#ifndef AV_FNV1A_DIGEST_HPP
#define AV_FNV1A_DIGEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class Fnv1aDigest {
public:
    Fnv1aDigest();
    /* Starts from the offset basis with `salt` folded in, unmultiplied: a
     * digest of something whose length leads it. */
    explicit Fnv1aDigest(uint64_t salt);
    Fnv1aDigest(const Fnv1aDigest &) = delete;
    Fnv1aDigest &operator=(const Fnv1aDigest &) = delete;

    void     add(const void *bytes, size_t count);
    /* A string's bytes and then its terminator, so "ab" then "c" and "a" then
     * "bc" digest differently. */
    void     add_text(const char *text);
    void     add_word(uint64_t word);
    uint64_t value() const;

private:
    static constexpr uint64_t offset_basis = 0xCBF29CE484222325ull;
    static constexpr uint64_t prime = 0x00000100000001B3ull;

    uint64_t value_;
};

}  // namespace av

#endif  // AV_FNV1A_DIGEST_HPP
