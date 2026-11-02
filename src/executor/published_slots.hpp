#ifndef AV_PUBLISHED_SLOTS_HPP
#define AV_PUBLISHED_SLOTS_HPP

#include <stdint.h>

namespace av {

class GuestBytes;

class PublishedSlots {
public:
    explicit PublishedSlots(GuestBytes &bytes);
    PublishedSlots(const PublishedSlots &) = delete;
    PublishedSlots &operator=(const PublishedSlots &) = delete;

    [[nodiscard]] bool bind(uint64_t offset, uint32_t count);
    void               unbind();
    bool               bound() const;
    [[nodiscard]] bool write(uint32_t index, uint64_t sequence, const uint8_t *payload, uint32_t size);
    [[nodiscard]] bool publish_error_word(uint64_t value);

    bool invariants_hold() const;

private:
    GuestBytes &bytes_;
    uint64_t     offset_;
    uint32_t     count_;
};

}

#endif
