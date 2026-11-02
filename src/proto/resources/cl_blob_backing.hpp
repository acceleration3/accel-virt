#ifndef AV_PROTO_CL_BLOB_BACKING_HPP
#define AV_PROTO_CL_BLOB_BACKING_HPP

#include <stdint.h>

namespace av {

struct ClBlobBackingProbe;

class ClBlobBacking {
public:
    static constexpr uint64_t property = 0x4156000000000001ull;
    static constexpr uint64_t import_property = 0x4156000000000002ull;
    static constexpr uint32_t list_words = 5u;

    ClBlobBacking();

    void set(uint64_t offset);
    void set_in(uint64_t memory, uint64_t offset);
    bool read(const uint64_t *properties);
    bool write(uint64_t *list) const;

    bool     held() const;
    uint64_t offset() const;
    uint64_t memory() const;

    bool invariants_hold() const;

private:
    friend struct ClBlobBackingProbe;

    static constexpr uint32_t max_words = 6u;

    bool refuse();

    uint64_t offset_;
    uint64_t memory_;
    bool     held_;
};

}

#endif
