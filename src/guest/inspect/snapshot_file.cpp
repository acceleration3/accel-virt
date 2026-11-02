#include "guest/inspect/snapshot_file.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/inspect/inspect_host.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {
constexpr uint64_t snapshot_magic = 0x003150414E535641ull;

}

SnapshotFile::SnapshotFile(InspectHost &host, const char *path) : host_(host), path_(path)
{
    AV_SCOPE("SnapshotFile::SnapshotFile");
}

Status SnapshotFile::write(Kind kind, uint64_t clock, const uint8_t *payload, size_t length)
{
    AV_SCOPE("SnapshotFile::write");
    if (length > 0xFFFFFFFFull) return Status::too_large;
    uint8_t    header[header_bytes] = {};
    WireWriter writer(header, sizeof header);
    writer.write_u64(snapshot_magic);
    writer.write_u64(clock);
    writer.write_u32(static_cast<uint32_t>(kind));
    writer.write_u32(static_cast<uint32_t>(length));
    return host_.write_file(path_, header, sizeof header, payload, length);
}

Status SnapshotFile::refuse(uint64_t clock, const char *text)
{
    AV_SCOPE("SnapshotFile::refuse");
    size_t length = 0u;
    while (text[length] != '\0') length++;
    return write(Kind::error, clock, static_cast<const uint8_t *>(static_cast<const void *>(text)), length);
}

}
