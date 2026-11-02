#ifndef AV_FNV1A_DIGEST_HPP
#define AV_FNV1A_DIGEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class Fnv1aDigest {
public:
    Fnv1aDigest();
    explicit Fnv1aDigest(uint64_t salt);
    Fnv1aDigest(const Fnv1aDigest &) = delete;
    Fnv1aDigest &operator=(const Fnv1aDigest &) = delete;

    void     add(const void *bytes, size_t count);
    void     add_text(const char *text);
    void     add_word(uint64_t word);
    uint64_t value() const;

private:
    static constexpr uint64_t offset_basis = 0xCBF29CE484222325ull;
    static constexpr uint64_t prime = 0x00000100000001B3ull;

    uint64_t value_;
};

}

#endif
