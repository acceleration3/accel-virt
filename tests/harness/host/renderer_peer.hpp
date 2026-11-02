#ifndef AV_RENDERER_PEER_HPP
#define AV_RENDERER_PEER_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class RendererPeer {
public:
    virtual void started() {}
    virtual void deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                         std::uint32_t count) = 0;
    virtual bool answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                        std::uint32_t capacity_descriptors, std::uint32_t &count) = 0;

protected:
    RendererPeer() = default;
    ~RendererPeer() = default;
    RendererPeer(const RendererPeer &) = delete;
    RendererPeer &operator=(const RendererPeer &) = delete;
};

}

#endif
