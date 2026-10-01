/* The far end of MockHost's renderer socket: what a test puts where
 * av-renderer would be. Each datagram the device sends is delivered with
 * descriptors of the peer's own - duplicates, as SCM_RIGHTS makes them - which
 * the peer closes. Each receive asks the peer for its next datagram; a peer
 * with none is a renderer that did not answer in time. */
#ifndef AV_RENDERER_PEER_HPP
#define AV_RENDERER_PEER_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class RendererPeer {
public:
    /* The device started a renderer. A peer that stands for one process
     * throughout keeps this empty. */
    virtual void started() {}
    virtual void deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                         std::uint32_t count) = 0;
    /* The next datagram into `buffer`, its length in `length`, and up to
     * `capacity_descriptors` descriptors, which become the device's: false
     * when the peer has nothing to say. */
    virtual bool answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                        std::uint32_t capacity_descriptors, std::uint32_t &count) = 0;

protected:
    RendererPeer() = default;
    ~RendererPeer() = default;
    RendererPeer(const RendererPeer &) = delete;
    RendererPeer &operator=(const RendererPeer &) = delete;
};

}  // namespace av

#endif  // AV_RENDERER_PEER_HPP
