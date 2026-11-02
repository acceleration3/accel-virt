#ifndef AV_DXBC_DIGEST_HPP
#define AV_DXBC_DIGEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class DxbcDigest {
public:
    static constexpr size_t digest_bytes = 16u;
    static constexpr size_t hashed_from = 20u;

    DxbcDigest();

    void compute(const uint8_t *container, size_t size, uint8_t *out);

private:
    static constexpr size_t block_bytes = 64u;

    void reset();
    void update(const uint8_t *data, size_t size);
    void process(const uint8_t *block);

    uint32_t state_[4];
    uint8_t  pending_[block_bytes];
    size_t   buffered_;
};

}

#endif
