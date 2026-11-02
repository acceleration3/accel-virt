#ifndef AV_SNAPSHOT_FILE_HPP
#define AV_SNAPSHOT_FILE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class InspectHost;

class SnapshotFile {
public:
    static constexpr size_t header_bytes = 24u;

    enum class Kind : uint32_t { table = 1u, state = 2u, error = 3u };

    SnapshotFile(InspectHost &host, const char *path);
    SnapshotFile(const SnapshotFile &) = delete;
    SnapshotFile &operator=(const SnapshotFile &) = delete;

    [[nodiscard]] Status write(Kind kind, uint64_t clock, const uint8_t *payload, size_t length);
    [[nodiscard]] Status refuse(uint64_t clock, const char *text);

private:
    InspectHost &host_;
    const char  *path_;
};

}

#endif
