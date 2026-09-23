#include "device/executor/dxbc_digest.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t shifts[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

/* floor(abs(sin(i + 1)) * 2^32), RFC 1321. */
constexpr uint32_t sines[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

constexpr uint32_t initial[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};

/* The bytes of the padding MD5 starts with: a one bit, then zeroes. */
constexpr uint8_t padding[64] = {0x80u};

constexpr size_t tail_room = 56u;

}  // namespace

DxbcDigest::DxbcDigest() : state_{}, pending_{}, buffered_(0u)
{
    AV_SCOPE("DxbcDigest::DxbcDigest");
    reset();
}

void DxbcDigest::reset()
{
    AV_SCOPE("DxbcDigest::reset");
    for (uint32_t word = 0u; word < 4u; word++) state_[word] = initial[word];
    buffered_ = 0u;
}

void DxbcDigest::process(const uint8_t *block)
{
    AV_SCOPE("DxbcDigest::process");
    uint32_t words[16];
    for (uint32_t index = 0u; index < 16u; index++)
        words[index] = static_cast<uint32_t>(block[index * 4u]) | static_cast<uint32_t>(block[index * 4u + 1u]) << 8u |
                       static_cast<uint32_t>(block[index * 4u + 2u]) << 16u |
                       static_cast<uint32_t>(block[index * 4u + 3u]) << 24u;
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    for (uint32_t round = 0u; round < 64u; round++) {
        uint32_t mixed = 0u;
        uint32_t word = 0u;
        if (round < 16u) {
            mixed = (b & c) | (~b & d);
            word = round;
        } else if (round < 32u) {
            mixed = (d & b) | (~d & c);
            word = (5u * round + 1u) % 16u;
        } else if (round < 48u) {
            mixed = b ^ c ^ d;
            word = (3u * round + 5u) % 16u;
        } else {
            mixed = c ^ (b | ~d);
            word = (7u * round) % 16u;
        }
        const uint32_t sum = a + mixed + sines[round] + words[word];
        a = d;
        d = c;
        c = b;
        b = b + ((sum << shifts[round]) | (sum >> (32u - shifts[round])));
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

void DxbcDigest::update(const uint8_t *data, size_t size)
{
    AV_SCOPE("DxbcDigest::update");
    for (size_t index = 0u; index < size; index++) {
        pending_[buffered_++] = data[index];
        if (buffered_ < block_bytes) continue;
        process(pending_);
        buffered_ = 0u;
    }
}

void DxbcDigest::compute(const uint8_t *container, size_t size, uint8_t *out)
{
    AV_SCOPE("DxbcDigest::compute");
    for (size_t index = 0u; index < digest_bytes; index++) out[index] = 0u;
    if (container == nullptr || size < hashed_from) return;
    reset();
    const uint8_t *const bytes = container + hashed_from;
    const size_t length = size - hashed_from;
    const uint32_t bits = static_cast<uint32_t>(length) * 8u;
    const uint32_t magic = (bits >> 2u) | 1u;
    const uint8_t count[4] = {static_cast<uint8_t>(bits), static_cast<uint8_t>(bits >> 8u),
                              static_cast<uint8_t>(bits >> 16u), static_cast<uint8_t>(bits >> 24u)};
    const uint8_t tail[4] = {static_cast<uint8_t>(magic), static_cast<uint8_t>(magic >> 8u),
                             static_cast<uint8_t>(magic >> 16u), static_cast<uint8_t>(magic >> 24u)};
    const size_t remainder = length % block_bytes;
    const size_t pad = block_bytes - remainder;
    update(bytes, length - remainder);
    if (remainder >= tail_room) {
        update(bytes + length - remainder, remainder);
        update(padding, pad);
        update(count, sizeof(count));
        update(padding + sizeof(count), block_bytes - sizeof(count) - sizeof(tail));
        update(tail, sizeof(tail));
    } else {
        update(count, sizeof(count));
        update(bytes + length - remainder, remainder);
        update(padding, pad - sizeof(count) - sizeof(tail));
        update(tail, sizeof(tail));
    }
    for (uint32_t word = 0u; word < 4u; word++)
        for (uint32_t byte = 0u; byte < 4u; byte++) out[word * 4u + byte] = static_cast<uint8_t>(state_[word] >> (8u * byte));
}

}  // namespace av
