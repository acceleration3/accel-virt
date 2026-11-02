#ifndef AV_PUBLISHED_SLOT_READER_HPP
#define AV_PUBLISHED_SLOT_READER_HPP

#include <stdint.h>

namespace av {

class PublishedSlotReader {
public:
    PublishedSlotReader();
    PublishedSlotReader(const PublishedSlotReader &) = delete;
    PublishedSlotReader &operator=(const PublishedSlotReader &) = delete;

    void take(uint8_t *slots, uint32_t count);
    bool reading() const;
    bool read(uint32_t index, uint64_t sequence, void *payload, uint32_t size) const;

    bool invariants_hold() const;

private:
    uint8_t *slots_;
    uint32_t count_;
};

}

#endif
