#include "device/replay/recording_reader.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

RecordingReader::RecordingReader(const uint8_t *bytes, size_t length)
    : bytes_(bytes), length_(bytes != nullptr ? length : 0u),
      offset_(0u),
      header_(),
      damage_offset_(0u),
      damage_(""),
      records_(0u),
      opened_(false)
{
    AV_SCOPE("RecordingReader::RecordingReader");
}

Status RecordingReader::open()
{
    AV_SCOPE("RecordingReader::open");
    if (opened_) return Status::invalid_state;
    const Status decoded = header_.decode(bytes_, length_);
    if (decoded != Status::ok) {
        static_cast<void>(damaged_by(decoded == Status::not_supported
                                         ? "the recording was written by a later version"
                                         : "the file header is not a recording's"));
        return decoded;
    }
    offset_ = RecordingHeader::encoded_size;
    opened_ = true;
    return Status::ok;
}

bool RecordingReader::opened() const
{
    AV_SCOPE("RecordingReader::opened");
    return opened_;
}

const RecordingHeader &RecordingReader::header() const
{
    AV_SCOPE("RecordingReader::header");
    return header_;
}

Status RecordingReader::next(RecordHeader &record, const uint8_t *&payload)
{
    AV_SCOPE("RecordingReader::next");
    payload = nullptr;
    if (!opened_) return Status::invalid_state;
    if (damage_[0] != '\0') return Status::malformed;
    if (offset_ == length_) return Status::not_found;
    const size_t left = length_ - offset_;
    RecordHeader framed;
    if (framed.decode(bytes_ + offset_, left) != Status::ok)
        return damaged_by("a record header no device wrote");
    if (!framed.fits(left)) return damaged_by("a record length passes the end of the recording");
    record = framed;
    payload = bytes_ + offset_ + RecordHeader::encoded_size;
    offset_ += static_cast<size_t>(framed.span());
    records_++;
    return Status::ok;
}

Status RecordingReader::damaged_by(const char *reason)
{
    AV_SCOPE("RecordingReader::damaged_by");
    damage_offset_ = offset_;
    damage_ = reason;
    return Status::malformed;
}

uint64_t RecordingReader::offset() const
{
    AV_SCOPE("RecordingReader::offset");
    return offset_;
}

uint64_t RecordingReader::records() const
{
    AV_SCOPE("RecordingReader::records");
    return records_;
}

bool RecordingReader::damaged() const
{
    AV_SCOPE("RecordingReader::damaged");
    return damage_[0] != '\0';
}

uint64_t RecordingReader::damage_offset() const
{
    AV_SCOPE("RecordingReader::damage_offset");
    return damage_offset_;
}

const char *RecordingReader::damage() const
{
    AV_SCOPE("RecordingReader::damage");
    return damage_;
}

bool RecordingReader::invariants_hold() const
{
    AV_SCOPE("RecordingReader::invariants_hold");
    if (offset_ > length_) return false;
    if (damage_offset_ > length_) return false;
    if (!opened_) return offset_ == 0u && records_ == 0u;
    return offset_ >= RecordingHeader::encoded_size && header_.invariants_hold();
}

}  // namespace av
