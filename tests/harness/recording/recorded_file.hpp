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
    RecordedFile(MockHost &host, const char *path);

    const RecordingHeader &header() const;
    bool                   next(RecordHeader &record, const std::uint8_t *&payload);
    std::uint32_t          remaining();
    const std::uint8_t    *expect(RecordKind kind, RecordHeader &record);

private:
    const std::uint8_t *bytes_;
    std::size_t         length_;
    std::size_t         offset_;
    RecordingHeader     header_;
};

}

#endif
