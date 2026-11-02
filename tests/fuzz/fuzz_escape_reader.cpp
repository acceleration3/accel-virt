#include "guest/inspect/escape_reader.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t answer_bytes = 64u;

class FuzzedDriver final : public av::AdapterEscapes {
public:
    FuzzedDriver(const std::uint8_t *data, std::size_t size) : data_(data), size_(size), used_(0u) {}

    std::uint32_t answer(std::uint8_t *escape, std::uint32_t length) override
    {
        if (used_ >= size_) return 0xC0000001u;
        const std::uint8_t selector = data_[used_++];
        for (std::uint32_t byte = 0u; byte < answer_bytes && byte < length && used_ < size_; byte++)
            escape[byte] = data_[used_++];
        return (selector & 0x80u) != 0u ? 0xC000000Du : 0u;
    }

private:
    const std::uint8_t *data_;
    std::size_t         size_;
    std::size_t         used_;
};

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::MockPlatform platform;
    {
        av::MockKmt  kmt;
        FuzzedDriver driver(data + 1u, size - 1u);
        kmt.add_adapter(av::PciIdentity(av::PciIdentity::virtio_vendor, av::AvgpuAdapters::virtio_gpu_device,
                                        av::PciIdentity::virtio_vendor, av::AvgpuAdapters::av_subsystem),
                        1u);
        kmt.answer_adapter_escapes(&driver);
        av::EscapeReader reader(kmt, platform);
        const av::TraceEscapeRecord::Kind kind =
            (data[0] & 1u) != 0u ? av::TraceEscapeRecord::Kind::state : av::TraceEscapeRecord::Kind::table;
        const av::Status status = reader.read(kind);
        if (!reader.invariants_hold()) __builtin_trap();
        if (status != av::Status::ok && (reader.length() != 0u || reader.bytes() != nullptr)) __builtin_trap();
        if (reader.length() > av::EscapeReader::max_total) __builtin_trap();
        std::uint64_t sum = 0u;
        for (std::size_t byte = 0u; byte < reader.length(); byte += 4096u) sum += reader.bytes()[byte];
        static_cast<void>(sum);
    }
    if (platform.live_allocations() != 0u || platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
