#include "executor/vulkan/issued_binaries.hpp"

#include "core/support/untestable.hpp"
#include "core/trace/function_scope.hpp"

#include <initializer_list>

#include <sys/random.h>

namespace av {

namespace {

class SipDigest {
public:
    explicit SipDigest(uint64_t key) : v0_(key ^ 0x736F6D6570736575ull), v1_(~key ^ 0x646F72616E646F6Dull),
                                       v2_(key ^ 0x6C7967656E657261ull), v3_(~key ^ 0x7465646279746573ull), tail_(0u),
                                       held_(0u), total_(0u)
    {
        AV_SCOPE("SipDigest::SipDigest");
    }

    void add(const void *bytes, size_t count)
    {
        AV_SCOPE("SipDigest::add");
        const uint8_t *const from = static_cast<const uint8_t *>(bytes);
        for (size_t index = 0u; from != nullptr && index < count; index++) {
            tail_ |= static_cast<uint64_t>(from[index]) << (8u * held_);
            total_++;
            if (++held_ == 8u) {
                block(tail_);
                tail_ = 0u;
                held_ = 0u;
            }
        }
    }

    void add_word(uint64_t word)
    {
        AV_SCOPE("SipDigest::add_word");
        add(&word, sizeof word);
    }

    uint64_t value()
    {
        AV_SCOPE("SipDigest::value");
        block(tail_ | (total_ << 56u));
        v2_ ^= 0xFFu;
        round();
        round();
        round();
        round();
        return v0_ ^ v1_ ^ v2_ ^ v3_;
    }

private:
    uint64_t rotated(uint64_t word, uint32_t by) const
    {
        AV_SCOPE("SipDigest::rotated");
        return (word << by) | (word >> (64u - by));
    }

    void round()
    {
        AV_SCOPE("SipDigest::round");
        v0_ += v1_;
        v1_ = rotated(v1_, 13u);
        v1_ ^= v0_;
        v0_ = rotated(v0_, 32u);
        v2_ += v3_;
        v3_ = rotated(v3_, 16u);
        v3_ ^= v2_;
        v0_ += v3_;
        v3_ = rotated(v3_, 21u);
        v3_ ^= v0_;
        v2_ += v1_;
        v1_ = rotated(v1_, 17u);
        v1_ ^= v2_;
        v2_ = rotated(v2_, 32u);
    }

    void block(uint64_t word)
    {
        AV_SCOPE("SipDigest::block");
        v3_ ^= word;
        round();
        round();
        v0_ ^= word;
    }

    uint64_t v0_;
    uint64_t v1_;
    uint64_t v2_;
    uint64_t v3_;
    uint64_t tail_;
    uint32_t held_;
    uint64_t total_;
};

}

IssuedBinaries::IssuedBinaries()
    : first_key_(0u), second_key_(0u), entries_{}, used_(0u), next_(0u), refused_(0u)
{
    AV_SCOPE("IssuedBinaries::IssuedBinaries");
    uint64_t keys[2] = {0x9E3779B97F4A7C15ull, 0xC2B2AE3D27D4EB4Full};
    if (::getrandom(keys, sizeof keys, 0u) != static_cast<ssize_t>(sizeof keys)) {
        AV_UNTESTABLE("the kernel always has random bytes to give at this size");
    }
    first_key_ = keys[0];
    second_key_ = keys[1];
}

IssuedBinaries::IssuedBinaries(uint64_t first_key, uint64_t second_key)
    : first_key_(first_key), second_key_(second_key), entries_{}, used_(0u), next_(0u), refused_(0u)
{
    AV_SCOPE("IssuedBinaries::IssuedBinaries");
}

IssuedBinaries::Pair IssuedBinaries::digest(Kind kind, const void *data, size_t bytes, const void *tag,
                                            size_t tag_bytes) const
{
    AV_SCOPE("IssuedBinaries::digest");
    SipDigest first(first_key_);
    SipDigest second(second_key_);
    for (SipDigest *one : {&first, &second}) {
        one->add_word(static_cast<uint64_t>(kind));
        one->add_word(bytes);
        one->add_word(tag_bytes);
        one->add(tag, tag_bytes);
        one->add(data, bytes);
    }
    return Pair{first.value(), second.value()};
}

uint32_t IssuedBinaries::find(const Pair &pair, size_t bytes) const
{
    AV_SCOPE("IssuedBinaries::find");
    for (uint32_t at = 0u; at < used_; at++)
        if (entries_[at].used_ && entries_[at].first_ == pair.first_ && entries_[at].second_ == pair.second_ &&
            entries_[at].bytes_ == bytes)
            return at;
    return capacity;
}

void IssuedBinaries::issue(Kind kind, const void *data, size_t bytes, const void *tag, size_t tag_bytes)
{
    AV_SCOPE("IssuedBinaries::issue");
    if (data == nullptr || bytes == 0u) return;
    const Pair pair = digest(kind, data, bytes, tag, tag_bytes);
    if (find(pair, bytes) != capacity) return;
    entries_[next_] = Entry{pair.first_, pair.second_, bytes, true};
    next_ = (next_ + 1u) % capacity;
    if (used_ < capacity) used_++;
}

bool IssuedBinaries::issued(Kind kind, const void *data, size_t bytes, const void *tag, size_t tag_bytes) const
{
    AV_SCOPE("IssuedBinaries::issued");
    if (data != nullptr && bytes != 0u && find(digest(kind, data, bytes, tag, tag_bytes), bytes) != capacity) return true;
    refused_++;
    return false;
}

uint32_t IssuedBinaries::count() const
{
    AV_SCOPE("IssuedBinaries::count");
    return used_;
}

uint64_t IssuedBinaries::refused() const
{
    AV_SCOPE("IssuedBinaries::refused");
    return refused_;
}

bool IssuedBinaries::invariants_hold() const
{
    AV_SCOPE("IssuedBinaries::invariants_hold");
    if (used_ > capacity || next_ >= capacity) return false;
    for (uint32_t at = 0u; at < capacity; at++)
        if (entries_[at].used_ != (at < used_)) return false;
    return true;
}

}
