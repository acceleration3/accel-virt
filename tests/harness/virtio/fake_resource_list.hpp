/* Builds the bytes of a CM_RESOURCE_LIST as dxgkrnl passes it in
 * DXGK_DEVICE_INFO::TranslatedResourceList: full descriptors, each with its
 * partial descriptors, counts kept current as entries are appended. */
#ifndef AV_FAKE_RESOURCE_LIST_HPP
#define AV_FAKE_RESOURCE_LIST_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class FakeResourceList {
public:
    static constexpr std::size_t capacity = 4096u;

    /* One full descriptor, ready for partial descriptors. */
    FakeResourceList();

    void begin_full_descriptor();
    void add_memory(std::uint64_t start, std::uint32_t length);
    /* `flags` is one of CM_RESOURCE_MEMORY_LARGE_40/48/64; `stored` the encoded length. */
    void add_large_memory(std::uint64_t start, std::uint16_t flags, std::uint32_t stored);
    void add_message_interrupt();
    void add_line_interrupt();
    void add_port(std::uint64_t start, std::uint32_t length);

    void set_list_count(std::uint32_t count);
    void set_partial_count(std::uint32_t count);

    const std::uint8_t *bytes() const;
    std::uint8_t       *mutable_bytes();
    std::size_t         size() const;

private:
    void add_partial(std::uint8_t type, std::uint16_t flags, std::uint64_t first, std::uint32_t second);
    void put(std::size_t offset, std::uint64_t value, std::size_t width);

    std::uint8_t  bytes_[capacity];
    std::size_t   size_;
    std::uint32_t full_count_;
    std::size_t   current_full_;
    std::uint32_t partial_count_;
};

}  // namespace av

#endif  // AV_FAKE_RESOURCE_LIST_HPP
