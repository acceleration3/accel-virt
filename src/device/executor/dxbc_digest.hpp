/* The checksum in a DXBC container's header.
 *
 * MD5's compression function over the container past its first twenty bytes,
 * with a finalisation of DXBC's own: the bit count and a derived number are
 * placed where MD5 would put the length, and on which side of the last block
 * depends on how much of it is left. DXVK refuses a shader whose checksum is
 * wrong, and the DDI hands the driver no container at all - the executor
 * builds one - so it has to be computed here, byte for byte as fxc does. */
#ifndef AV_DXBC_DIGEST_HPP
#define AV_DXBC_DIGEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class DxbcDigest {
public:
    static constexpr size_t digest_bytes = 16u;
    /* Where the checksummed part of a container starts: after the magic and
     * the checksum itself. */
    static constexpr size_t hashed_from = 20u;

    DxbcDigest();

    /* The checksum of `size` bytes of container at `container`, into `out`.
     * A container too short to have a header gets zeroes. */
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

}  // namespace av

#endif  // AV_DXBC_DIGEST_HPP
