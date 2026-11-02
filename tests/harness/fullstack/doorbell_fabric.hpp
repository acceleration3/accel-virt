#ifndef AV_DOORBELL_FABRIC_HPP
#define AV_DOORBELL_FABRIC_HPP

#include <cstdint>

namespace av {

class MockPlatform;

class DoorbellFabric {
public:
    static constexpr std::uint32_t max_pages = 16u;
    static constexpr std::uint32_t page_bytes = 4096u;

    explicit DoorbellFabric(MockPlatform &platform);
    DoorbellFabric(const DoorbellFabric &) = delete;
    DoorbellFabric &operator=(const DoorbellFabric &) = delete;

    void         *reserve();
    void          aim(void *page, void *kernel);
    void          blind(void *page);
    void          release(void *page);

    std::uint32_t deliver();
    std::uint32_t delivered() const;
    std::uint32_t dropped() const;

    bool invariants_hold() const;

private:
    std::uint32_t index_of(const void *page) const;

    MockPlatform &platform_;
    alignas(page_bytes) std::uint8_t bytes_[max_pages][page_bytes];
    bool          taken_[max_pages];
    void         *kernel_[max_pages];
    std::uint32_t delivered_;
    std::uint32_t dropped_;
};

}

#endif
