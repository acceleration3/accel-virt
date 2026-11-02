#ifndef AV_GLICD_BUFFER_MAP_TRANSFER_HPP
#define AV_GLICD_BUFFER_MAP_TRANSFER_HPP

#include "icd/common/written_runs.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class BufferMapping;
class BufferShadows;

class BufferMapTransfer {
public:
    explicit BufferMapTransfer(BufferShadows &shadows);
    BufferMapTransfer(const BufferMapTransfer &) = delete;
    BufferMapTransfer &operator=(const BufferMapTransfer &) = delete;

    void start(ClientTransport &wire, BufferMapping &mapping);
    void write_back(ClientTransport &wire, BufferMapping &mapping);

private:
    static constexpr uint64_t word = 8u;

    void     read(ClientTransport &wire, const BufferMapping &mapping);
    void     send(ClientTransport &wire, const BufferMapping &mapping, uint64_t at, uint64_t end);
    void     send_changes(ClientTransport &wire, const BufferMapping &mapping, uint8_t *known);
    void     send_runs(ClientTransport &wire, const BufferMapping &mapping, uint8_t *known);
    uint64_t next_change(const BufferMapping &mapping, const uint8_t *known, uint64_t at) const;
    uint64_t change_end(const BufferMapping &mapping, const uint8_t *known, uint64_t from) const;

    BufferShadows   &shadows_;
    icd::WrittenRuns runs_;
};

}

#endif
