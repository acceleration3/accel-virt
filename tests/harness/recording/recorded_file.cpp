#include "harness/recording/recorded_file.hpp"

#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

namespace av {

RecordedFile::RecordedFile(MockHost &host, const char *path)
    : bytes_(host.recording_bytes(path)),
      length_(host.recording_length(path)),
      offset_(RecordingHeader::encoded_size),
      header_()
{
    if (bytes_ == nullptr || length_ < RecordingHeader::encoded_size) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "no recording was written to %s", path);
        length_ = 0u;
        offset_ = 0u;
        return;
    }
    if (header_.decode(bytes_, RecordingHeader::encoded_size) != Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the recording's file header did not decode");
}

const RecordingHeader &RecordedFile::header() const
{
    return header_;
}

bool RecordedFile::next(RecordHeader &record, const std::uint8_t *&payload)
{
    if (offset_ >= length_) return false;
    if (record.decode(bytes_ + offset_, length_ - offset_) != Status::ok) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a record did not decode at offset %zu",
                                                offset_);
        return false;
    }
    if (!record.fits(length_ - offset_)) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a record ran past the recording");
        return false;
    }
    payload = bytes_ + offset_ + RecordHeader::encoded_size;
    offset_ += static_cast<std::size_t>(record.span());
    return true;
}

std::uint32_t RecordedFile::remaining()
{
    std::uint32_t       count = 0u;
    RecordHeader        record;
    const std::uint8_t *payload = nullptr;
    while (next(record, payload)) count++;
    return count;
}

const std::uint8_t *RecordedFile::expect(RecordKind kind, RecordHeader &record)
{
    const std::uint8_t *payload = nullptr;
    if (!next(record, payload)) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the recording ended before kind %u",
                                                static_cast<unsigned>(kind));
        return nullptr;
    }
    if (record.kind() == kind) return payload;
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "expected record kind %u, found %u",
                                            static_cast<unsigned>(kind),
                                            static_cast<unsigned>(record.kind()));
    return nullptr;
}

}
