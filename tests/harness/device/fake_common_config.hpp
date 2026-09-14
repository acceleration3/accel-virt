/* The common configuration structure of a fake virtio-pci device (virtio 1.2
 * 4.1.4.3), with knobs for every way a device can refuse or misbehave. An
 * access at an offset or width the spec does not define fails the test. */
#ifndef AV_FAKE_COMMON_CONFIG_HPP
#define AV_FAKE_COMMON_CONFIG_HPP

#include "core/platform/register_window.hpp"

#include <cstdint>

namespace av {

class FakeCommonConfig final : public RegisterWindow {
public:
    static constexpr std::uint16_t max_queues = 66u;   /* virtio-gpu-rvk-pci's */
    static constexpr std::uint32_t history_capacity = 16u;

    struct Queue {
        std::uint16_t max_size;
        std::uint16_t size;
        std::uint16_t vector;
        std::uint16_t enable;
        std::uint16_t notify_off;
        std::uint64_t desc;
        std::uint64_t driver;
        std::uint64_t device;
    };

    FakeCommonConfig();

    std::uint64_t length() const override;
    std::uint8_t  read_u8(std::uint64_t offset) override;
    std::uint16_t read_u16(std::uint64_t offset) override;
    std::uint32_t read_u32(std::uint64_t offset) override;
    void          write_u8(std::uint64_t offset, std::uint8_t value) override;
    void          write_u16(std::uint64_t offset, std::uint16_t value) override;
    void          write_u32(std::uint64_t offset, std::uint32_t value) override;
    void          write_u64(std::uint64_t offset, std::uint64_t value) override;

    /* Knobs. */
    void offer_features(std::uint64_t features);
    void set_queues(std::uint16_t count, std::uint16_t max_size);
    void set_queue_max_size(std::uint16_t index, std::uint16_t max_size);
    void set_queue_notify_off(std::uint16_t index, std::uint16_t notify_off);
    void refuse_features_ok();
    void delay_reset(std::uint32_t reads);
    void refuse_vectors();
    void refuse_queue_enable();
    void need_reset();
    /* Every status the driver writes from now, after the register holds it: a
     * device behind the function that follows the driver's status. Null stops it. */
    using StatusSink = void (*)(void *context, std::uint8_t status);
    void on_status(StatusSink sink, void *context);

    /* Observations. */
    std::uint64_t driver_features() const;
    std::uint8_t  status() const;
    std::uint8_t  status_written(std::uint32_t index) const;
    std::uint32_t status_writes() const;
    std::uint16_t config_vector() const;
    const Queue  &queue(std::uint16_t index) const;

private:
    void           unexpected(const char *access, std::uint64_t offset);
    Queue         *selected();

    std::uint64_t offered_;
    std::uint32_t device_feature_select_;
    std::uint32_t driver_feature_select_;
    std::uint64_t driver_features_;
    std::uint16_t config_vector_;
    std::uint16_t queue_count_;
    std::uint8_t  status_;
    std::uint8_t  history_[history_capacity];
    std::uint32_t history_count_;
    std::uint16_t queue_select_;
    Queue         queues_[max_queues];
    Queue         absent_;
    std::uint32_t reset_reads_;
    bool          refuse_features_ok_;
    bool          refuse_vectors_;
    bool          refuse_queue_enable_;
    bool          needs_reset_;
    StatusSink    status_sink_;
    void         *status_context_;
};

}  // namespace av

#endif  // AV_FAKE_COMMON_CONFIG_HPP
