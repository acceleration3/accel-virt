#ifndef AV_SCANOUT_COMMANDS_HPP
#define AV_SCANOUT_COMMANDS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class ControlRequest;
class DisplayHeads;
class Host;

class ScanoutCommands {
public:
    enum : uint32_t {
        resource_flush = 0x0104u,
        set_scanout_blob = 0x010Du,
    };

    ScanoutCommands(Host &host, BlobTable &blobs, DisplayHeads &heads);
    ScanoutCommands(const ScanoutCommands &) = delete;
    ScanoutCommands &operator=(const ScanoutCommands &) = delete;

    bool answer(ControlRequest &request, uint32_t type);

private:
    uint32_t show(ControlRequest &request);
    uint32_t shown_answer(Status status) const;
    bool     flush(ControlRequest &request, uint32_t &response);

    Host         &host_;
    BlobTable    &blobs_;
    DisplayHeads &heads_;
};

}

#endif
