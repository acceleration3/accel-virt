#include "harness/device/fake_common_config.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

namespace {

constexpr std::uint8_t  features_ok = 8u;
constexpr std::uint8_t  device_needs_reset = 0x40u;
constexpr std::uint16_t no_vector = 0xFFFFu;

}  // namespace

FakeCommonConfig::FakeCommonConfig()
    : offered_(0u), device_feature_select_(0u), driver_feature_select_(0u), driver_features_(0u),
      config_vector_(no_vector), queue_count_(0u), status_(0u), history_{}, history_count_(0u),
      queue_select_(0u), queues_{}, absent_{}, reset_reads_(0u), refuse_features_ok_(false),
      refuse_vectors_(false), refuse_queue_enable_(false), needs_reset_(false), status_sink_(nullptr),
      status_context_(nullptr) {}

std::uint64_t FakeCommonConfig::length() const { return 0x1000u; }

void FakeCommonConfig::unexpected(const char *access, std::uint64_t offset)
{
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "unexpected %s at common config 0x%llx",
                                            access, static_cast<unsigned long long>(offset));
}

FakeCommonConfig::Queue *FakeCommonConfig::selected()
{
    return queue_select_ < queue_count_ ? &queues_[queue_select_] : &absent_;
}

std::uint8_t FakeCommonConfig::read_u8(std::uint64_t offset)
{
    if (offset == 0x14u) {
        if (reset_reads_ != 0u) { reset_reads_--; return status_ == 0u ? 1u : status_; }
        return static_cast<std::uint8_t>(status_ | (needs_reset_ ? device_needs_reset : 0u));
    }
    if (offset == 0x15u) return 0u;
    unexpected("read_u8", offset);
    return 0u;
}

std::uint16_t FakeCommonConfig::read_u16(std::uint64_t offset)
{
    switch (offset) {
    case 0x10u: return config_vector_;
    case 0x12u: return queue_count_;
    case 0x16u: return queue_select_;
    case 0x18u: return selected()->size;
    case 0x1Au: return selected()->vector;
    case 0x1Cu: return selected()->enable;
    case 0x1Eu: return selected()->notify_off;
    default: unexpected("read_u16", offset); return 0u;
    }
}

std::uint32_t FakeCommonConfig::read_u32(std::uint64_t offset)
{
    if (offset == 0x04u) return static_cast<std::uint32_t>(device_feature_select_ == 0u ? offered_ : offered_ >> 32u);
    if (offset == 0x0Cu) return static_cast<std::uint32_t>(driver_feature_select_ == 0u ? driver_features_ : driver_features_ >> 32u);
    unexpected("read_u32", offset);
    return 0u;
}

void FakeCommonConfig::write_u8(std::uint64_t offset, std::uint8_t value)
{
    if (offset != 0x14u) { unexpected("write_u8", offset); return; }
    if (history_count_ < history_capacity) history_[history_count_] = value;
    history_count_++;
    if (value == 0u) {
        driver_features_ = 0u;
        for (Queue &queue : queues_) { queue.enable = 0u; queue.vector = no_vector; queue.size = queue.max_size; }
        config_vector_ = no_vector;
    }
    const bool refused = (value & features_ok) != 0u &&
                         (refuse_features_ok_ || (driver_features_ & ~offered_) != 0u);
    status_ = refused ? static_cast<std::uint8_t>(value & ~features_ok) : value;
    if (status_sink_ != nullptr) status_sink_(status_context_, status_);
}

void FakeCommonConfig::write_u16(std::uint64_t offset, std::uint16_t value)
{
    switch (offset) {
    case 0x10u: config_vector_ = refuse_vectors_ ? no_vector : value; return;
    case 0x16u: queue_select_ = value; return;
    case 0x18u: selected()->size = value; return;
    case 0x1Au: selected()->vector = refuse_vectors_ ? no_vector : value; return;
    case 0x1Cu: selected()->enable = refuse_queue_enable_ ? 0u : value; return;
    default: unexpected("write_u16", offset); return;
    }
}

void FakeCommonConfig::write_u32(std::uint64_t offset, std::uint32_t value)
{
    switch (offset) {
    case 0x00u: device_feature_select_ = value; return;
    case 0x08u: driver_feature_select_ = value; return;
    case 0x0Cu:
        driver_features_ = driver_feature_select_ == 0u
            ? (driver_features_ & ~std::uint64_t{0xFFFFFFFFu}) | value
            : (driver_features_ & 0xFFFFFFFFu) | (std::uint64_t{value} << 32u);
        return;
    default: unexpected("write_u32", offset); return;
    }
}

void FakeCommonConfig::write_u64(std::uint64_t offset, std::uint64_t value)
{
    switch (offset) {
    case 0x20u: selected()->desc = value; return;
    case 0x28u: selected()->driver = value; return;
    case 0x30u: selected()->device = value; return;
    default: unexpected("write_u64", offset); return;
    }
}

void FakeCommonConfig::offer_features(std::uint64_t features) { offered_ = features; }

void FakeCommonConfig::set_queues(std::uint16_t count, std::uint16_t max_size)
{
    queue_count_ = count;
    for (std::uint16_t i = 0u; i < max_queues; i++) {
        queues_[i] = Queue{};
        queues_[i].max_size = max_size;
        queues_[i].size = max_size;
        queues_[i].vector = no_vector;
    }
}

void FakeCommonConfig::set_queue_max_size(std::uint16_t index, std::uint16_t max_size)
{
    queues_[index].max_size = max_size;
    queues_[index].size = max_size;
}

void FakeCommonConfig::set_queue_notify_off(std::uint16_t index, std::uint16_t notify_off)
{
    queues_[index].notify_off = notify_off;
}

void FakeCommonConfig::refuse_features_ok()        { refuse_features_ok_ = true; }
void FakeCommonConfig::delay_reset(std::uint32_t reads) { reset_reads_ = reads; }
void FakeCommonConfig::refuse_vectors()            { refuse_vectors_ = true; }
void FakeCommonConfig::refuse_queue_enable()       { refuse_queue_enable_ = true; }
void FakeCommonConfig::need_reset()                { needs_reset_ = true; }

void FakeCommonConfig::on_status(StatusSink sink, void *context)
{
    status_sink_ = sink;
    status_context_ = context;
}

std::uint64_t FakeCommonConfig::driver_features() const { return driver_features_; }
std::uint8_t  FakeCommonConfig::status() const          { return status_; }
std::uint32_t FakeCommonConfig::status_writes() const   { return history_count_; }
std::uint16_t FakeCommonConfig::config_vector() const   { return config_vector_; }

std::uint8_t FakeCommonConfig::status_written(std::uint32_t index) const
{
    return index < history_capacity ? history_[index] : 0u;
}

const FakeCommonConfig::Queue &FakeCommonConfig::queue(std::uint16_t index) const
{
    return queues_[index];
}

}  // namespace av
