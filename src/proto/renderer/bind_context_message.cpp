#include "proto/renderer/bind_context_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BindContextMessage::BindContextMessage()
    : context_(0u), doorbell_(0u), flags_(0u), setup_(nullptr), setup_length_(0u)
{
    AV_SCOPE("BindContextMessage::BindContextMessage");
}

BindContextMessage::BindContextMessage(uint32_t context, const uint8_t *setup, uint32_t setup_length)
    : context_(context), doorbell_(0u), flags_(0u), setup_(setup),
      setup_length_(setup != nullptr ? setup_length : 0u)
{
    AV_SCOPE("BindContextMessage::BindContextMessage");
}

BindContextMessage::BindContextMessage(uint32_t context, const uint8_t *setup, uint32_t setup_length,
                                       uint32_t doorbell)
    : context_(context), doorbell_(doorbell), flags_(has_doorbell_flag), setup_(setup),
      setup_length_(setup != nullptr ? setup_length : 0u)
{
    AV_SCOPE("BindContextMessage::BindContextMessage");
}

uint32_t BindContextMessage::context() const
{
    AV_SCOPE("BindContextMessage::context");
    return context_;
}

bool BindContextMessage::has_doorbell() const
{
    AV_SCOPE("BindContextMessage::has_doorbell");
    return (flags_ & has_doorbell_flag) != 0u;
}

uint32_t BindContextMessage::doorbell() const
{
    AV_SCOPE("BindContextMessage::doorbell");
    return doorbell_;
}

const uint8_t *BindContextMessage::setup() const
{
    AV_SCOPE("BindContextMessage::setup");
    return setup_;
}

uint32_t BindContextMessage::setup_length() const
{
    AV_SCOPE("BindContextMessage::setup_length");
    return setup_length_;
}

uint32_t BindContextMessage::descriptors() const
{
    AV_SCOPE("BindContextMessage::descriptors");
    return has_doorbell() ? 1u : 0u;
}

size_t BindContextMessage::encoded_length() const
{
    AV_SCOPE("BindContextMessage::encoded_length");
    return fixed_size + setup_length_;
}

bool BindContextMessage::fields_valid() const
{
    AV_SCOPE("BindContextMessage::fields_valid");
    const bool known_flags = (flags_ & ~has_doorbell_flag) == 0u;
    const bool doorbell_named = has_doorbell() || doorbell_ == 0u;
    return context_ != 0u && known_flags && doorbell_named && setup_length_ != 0u &&
           setup_length_ <= ContextSetup::encoded_size;
}

Status BindContextMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BindContextMessage::encode");
    if (capacity < encoded_length()) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(context_);
    writer.write_u32(flags_);
    writer.write_u32(doorbell_);
    writer.write_u32(setup_length_);
    writer.copy_bytes(setup_, setup_length_);
    return writer.status();
}

Status BindContextMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("BindContextMessage::decode");
    WireReader         reader(bytes, length);
    BindContextMessage candidate;
    candidate.context_ = reader.read_u32();
    candidate.flags_ = reader.read_u32();
    candidate.doorbell_ = reader.read_u32();
    candidate.setup_length_ = reader.read_u32();
    if (reader.status() != Status::ok) return reader.status();
    if (candidate.setup_length_ > ContextSetup::encoded_size || length != candidate.encoded_length())
        return Status::malformed;
    candidate.setup_ = bytes + fixed_size;
    if (!candidate.fields_valid() || descriptor_count != candidate.descriptors()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
