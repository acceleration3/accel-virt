#include "proto/export/export_answer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/export/export_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

constexpr size_t body_size = 32u;

/* The statuses an answer carries, by their number on the wire. */
constexpr Status wire_statuses[] = {Status::ok,        Status::malformed, Status::not_supported,
                                    Status::not_found, Status::no_space,  Status::invalid_state};

}  // namespace

ExportAnswer::ExportAnswer() : ExportAnswer(Status::malformed)
{
    AV_SCOPE("ExportAnswer::ExportAnswer");
}

ExportAnswer::ExportAnswer(Status status)
    : operation_(ExportOperation::unreadable), status_(status), surface_(0u), tag_(0u), greeting_(), layout_(),
      writes_(0u), reason_(0u)
{
    AV_SCOPE("ExportAnswer::ExportAnswer");
}

ExportAnswer::ExportAnswer(const ExportRequest &request, Status status)
    : operation_(request.operation()), status_(status), surface_(request.surface()), tag_(request.tag()),
      greeting_(), layout_(), writes_(0u), reason_(0u)
{
    AV_SCOPE("ExportAnswer::ExportAnswer");
}

ExportAnswer::ExportAnswer(ExportRevocation reason, uint64_t surface)
    : operation_(ExportOperation::revoked), status_(Status::ok), surface_(surface), tag_(0u), greeting_(), layout_(),
      writes_(0u), reason_(static_cast<uint32_t>(reason))
{
    AV_SCOPE("ExportAnswer::ExportAnswer");
}

void ExportAnswer::greet(const ExportGreeting &greeting)
{
    AV_SCOPE("ExportAnswer::greet");
    greeting_ = greeting;
}

void ExportAnswer::describe(const ExportedLayout &layout)
{
    AV_SCOPE("ExportAnswer::describe");
    layout_ = layout;
}

void ExportAnswer::count(uint64_t writes)
{
    AV_SCOPE("ExportAnswer::count");
    writes_ = writes;
}

ExportOperation ExportAnswer::operation() const
{
    AV_SCOPE("ExportAnswer::operation");
    return operation_;
}

Status ExportAnswer::status() const
{
    AV_SCOPE("ExportAnswer::status");
    return status_;
}

uint64_t ExportAnswer::surface() const
{
    AV_SCOPE("ExportAnswer::surface");
    return surface_;
}

uint64_t ExportAnswer::tag() const
{
    AV_SCOPE("ExportAnswer::tag");
    return tag_;
}

const ExportGreeting &ExportAnswer::greeting() const
{
    AV_SCOPE("ExportAnswer::greeting");
    return greeting_;
}

const ExportedLayout &ExportAnswer::layout() const
{
    AV_SCOPE("ExportAnswer::layout");
    return layout_;
}

uint64_t ExportAnswer::writes() const
{
    AV_SCOPE("ExportAnswer::writes");
    return writes_;
}

uint32_t ExportAnswer::reason() const
{
    AV_SCOPE("ExportAnswer::reason");
    return reason_;
}

uint32_t ExportAnswer::descriptors() const
{
    AV_SCOPE("ExportAnswer::descriptors");
    const bool hands_over = operation_ == ExportOperation::export_surface || operation_ == ExportOperation::sync;
    return hands_over && status_ == Status::ok ? 1u : 0u;
}

uint32_t ExportAnswer::wire_status() const
{
    AV_SCOPE("ExportAnswer::wire_status");
    for (uint32_t wire = 0u; wire < sizeof wire_statuses / sizeof wire_statuses[0]; wire++)
        if (wire_statuses[wire] == status_) return wire;
    return no_wire_status;
}

bool ExportAnswer::read_status(uint32_t wire)
{
    AV_SCOPE("ExportAnswer::read_status");
    if (wire >= sizeof wire_statuses / sizeof wire_statuses[0]) return false;
    status_ = wire_statuses[wire];
    return true;
}

bool ExportAnswer::body_empty() const
{
    AV_SCOPE("ExportAnswer::body_empty");
    return greeting_.empty() && layout_.empty() && writes_ == 0u;
}

/* A reason is only ever a revocation's: no constructor gives another answer
 * one, and decode reads that word as reserved in every other. */
bool ExportAnswer::fields_valid() const
{
    AV_SCOPE("ExportAnswer::fields_valid");
    if (wire_status() == no_wire_status) return false;
    switch (operation_) {
    case ExportOperation::unreadable:
        return (status_ == Status::malformed || status_ == Status::not_supported) && surface_ == 0u && tag_ == 0u &&
               body_empty();
    case ExportOperation::revoked:
        return status_ == Status::ok && tag_ == 0u && greeting_.empty() && layout_.empty() && writes_ == 0u &&
               ((reason_ == static_cast<uint32_t>(ExportRevocation::released) && surface_ != 0u) ||
                (reason_ == static_cast<uint32_t>(ExportRevocation::reset) && surface_ == 0u));
    case ExportOperation::hello:
        return surface_ == 0u && layout_.empty() && writes_ == 0u &&
               (status_ == Status::ok ? greeting_.valid() : greeting_.empty());
    case ExportOperation::export_surface:
        return surface_ != 0u && greeting_.empty() && writes_ == 0u &&
               (status_ == Status::ok ? layout_.describes_a_picture() : layout_.empty());
    case ExportOperation::sync:
        return surface_ != 0u && greeting_.empty() && layout_.empty() &&
               (status_ == Status::ok || writes_ == 0u);
    case ExportOperation::release: return surface_ != 0u && body_empty();
    default: return false;
    }
}

