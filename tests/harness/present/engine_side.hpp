#ifndef AV_ENGINE_SIDE_HPP
#define AV_ENGINE_SIDE_HPP

#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "device/present/present_frames.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>

namespace av {

class EngineSide {
public:
    explicit EngineSide(MappedVirtioFunction &mapped);
    ~EngineSide();
    EngineSide(const EngineSide &) = delete;
    EngineSide &operator=(const EngineSide &) = delete;

    void          hold(bool held);
    void          refuse_page();
    void          accept_page();
    void          truncate_page();
    void          serve();
    std::uint32_t presents() const;
    void          frame(const SurfacePoint &frame, const AvFrameView &view);
    void          copy_between_surfaces(bool refuse);
    void          texture(std::uint64_t surface, const AvFrameView &view);
    SurfacePair   copied() const;
    std::uint32_t copied_regions() const;

private:
    static void          stall(void *context);
    static std::uint32_t answer(void *context, const std::uint8_t *request, std::uint32_t length, std::uint8_t *answer,
                                std::uint32_t room);

    MockPlatform         &platform_;
    FakeVirtqueueDevice   queue_;
    MockHost              host_;
    FenceInterrupts       interrupts_;
    PresentEngine         engine_;
    RecordingContextPlane plane_;
    PresentFrames         snapshots_;
    bool                  held_ = false;
    bool                  refuse_page_ = false;
    bool                  truncate_page_ = false;
    std::uint32_t         presents_ = 0u;
    std::uint8_t          request_[0x9000] = {};
};

}

#endif
