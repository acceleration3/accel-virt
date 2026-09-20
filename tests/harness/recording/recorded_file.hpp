/* What a WireRecorder left in a MockHost's recording, walked record by record,
 * so a test judges what the device wrote down rather than that it wrote
 * something. It decodes with the same format classes the device encodes with
 * and fails the test at the first thing that does not decode - it is a fixture
 * over bytes this side wrote, not the fuzzed reader. */
#ifndef AV_RECORDED_FILE_HPP
#define AV_RECORDED_FILE_HPP

#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_header.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockHost;

class RecordedFile {
public:
    /* The recording `host` holds at `path`; a file with no header fails the
     * test at once. */
    RecordedFile(MockHost &host, const char *path);

    const RecordingHeader &header() const;
    /* The next record and its payload; false at the end. */
    bool                   next(RecordHeader &record, const std::uint8_t *&payload);
    /* The records left, whatever they are. */
    std::uint32_t          remaining();
    /* The next record, which must be of `kind`: its payload, or null with the
     * test failed. */
    const std::uint8_t    *expect(RecordKind kind, RecordHeader &record);

private:
    const std::uint8_t *bytes_;
    std::size_t         length_;
    std::size_t         offset_;
    RecordingHeader     header_;
};

}  // namespace av

#endif  // AV_RECORDED_FILE_HPP