Status ExportAnswer::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ExportAnswer::encode");
    if (capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(major);
    writer.write_u16(minor);
    writer.write_u32(static_cast<uint32_t>(operation_));
    writer.write_u32(wire_status());
    writer.write_u64(surface_);
    writer.write_u64(tag_);
    switch (operation_) {
    case ExportOperation::hello:
        writer.copy_bytes(greeting_.uuid(), ExportGreeting::uuid_bytes);
        writer.write_u32(greeting_.domain());
        writer.write_u8(greeting_.bus());
        writer.write_u8(greeting_.device());
        writer.write_u8(greeting_.function());
        writer.write_u8(greeting_.on_pci() ? on_pci_flag : 0u);
        writer.write_zeros(8u);
        break;
    case ExportOperation::export_surface:
        writer.write_u32(layout_.width());
        writer.write_u32(layout_.height());
        writer.write_u32(layout_.fourcc());
        writer.write_u32(layout_.stride());
        writer.write_u32(layout_.offset());
        writer.write_zeros(4u);
        writer.write_u64(layout_.modifier());
        break;
    case ExportOperation::sync:
        writer.write_u64(writes_);
        writer.write_zeros(body_size - sizeof(uint64_t));
        break;
    case ExportOperation::revoked:
        writer.write_u32(reason_);
        writer.write_zeros(body_size - sizeof(uint32_t));
        break;
    default: writer.write_zeros(body_size); break;
    }
    return writer.status();
}

Status ExportAnswer::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("ExportAnswer::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t found_major = reader.read_u16();
    reader.skip(sizeof(uint16_t));
    if (found_magic != magic) return Status::malformed;
    if (found_major != major) return Status::not_supported;
    ExportAnswer candidate;
    candidate.operation_ = static_cast<ExportOperation>(reader.read_u32());
    if (!candidate.read_status(reader.read_u32())) return Status::malformed;
    candidate.surface_ = reader.read_u64();
    candidate.tag_ = reader.read_u64();
    uint32_t reserved = 0u;
    if (candidate.operation_ == ExportOperation::hello) {
        uint8_t uuid[ExportGreeting::uuid_bytes] = {};
        reader.copy_bytes(uuid, sizeof uuid);
        const uint32_t domain = reader.read_u32();
        const uint8_t  bus = reader.read_u8();
        const uint8_t  device = reader.read_u8();
        const uint8_t  function = reader.read_u8();
        const uint8_t  flags = reader.read_u8();
        reserved = reader.read_u32() | reader.read_u32() | (flags & ~on_pci_flag);
        candidate.greeting_ = (flags & on_pci_flag) != 0u ? ExportGreeting(uuid, domain, bus, device, function)
                                                          : ExportGreeting(uuid);
        /* Without the flag the address fields must be 0; the greeting built
         * without them cannot say otherwise, so they are kept as reserved. */
        if ((flags & on_pci_flag) == 0u) reserved |= domain | bus | device | function;
    } else if (candidate.operation_ == ExportOperation::export_surface) {
        const uint32_t width = reader.read_u32();
        const uint32_t height = reader.read_u32();
        const uint32_t fourcc = reader.read_u32();
        const uint32_t stride = reader.read_u32();
        const uint32_t offset = reader.read_u32();
        reserved = reader.read_u32();
        candidate.layout_ = ExportedLayout(width, height, fourcc, stride, offset, reader.read_u64());
    } else if (candidate.operation_ == ExportOperation::sync) {
        candidate.writes_ = reader.read_u64();
        for (size_t word = 0u; word < (body_size - sizeof(uint64_t)) / sizeof(uint64_t); word++)
            reserved |= reader.read_u64() != 0u ? 1u : 0u;
    } else {
        candidate.reason_ = reader.read_u32();
        for (size_t word = 0u; word < (body_size - sizeof(uint32_t)) / sizeof(uint32_t); word++)
            reserved |= reader.read_u32();
        /* Only a revocation carries a reason in its first word. */
        if (candidate.operation_ != ExportOperation::revoked) reserved |= candidate.reason_;
        if (candidate.operation_ != ExportOperation::revoked) candidate.reason_ = 0u;
    }
    if (reserved != 0u || !candidate.fields_valid() || descriptor_count != candidate.descriptors())
        return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
