#ifndef AV_ISSUED_BINARIES_HPP
#define AV_ISSUED_BINARIES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class IssuedBinaries {
public:
    enum class Kind : uint8_t { shader, pipeline };

    static constexpr uint32_t capacity = 2048u;

    IssuedBinaries();
    IssuedBinaries(uint64_t first_key, uint64_t second_key);
    IssuedBinaries(const IssuedBinaries &) = delete;
    IssuedBinaries &operator=(const IssuedBinaries &) = delete;

    void     issue(Kind kind, const void *data, size_t bytes, const void *tag = nullptr, size_t tag_bytes = 0u);
    bool     issued(Kind kind, const void *data, size_t bytes, const void *tag = nullptr, size_t tag_bytes = 0u) const;
    uint32_t count() const;
    uint64_t refused() const;
    bool     invariants_hold() const;

private:
    struct Entry {
        uint64_t first_;
        uint64_t second_;
        uint64_t bytes_;
        bool     used_;
    };

    struct Pair {
        uint64_t first_;
        uint64_t second_;
    };

    Pair     digest(Kind kind, const void *data, size_t bytes, const void *tag, size_t tag_bytes) const;
    uint32_t find(const Pair &pair, size_t bytes) const;

    uint64_t       first_key_;
    uint64_t       second_key_;
    Entry          entries_[capacity];
    uint32_t       used_;
    uint32_t       next_;
    mutable uint64_t refused_;
};

}

#endif
