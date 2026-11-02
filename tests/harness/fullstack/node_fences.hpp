#ifndef AV_NODE_FENCES_HPP
#define AV_NODE_FENCES_HPP

#include "miniport/ddi/dxgkargcb_notify_interrupt_data_record.hpp"
#include "core/support/status.hpp"

#include <cstdint>

namespace av {

class NodeFences {
public:
    static constexpr std::uint32_t max_contexts = 16u;

    NodeFences();

    std::uint32_t next() const;
    void          commit(std::uint32_t context);
    void          observe(const DxgkargcbNotifyInterruptDataRecord &data);
    Status        settled(std::uint32_t context) const;
    void          forget(std::uint32_t context);
    std::uint32_t completed() const;
    std::uint32_t vertical_blanks() const;

private:
    enum : std::int32_t { dma_completed = 1, crtc_vsync = 3 };

    std::uint32_t next_;
    std::uint32_t completed_;
    std::uint32_t blanks_;
    std::uint32_t last_[max_contexts];
};

}

#endif
